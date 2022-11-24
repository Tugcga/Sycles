#pragma once
#include <xsi_string.h>
#include <xsi_parameter.h>

#include <vector>

#include "../render_cycles/cyc_output/output_context.h"

// write_image
//clamp value betwen given min and max values
float clamp_float(float value, float min, float max);

// write all outputs to image files
void write_outputs(OutputContext* output_context, const XSI::CParameterRefArray &render_parameters);

// pixel_process
// convert pixels with one number of components to the other
// used, when we should save buffer with one number of components nto image with another ones
// also this method flips the image
void convert_with_components(size_t width, size_t height, int input_components, int output_components, float* input_pixels, float* output_pixels);