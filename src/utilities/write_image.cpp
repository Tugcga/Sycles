#include <vector>
#include <fstream>
#include <xsi_string.h>
#include "logs.h"

float clamp_float(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

bool write_float(const XSI::CString &path, const XSI::CString &ext, const XSI::CString &data_type, int width, int height, int components, const std::vector<float> &pixels)
{
	return true;
}