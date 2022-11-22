#include <vector>

void convert_with_components(size_t width, size_t height, int input_components, int output_components, float* input_pixels, float* output_pixels)
{
	if (input_components == output_components)
	{// components are equal, simply copy one array to the other
		for (size_t y = 0; y < height; y++)
		{
			for (size_t x = 0; x < width; x++)
			{
				for (size_t c = 0; c < input_components; c++)
				{
					output_pixels[((height - y - 1) * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
				}
			}
		}
	}
	else if (input_components < output_components)
	{// buffer is less than output (for example depth aov (1 channel) written to bmp-file (3 channels))
		for (size_t y = 0; y < height; y++)
		{
			for (size_t x = 0; x < width; x++)
			{
				output_pixels[((height - y - 1) * width + x) * output_components] = input_pixels[y * width + x];
				output_pixels[((height - y - 1) * width + x) * output_components + 1] = input_pixels[y * width + x];
				output_pixels[((height - y - 1) * width + x) * output_components + 2] = input_pixels[y * width + x];
				if (output_components == 4)
				{
					output_pixels[((height - y - 1) * width + x) * output_components + 3] = 1.0f;
				}

				for (size_t c = 0; c < input_components; c++)
				{
					output_pixels[((height - y - 1) * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
				}
			}
		}
	}
	else
	{// buffer is greater than output (for example Main (4 channels) written to bmp (3 channels))
		for (size_t y = 0; y < height; y++)
		{
			for (size_t x = 0; x < width; x++)
			{
				for (size_t c = 0; c < output_components; c++)
				{
					output_pixels[((height - y - 1) * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
				}
			}
		}
	}
}