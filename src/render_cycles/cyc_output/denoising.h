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

void denoise_outputs(OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal);
void denoise_visual(RenderVisualBuffer* visual_buffer, OutputContext* output_context, DenoiseMode denoise_mode, bool use_albedo, bool use_normal);