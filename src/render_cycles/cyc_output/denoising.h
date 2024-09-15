#pragma once
#include "output_context.h";
#include "../../render_base/render_visual_buffer.h"

enum DenoiseMode
{
	DISABLE,
	OIDN,
	OPTIX
};

// convert ui values to enum
DenoiseMode denoise_mode_enum(int mode_enum);

std::vector<float> get_pixels_from_passes(OutputContext* output_context, ccl::PassType pass_type);

void denoise_outputs(OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal);
void denoise_visual(RenderVisualBuffer* visual_buffer, OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal);

// denoising_oidn
std::vector<float> denoise_buffer_oidn(ImageBuffer* buffer, OutputContext* output_context, bool use_albedo, bool use_normal);

// denoising_optix
std::vector<float> denoise_buffer_optix(ImageBuffer* buffer, OutputContext* output_context, bool use_albedo, bool use_normal);