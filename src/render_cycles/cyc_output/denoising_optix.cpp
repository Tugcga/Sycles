#include "denoising.h"

#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            log_warning("[CUDA error] " + XSI::CString(std::string(cudaGetErrorString(err)).c_str())); \
            goto cleanup; \
        } \
    } while(0)

#define OPTIX_CHECK(call) \
    do { \
        OptixResult res = call; \
        if (res != OPTIX_SUCCESS) { \
            log_warning("[OptiX error] " + XSI::CString((std::to_string(res) + " " + optixGetErrorName(res)).c_str())); \
            goto cleanup; \
        } \
    } while(0)

bool has_device()
{
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count))
    {
        log_warning("[OptiX error]: Failed to get device information");
        return false;
    }

    if (device_count == 0) {
        log_warning("[OptiX error]: No Nvidia GPUs found");
        return false;
    }

    return true;
}

inline void image_convert_format(const float* in_ptr, uint8_t in_size,
    float* out_ptr, uint8_t out_size,
    unsigned int width, unsigned int height) {
    for (unsigned int i = 0; i < width * height; ++i) {
        const float* src = in_ptr + i * in_size;
        float* dst = out_ptr + i * out_size;

        uint8_t copy_count = std::min(in_size, out_size);
        memcpy(dst, src, copy_count * sizeof(float));

        if (in_size < out_size) {
            for (uint8_t c = in_size; c < out_size; ++c) {
                dst[c] = (c == 3) ? 1.0f : 0.0f;  // only alpha set 1.0, other set 0.0
            }
        }
    }
}

std::vector<float> denoise_buffer_optix(ImageBuffer* buffer, OutputContext* output_context, bool use_albedo, bool use_normal)
{
    OptixDenoiser optix_denoiser = nullptr;
    OptixDeviceContext optix_context = nullptr;
    cudaStream_t cuda_stream = nullptr;
    void* denoiser_state_buffer = nullptr;
    void* denoiser_scratch_buffer = nullptr;
    float* hdr_intensity_gpu = nullptr;
    OptixDenoiserLayer layers[1];
    memset(layers, 0, sizeof(layers));
    OptixDenoiserGuideLayer guide_layer = {};
    memset(&guide_layer, 0, sizeof(guide_layer));

    // Initialize our optix context
    CUcontext cuCtx = 0; // Zero means take the current context

	size_t width = buffer->get_width();
	size_t height = buffer->get_height();
	size_t channels = buffer->get_channels();

    unsigned int buffer_size = 4 * width * height;
    std::vector<float> host_scratch(buffer_size, 0.0f);

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
            use_normal = false;
        }
    }

    if (!has_device())
    {
        return beauty_pixels;
    }

    OptixResult result = optixInit();
    if (result != OPTIX_SUCCESS)
    {
        log_warning("[OptiX error]: Cannot initialize OptiX library (" + XSI::CString(result) + ")");

        return beauty_pixels;
    }

    // Set the denoiser options
    OptixDenoiserOptions denoiser_options = {};
    denoiser_options.guideAlbedo = use_albedo;
    denoiser_options.guideNormal = use_normal;
    denoiser_options.denoiseAlpha = OptixDenoiserAlphaMode(0);

    OptixDenoiserParams denoiser_params = {};
    denoiser_params.blendFactor = 0.0f;

    OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_HDR;

    // Select the first gpu
    CUDA_CHECK(cudaSetDevice(0));

    // The runtime API lazily initializes its CUDA context on first usage
    // Calling cudaFree here forces our context to initialize
    CUDA_CHECK(cudaFree(0));

    // Create a stream to run the denoiser on
    CUDA_CHECK(cudaStreamCreate(&cuda_stream));

    result = optixDeviceContextCreate(cuCtx, nullptr, &optix_context);
    if (result != OPTIX_SUCCESS)
    {
        log_warning("[OptiX error]: Could not create OptiX context: (" + XSI::CString(result) + "%d) " + XSI::CString(optixGetErrorName(result)) + "%s");

        return beauty_pixels;
    }

    // Iniitalize the OptiX denoiser
    OPTIX_CHECK(optixDenoiserCreate(optix_context, model, &denoiser_options, &optix_denoiser));

    // Compute memory needed for the denoiser to exist on the GPU
    OptixDenoiserSizes denoiser_sizes;
    memset(&denoiser_sizes, 0, sizeof(OptixDenoiserSizes));
    OPTIX_CHECK(optixDenoiserComputeMemoryResources(optix_denoiser, width, height, &denoiser_sizes));
    // Allocate this space on the GPu
    CUDA_CHECK(cudaMalloc(&denoiser_state_buffer, denoiser_sizes.stateSizeInBytes));
    CUDA_CHECK(cudaMalloc(&denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes));
    // Setup the denoiser
    OPTIX_CHECK(optixDenoiserSetup(optix_denoiser, cuda_stream,
        width, height,
        (CUdeviceptr)denoiser_state_buffer, denoiser_sizes.stateSizeInBytes,
        (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes));

    // Set the denoiser parameters
    CUDA_CHECK(cudaMalloc(&hdr_intensity_gpu, sizeof(float)));
    denoiser_params.hdrIntensity = (CUdeviceptr)hdr_intensity_gpu;

    memset(&layers[0], 0, sizeof(OptixDenoiserLayer));

    // Allocate memory for all our layers on the GPU
    for (auto& l : layers)
    {
        // Input
        CUDA_CHECK(cudaMalloc(((void**)&(l.input.data)), sizeof(float) * buffer_size));
        l.input.width = width;
        l.input.height = height;
        l.input.rowStrideInBytes = width * sizeof(float) * 4;
        l.input.pixelStrideInBytes = sizeof(float) * 4;
        l.input.format = OPTIX_PIXEL_FORMAT_FLOAT4;

        // Output
        CUDA_CHECK(cudaMalloc(((void**)&(l.output.data)), sizeof(float) * buffer_size));
        l.output.width = width;
        l.output.height = height;
        l.output.rowStrideInBytes = width * sizeof(float) * 4;
        l.output.pixelStrideInBytes = sizeof(float) * 4;
        l.output.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    // albedo
    if (use_albedo)
    {
        CUDA_CHECK(cudaMalloc(((void**)&guide_layer.albedo.data), sizeof(float)* buffer_size));
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
        CUDA_CHECK(cudaMalloc(((void**)&guide_layer.normal.data), sizeof(float) * buffer_size));
        // guide_layer.normal.data               = (CUdeviceptr)normal_buffer;
        guide_layer.normal.width = width;
        guide_layer.normal.height = height;
        guide_layer.normal.rowStrideInBytes = width * sizeof(float) * 4;
        guide_layer.normal.pixelStrideInBytes = sizeof(float) * 4;
        guide_layer.normal.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    // Copy our beauty image data to the GPU
    // Convert image to float4 to use with the denoiser
    image_convert_format(&beauty_pixels[0], channels, &host_scratch[0], 4, width, height);
    // Copy our data to the GPU
    // First layer must always be beauty AOV
    CUDA_CHECK(cudaMemcpy((void*)layers[0].input.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice));

    if (use_albedo)
    {
        // Copy albedo image data to the GPU
        memset(&host_scratch[0], 0, sizeof(float)* buffer_size);
        // Convert image to float4 to use with the denoiser
        image_convert_format(&albedo_pixels[0], 3, &host_scratch[0], 4, width, height);
        // Copy our data to the GPU
        CUDA_CHECK(cudaMemcpy((void*)guide_layer.albedo.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice));
    }

    if (use_normal)
    {
        // Copy normal image data to the GPU
        memset(&host_scratch[0], 0, sizeof(float) * buffer_size);
        // Convert image to float4 to use with the denoiser
        image_convert_format(&normal_pixels[0], 3, &host_scratch[0], 4, width, height);
        // Copy our data to the GPU
        CUDA_CHECK(cudaMemcpy((void*)guide_layer.normal.data, &host_scratch[0], sizeof(float) * buffer_size, cudaMemcpyHostToDevice));
    }

    // Execute dnoising
    // Compute the intensity of the input image
    OPTIX_CHECK(optixDenoiserComputeIntensity(optix_denoiser, cuda_stream, &layers[0].input, denoiser_params.hdrIntensity, (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes));

    // Execute the denoiser
    OPTIX_CHECK(optixDenoiserInvoke(optix_denoiser, cuda_stream, &denoiser_params,
        (CUdeviceptr)denoiser_state_buffer, denoiser_sizes.stateSizeInBytes,
        &guide_layer, &layers[0], 1, 0, 0,
        (CUdeviceptr)denoiser_scratch_buffer, denoiser_sizes.withoutOverlapScratchSizeInBytes));

    // Copy denoised images back to the CPU
    CUDA_CHECK(cudaMemcpy(&host_scratch[0], (void*)layers[0].output.data, sizeof(float) * buffer_size, cudaMemcpyDeviceToHost));
    image_convert_format(&host_scratch[0], 4, &beauty_pixels[0], channels, width, height);

cleanup:
    if (layers[0].input.data) { cudaFree((void*)layers[0].input.data); }
    if (layers[0].previousOutput.data) { cudaFree((void*)layers[0].previousOutput.data); }
    if (layers[0].output.data) { cudaFree((void*)layers[0].output.data); }
    if (guide_layer.albedo.data) { cudaFree((void*)guide_layer.albedo.data); }
    if (guide_layer.normal.data) { cudaFree((void*)guide_layer.normal.data); }
    if (guide_layer.flow.data) { cudaFree((void*)guide_layer.flow.data); }
    if (hdr_intensity_gpu) { cudaFree(hdr_intensity_gpu); }
    if (denoiser_state_buffer) { cudaFree(denoiser_state_buffer); }
    if (denoiser_scratch_buffer) { cudaFree(denoiser_scratch_buffer); }
    if (optix_denoiser) { optixDenoiserDestroy(optix_denoiser); }
    if (optix_context) { optixDeviceContextDestroy(optix_context); }
    if (cuda_stream) { cudaStreamDestroy(cuda_stream); }

	return beauty_pixels;
}