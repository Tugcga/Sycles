#include <string>

#include "logs.h"

#define TINYEXR_IMPLEMENTATION
#include "../utilities/tinyexr.h"

bool write_output_exr(size_t width, size_t height, size_t components, const std::string& file_path, float* pixels)
{
	// we should flip vertical coordinates of pixels
	std::vector<float> flip_pixels(width * height * components, 0.0f);
	LONG row = 0;
	LONG column = 0;
	LONG flip_pixel = 0;
	for (LONG pixel = 0; pixel < width * height; pixel++)
	{
		row = pixel / width;
		column = pixel - row * width;
		flip_pixel = (height - row - 1) * width + column;
		// copy all input components
		for (LONG c = 0; c < components; c++)
		{
			flip_pixels[components * flip_pixel + c] = pixels[components * pixel + c];
		}
	}

	const char* err;
	int out = SaveEXR(pixels, width, height, components, 0, file_path.c_str(), &err);
	flip_pixels.clear();
	flip_pixels.shrink_to_fit();
	return out == 0;
}

bool load_input_exr(const std::string &input_filepath, std::vector<float> &out_pixels, int &out_width, int &out_height, int &out_channels)
{
	float* out;
	int width;
	int height;
	const char* err = NULL;

	int ret = LoadEXR(&out, &width, &height, input_filepath.c_str(), &err);

	if (ret != TINYEXR_SUCCESS) 
	{
		if (err) 
		{
			log_warning("Fails to load exr file from the path " + XSI::CString(input_filepath.c_str()));
			log_warning("Error: " + XSI::CString(err));
			FreeEXRErrorMessage(err); // release memory of error message.
		}

		return false;
	}
	else 
	{
		size_t pixels_count = width * height;
		out_width = width;
		out_height = height;
		out_channels = 4;
		out_pixels.resize(pixels_count * out_channels);
		for (size_t p = 0; p < pixels_count; p++)
		{
			for (size_t c = 0; c < out_channels; c++)
			{
				out_pixels[out_channels * p + c] = out[4 * p + c];
			}
		}

		free(out); // release memory of image data

		return true;
	}
}