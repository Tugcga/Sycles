#include <vector>

void convert_with_components(size_t width, size_t height, int input_components, int output_components, float* input_pixels, float* output_pixels)
{
	if (input_components == output_components)
	{// components are equal, simply copy one array to the other
		for (size_t y = 0; y < height; y++)
		{
			size_t flip_y = height - y - 1;
			for (size_t x = 0; x < width; x++)
			{
				for (size_t c = 0; c < input_components; c++)
				{
					output_pixels[(flip_y * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
				}
			}
		}
	}
	else if (input_components < output_components)
	{// buffer is less than output (for example depth aov (1 channel) written to bmp-file (3 channels))
		for (size_t y = 0; y < height; y++)
		{
			size_t flip_y = height - y - 1;
			for (size_t x = 0; x < width; x++)
			{
				output_pixels[(flip_y * width + x) * output_components] = input_pixels[y * width + x];
				output_pixels[(flip_y * width + x) * output_components + 1] = input_pixels[y * width + x];
				output_pixels[(flip_y * width + x) * output_components + 2] = input_pixels[y * width + x];
				if (output_components == 4)
				{
					output_pixels[(flip_y * width + x) * output_components + 3] = 1.0f;  // always fill the alpha
				}

				for (size_t c = 0; c < input_components; c++)
				{
					output_pixels[(flip_y * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
				}
			}
		}
	}
	else
	{// buffer is greater than output (for example Main (4 channels) written to bmp (3 channels), or 3-channels to 1-channel)
		// the approach is the following:
		// 4->1 - copy only the alpha
		// 4->3 - ignore alpha
		// 3->1 - get average color
		// oher situations are impossible (because Cycles return only 1, 3 or 4 channels and otuput images supports 1, 3 or 4 channels)
		for (size_t y = 0; y < height; y++)
		{
			size_t flip_y = height - y - 1;
			for (size_t x = 0; x < width; x++)
			{
				if (input_components == 4 && output_components == 1)
				{
					output_pixels[flip_y * width + x] = input_pixels[(y * width + x) * input_components + 3];
				}
				else if (input_components == 3 && output_components == 1)
				{
					output_pixels[flip_y * width + x] = (input_pixels[(y * width + x) * input_components] + 
														 input_pixels[(y * width + x) * input_components] + 
														 input_pixels[(y * width + x) * input_components]) / 0.3f;
				}
				else
				{
					for (size_t c = 0; c < output_components; c++)
					{
						output_pixels[(flip_y * width + x) * output_components + c] = input_pixels[(y * width + x) * input_components + c];
					}
				}
			}
		}
	}
}