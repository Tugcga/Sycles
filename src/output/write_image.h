#pragma once
#include <xsi_string.h>

#include <vector>

//clamp value betwen given min and max values
float clamp_float(float value, float min, float max);

//write to the disc image file
bool write_float(const XSI::CString &path, const XSI::CString &ext, const XSI::CString &data_type, int width, int height, int components, const std::vector<float> &pixels);