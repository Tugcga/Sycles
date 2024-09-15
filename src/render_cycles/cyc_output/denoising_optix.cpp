#include "denoising.h"

#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>

bool has_device()
{
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count))
    {
        log_message("[OptiX error]: Failed to get device information", XSI::siWarningMsg);
        return false;
    }

    if (device_count == 0) {
        log_message("[OptiX error]: No Nvidia GPUs found", XSI::siWarningMsg);
        return false;
    }

    return true;
}

inline void image_convert_format(float* in_ptr, uint8_t in_size, float* out_ptr, uint8_t out_size, unsigned int width, unsigned int height)
{
    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            memcpy(out_ptr, in_ptr, sizeof(float) * in_size);
            out_ptr += out_size;
            in_ptr += in_size;
        }
    }
}

std::vector<float> denoise_buffer_optix(ImageBuffer* buffer, OutputContext* output_context, bool use_albedo, bool use_normal)
{
	size_t width = buffer->get_width();
	size_t height = buffer->get_height();
	size_t channels = buffer->get_channels();

    // Allocate space for our pixel data
    std::vector<float> beauty_pixels = buffer->get_pixels();

    std::vector<float> albedo_pixels(0);
    if (use_albedo)
    {
        albedo_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_ALBEDO);
        if (albedo_pixels.size() != width * height * 3)
        {
            use_albedo = false;
        }
    }

    std::vector<float> normal_pixels(0);
    if (use_normal)
    {
        normal_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_NORMAL);
        if (normal_pixels.size() != width * height * 3)
        {
            use_albedo = false;
        }
    }

    if (!has_device())
    {
        return beauty_pixels;
    }

    OptixResult result = optixInit();
    if (result != OPTIX_SUCCESS)
    {
        log_message("[OptiX error]: Cannot initialize OptiX library (" + XSI::CString(result) + ")", XSI::siWarningMsg);

        return beauty_pixels;
    }

    // Select the first gpu
    cudaSetDevice(0);

    // The runtime API lazily initializes its CUDA context on first usage
    // Calling cudaFree here forces our context to initialize
    cudaFree(0);

    // Create a stream to run the denoiser on
    cudaStream_t cuda_stream;
    cudaStreamCreate(&cuda_stream);

    // Initialize our optix context
    CUcontext cuCtx = 0; // Zero means take the current context
    OptixDeviceContext optix_context = nullptr;
    result = optixDeviceContextCreate(cuCtx, nullptr, &optix_context);
    if (result != OPTIX_SUCCESS)
    {
        log_message("[OptiX error]: Could not create OptiX context: (" + XSI::CString(result) + "%d) " + XSI::CString(optixGetErrorName(result)) + "%s", XSI::siWarningMsg);

        return beauty_pixels;
    }

    // Set the denoiser options
    OptixDenoiserOptions denoiser_options = {};
    denoiser_options.guideAlbedo = use_albedo;
    denoiser_options.guideNormal = use_normal;

    // Iniitalize the OptiX denoiser
    OptixDenoiser optix_denoiser = nullptr;
    OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_HDR;

    optixDenoiserCreate(optix_context, model, &denoiser_options, &optix_denoiser);

    // Compute memory needed for the denoiser to exist on the GPU
    OptixDenoiserSizes denoiser_sizes;
    memset(&denoiser_sizes, 0, sizeof(OptixDenoiserSizes));
    optixDenoiserComputeMemoryResources(optix_denoiser, width, height, &denoiser_sizes);
    // Allocate this space on the GPu
    void* denoiser_state_buffer = nullptr;
    void* denoiser_scratch_buffer = nullptr;
    cudaMalloc(&denoiser_state_buffer, denoiser_sizes.stateSizeInBytes);
    cudaMalloc(&denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes);
    // Setup the denoiser
    optixDenoiserSetup(optix_denoiser, cuda_stream,
        width, height,
        (CUdeviceptr)denoiser_state_buffer, denoiser_sizes.stateSizeInBytes,
        (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes);

    // Set the denoiser parameters
    OptixDenoiserParams denoiser_params = {};
    denoiser_options.denoiseAlpha = OptixDenoiserAlphaMode(0);
    denoiser_params.blendFactor = 0.0f;
    cudaMalloc((void**)&denoiser_params.hdrIntensity, sizeof(float));

    // Create and set our OptiX layers
    std::vector<OptixDenoiserLayer> layers(1);
    memset(&layers[0], 0, sizeof(OptixDenoiserLayer) * layers.size());

    // Allocate memory for all our layers on the GPU
    for (auto& l : layers)
    {
        // Input
        cudaMalloc(((void**)&(l.input.data)), sizeof(float) * 4 * width * height);
        l.input.width = width;
        l.input.height = height;
        l.input.rowStrideInBytes = width * sizeof(float) * 4;
        l.input.pixelStrideInBytes = sizeof(float) * 4;
        l.input.format = OPTIX_PIXEL_FORMAT_FLOAT4;

        // Output
        cudaMalloc(((void**)&(l.output.data)), sizeof(float) * 4 * width * height);
        l.output.width = width;
        l.output.height = height;
        l.output.rowStrideInBytes = width * sizeof(float) * 4;
        l.output.pixelStrideInBytes = sizeof(float) * 4;
        l.output.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    OptixDenoiserGuideLayer guide_layer = {};
    // albedo
    if (use_albedo)
    {
        cudaMalloc(((void**)&guide_layer.albedo.data), sizeof(float) * 4 * width * height);
        // guide_layer.albedo.data               = (CUdeviceptr)albedo_buffer;
        guide_layer.albedo.width = width;
        guide_layer.albedo.height = height;
        guide_layer.albedo.rowStrideInBytes = width * sizeof(float) * 4;
        guide_layer.albedo.pixelStrideInBytes = sizeof(float) * 4;
        guide_layer.albedo.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    // normal
    if (use_normal)
    {
        cudaMalloc(((void**)&guide_layer.normal.data), sizeof(float) * 4 * width * height);
        // guide_layer.normal.data               = (CUdeviceptr)normal_buffer;
        guide_layer.normal.width = width;
        guide_layer.normal.height = height;
        guide_layer.normal.rowStrideInBytes = width * sizeof(float) * 4;
        guide_layer.normal.pixelStrideInBytes = sizeof(float) * 4;
        guide_layer.normal.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    unsigned int buffer_size = 4 * width * height;
    std::vector<float> host_scratch(buffer_size, 0.0f);

    // Copy our beauty image data to the GPU
    // Convert image to float4 to use with the denoiser
    image_convert_format(&beauty_pixels[0], channels, &host_scratch[0], 4, width, height);
    // Copy our data to the GPU
    // First layer must always be beauty AOV
    cudaMemcpy((void*)layers[0].input.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice);

    if (use_albedo)
    {
        // Copy albedo image data to the GPU
        memset(&host_scratch[0], 0, sizeof(float)* buffer_size);
        // Convert image to float4 to use with the denoiser
        image_convert_format(&albedo_pixels[0], 3, &host_scratch[0], 4, width, height);
        // Copy our data to the GPU
        cudaMemcpy((void*)guide_layer.albedo.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice);
    }

    if (use_normal)
    {
        // Copy normal image data to the GPU
        memset(&host_scratch[0], 0, sizeof(float) * buffer_size);
        // Convert image to float4 to use with the denoiser
        image_convert_format(&normal_pixels[0], 3, &host_scratch[0], 4, width, height);
        // Copy our data to the GPU
        cudaMemcpy((void*)guide_layer.normal.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice);
    }

    // Execute dnoising
    // Compute the intensity of the input image
    optixDenoiserComputeIntensity(optix_denoiser, cuda_stream, &layers[0].input, denoiser_params.hdrIntensity, (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes);

    // Execute the denoiser
    optixDenoiserInvoke(optix_denoiser, cuda_stream, &denoiser_params,
        (CUdeviceptr)denoiser_state_buffer, denoiser_sizes.stateSizeInBytes,
        &guide_layer, &layers[0], layers.size(), 0, 0,
        (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes);

    // Copy denoised images back to the CPU
    cudaMemcpy(&host_scratch[0], (void*)layers[0].output.data, sizeof(float) * buffer_size, cudaMemcpyDeviceToHost);
    image_convert_format(&host_scratch[0], 4, &beauty_pixels[0], channels, width, height);

    // Remove our gpu buffers
    for (auto& l : layers)
    {
        cudaFree((void*)l.input.data);
        cudaFree((void*)l.previousOutput.data);
        cudaFree((void*)l.output.data);
    }
    cudaFree((void*)denoiser_params.hdrIntensity);
    cudaFree((void*)guide_layer.albedo.data);
    cudaFree((void*)guide_layer.normal.data);
    cudaFree((void*)guide_layer.flow.data);
    // Destroy the denoiser
    cudaFree(denoiser_state_buffer);
    cudaFree(denoiser_scratch_buffer);
    optixDenoiserDestroy(optix_denoiser);
    // Destroy the OptiX context
    optixDeviceContextDestroy(optix_context);
    // Delete our CUDA stream as well
    cudaStreamDestroy(cuda_stream);

	return beauty_pixels;
}