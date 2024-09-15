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

void denoise_visual(RenderVisualBuffer* visual_buffer, OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal)
{
	if (visual_buffer->get_pass_type() == ccl::PassType::PASS_COMBINED)
	{
		if (denoise_mode == DenoiseMode::OIDN)
		{
			std::vector<float> denoised_pixels = denoise_buffer_oidn(visual_buffer->get_buffer(), output_context, use_albedo, use_normal);
			// rewrite buffer rgb-channels from denoised array
			visual_buffer->redefine_rgb(denoised_pixels);
		}
		else if (denoise_mode == DenoiseMode::OPTIX)
		{
			std::vector<float> denoised_pixels = denoise_buffer_optix(visual_buffer->get_buffer(), output_context, use_albedo, use_normal);
			visual_buffer->set_pixels(denoised_pixels);
		}
	}
}

void denoise_outputs(OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal)
{
	size_t passes_count = output_context->get_output_passes_count();
	for (size_t i = 0; i < passes_count; i++)
	{
		ccl::PassType pass_type = output_context->get_output_pass_type(i);
		if (pass_type == ccl::PASS_COMBINED)  // denoise only combined passes
		{
			ImageBuffer* pass_buffer = output_context->get_output_buffer(i);
			if (denoise_mode == DenoiseMode::OIDN)
			{
				std::vector<float> denoised_pixels = denoise_buffer_oidn(pass_buffer, output_context, use_albedo, use_normal);
				pass_buffer->redefine_rgb(denoised_pixels);
			}
			else if (denoise_mode == DenoiseMode::OPTIX)
			{
				std::vector<float> denoised_pixels = denoise_buffer_optix(pass_buffer, output_context, use_albedo, use_normal);
				pass_buffer->set_pixels(ImageRectangle(0, pass_buffer->get_width(), 0, pass_buffer->get_height()), denoised_pixels);
			}		
		}
	}
}