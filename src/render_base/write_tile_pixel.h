#pragma once
#include <vector>

void write_int8_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components);
void write_int16_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components);
void write_float32_pixel(void* ptr, size_t index, const std::vector<float>& in_pixels, bool is_srgb, int components);