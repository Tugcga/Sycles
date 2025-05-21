#include "../utilities/math.h"

uint8_t float_to_int8(float value, bool is_srgb)
{
	if (is_srgb) { return linear_to_srgb_int8(value); }
	else { return linear_clamp_int8(value); }
}

uint16_t float_to_int16(float value, bool is_srgb)
{
	if (is_srgb) { return linear_to_srgb_int16(value); }
	else { return linear_clamp_int16(value); }
}

float float_to_float(float value, bool is_srgb)
{
	if (is_srgb) { return linear_to_srgb_float(value); }
	else { return value; }
}

void write_int8_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components)
{
	uint8_t* pixel = (uint8_t*)ptr;
	pixel[3] = components == 4 ? static_cast<uint8_t>(in_pixels[4 * index + 3] * 255.0) : static_cast<uint8_t>(255);
	if (components == 4 || components == 3)
	{
		pixel[0] = float_to_int8(in_pixels[components * index], is_srgb);
		pixel[1] = float_to_int8(in_pixels[components * index + 1], is_srgb);
		pixel[2] = float_to_int8(in_pixels[components * index + 2], is_srgb);
	}
	else if (components == 1)
	{
		uint8_t v = float_to_int8(in_pixels[components * index], is_srgb);
		pixel[0] = v;
		pixel[1] = v;
		pixel[2] = v;
	}
}

void write_int16_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components)
{
	uint16_t* pixel = (uint16_t*)ptr;
	pixel[3] = components == 4 ? static_cast<uint16_t>(in_pixels[4 * index + 3] * 65535.0) : static_cast<uint16_t>(65535);
	if (components == 4 || components == 3)
	{
		pixel[0] = float_to_int16(in_pixels[components * index], is_srgb);
		pixel[1] = float_to_int16(in_pixels[components * index + 1], is_srgb);
		pixel[2] = float_to_int16(in_pixels[components * index + 2], is_srgb);
	}
	else if (components == 1)
	{
		uint8_t v = float_to_int16(in_pixels[components * index], is_srgb);
		pixel[0] = v;
		pixel[1] = v;
		pixel[2] = v;
	}
}

void write_float32_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components)
{
	float* pixel = (float*)ptr;
	pixel[3] = components == 4 ? (in_pixels[4 * index + 3]) : static_cast<uint8_t>(1.0);
	if (components == 4 || components == 3)
	{
		pixel[0] = float_to_float(in_pixels[components * index], is_srgb);
		pixel[1] = float_to_float(in_pixels[components * index + 1], is_srgb);
		pixel[2] = float_to_float(in_pixels[components * index + 2], is_srgb);
	}
	else if (components == 1)
	{
		float v = float_to_float(in_pixels[components * index], is_srgb);
		pixel[0] = v;
		pixel[1] = v;
		pixel[2] = v;
	}
}