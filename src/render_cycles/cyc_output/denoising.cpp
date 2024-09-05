#include "OpenImageDenoise/oidn.hpp"

#include "denoising.h"

DenoiseMode denoise_mode_enum(int mode_enum)
{
	if (mode_enum == 1) { return DenoiseMode::OIDN;  }
	else if (mode_enum == 2) { return DenoiseMode::OPTIX; }
	else { return DenoiseMode::DISABLE; }
}

oidn::Format components_to_format(ULONG components)
{
	if (components == 1) { return oidn::Format::Float; }
	else if (components == 2) { return oidn::Format::Float2; }
	else if (components == 3) { return oidn::Format::Float3; }
	else if (components == 4) { return oidn::Format::Float4; }
	else { return oidn::Format::Undefined; }
}

void log_oidn_error(const char* message)
{
	log_message("[OIDN error]: " + XSI::CString(message), XSI::siWarningMsg);
}

void error_callback(void* user_ptr, oidn::Error error, const char* message)
{
	log_oidn_error(message);
}

std::vector<float> get_pixels_from_passes(OutputContext* output_context, ccl::PassType pass_type)
{
	std::vector<float> to_return(0);

	for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
	{
		ccl::PassType type = output_context->get_output_pass_type(i);
		if (type == pass_type)
		{
			ImageBuffer* buffer = output_context->get_output_buffer(i);
			return buffer->convert_channel_pixels(3);
		}
	}

	return to_return;
}

std::vector<float> get_denoised_pixels(ImageBuffer *buffer, OutputContext* output_context, bool use_albedo, bool use_normal)
{
	size_t width = buffer->get_width();
	size_t height = buffer->get_height();

	oidn::DeviceType device_type = oidn::DeviceType::CPU;

	oidn::DeviceRef device = oidn::newDevice(device_type);
	const char* error_message;
	if (device.getError(error_message) != oidn::Error::None)
	{
		log_oidn_error(error_message);
	}

	device.set("setAffinity", false);
	device.commit();

	oidn::FilterRef filter = device.newFilter("RT");
	// convert to 3 channels per pixel (4-channels are not supported)
	std::vector<float> original_pixels = buffer->convert_channel_pixels(3);
	filter.setImage("color", original_pixels.data(), oidn::Format::Float3, width, height);

	std::vector<float> albedo_pixels;
	std::vector<float> normal_pixels;

	// next additional channels
	if (use_albedo)
	{
		// we should find albedo pass in output context and get pixels from this pass
		albedo_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_ALBEDO);
		if (albedo_pixels.size() == width * height * ULONG(3))
		{
			filter.setImage("albedo", albedo_pixels.data(), oidn::Format::Float3, width, height);
		}
	}

	// the same for normal
	if (use_normal)
	{
		normal_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_NORMAL);
		if (normal_pixels.size() == width * height * ULONG(3))
		{
			filter.setImage("normal", normal_pixels.data(), oidn::Format::Float3, width, height);
		}
	}

	// denoised pixels also contains only 3 channels
	std::vector<float> denoised_pixels(width * height * ULONG(3));
	filter.setImage("output", denoised_pixels.data(), oidn::Format::Float3, width, height);
	filter.set("hdr", true);
	filter.set("srgb", false);
	filter.set("cleanAux", true);

	filter.commit();
	filter.execute();

	albedo_pixels.clear();
	normal_pixels.clear();
	albedo_pixels.shrink_to_fit();
	normal_pixels.shrink_to_fit();

	return denoised_pixels;
}

void denoise_visual(RenderVisualBuffer* visual_buffer, OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal)
{
	if (denoise_mode == DenoiseMode::OIDN && visual_buffer->get_pass_type() == ccl::PassType::PASS_COMBINED)
	{
		std::vector<float> denoised_pixels = get_denoised_pixels(visual_buffer->get_buffer(), output_context, use_albedo, use_normal);

		// rewrite buffer rgb-channels from denoised array
		visual_buffer->redefine_rgb(denoised_pixels);
	}
}

void denoise_outputs(OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal)
{
	if (denoise_mode == DenoiseMode::OIDN)
	{
		size_t passes_count = output_context->get_output_passes_count();
		for (size_t i = 0; i < passes_count; i++)
		{
			ccl::PassType pass_type = output_context->get_output_pass_type(i);
			if (pass_type == ccl::PASS_COMBINED)  // denoise only combined passes
			{
				ImageBuffer* pass_buffer = output_context->get_output_buffer(i);
				std::vector<float> denoised_pixels = get_denoised_pixels(pass_buffer, output_context, use_albedo, use_normal);

				pass_buffer->redefine_rgb(denoised_pixels);
			}
		}
	}
}