#pragma once
#include <string>

bool write_output_exr(size_t width, size_t height, size_t components, const std::string& file_path, float* pixels);
bool load_input_exr(const std::string& input_filepath, std::vector<float>& out_pixels, int& out_width, int& out_height, int& out_channels);