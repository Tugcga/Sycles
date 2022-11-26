#pragma once
#include <xsi_string.h>
#include <xsi_parameter.h>

#include <vector>

#include "OpenImageIO/imagebuf.h"

#include "../render_cycles/cyc_output/output_context.h"
#include "../render_cycles/cyc_output/color_transform_context.h"

// write_image
//clamp value betwen given min and max values
float clamp_float(float value, float min, float max);

// write all outputs to image files
void write_outputs(OutputContext* output_context, ColorTransformContext* color_transform_context, const XSI::CParameterRefArray &render_parameters);

// pixel_process
// convert pixels with one number of components to the other
// used, when we should save buffer with one number of components nto image with another ones
// also this method allows to flip the image in vertical direction (because some output formats requires this flip)
void convert_with_components(size_t width, size_t height, int input_components, int output_components, bool flip_verticaly, float* input_pixels, float* output_pixels);

// extract channel from the input pixels array and output it
// also allows to flip pixels of the image in vertical direction
void extract_channel(size_t image_width, size_t image_height, size_t channel, size_t components, bool flip_verticaly, float* pixels, float* output);

// combine over_pixels and back_pixels and write to back_pixels
// we assume that all buffers already have the same size and 4 channels
void overlay_pixels(size_t width, size_t height, float* over_pixels, float* back_pixels);

// labels_buffer
void build_labels_buffer(OIIO::ImageBuf& buffer,
	const XSI::CString& text_string,
	size_t image_width, size_t image_height,
	size_t horisontal_shift,
	size_t box_height,
	float back_r, float back_g, float back_b, float back_a,
	size_t bottom_row);