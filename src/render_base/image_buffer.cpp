#include "image_buffer.h"

#include "../utilities/logs.h"

ImageRectangle::ImageRectangle(size_t in_x_start, size_t in_x_end, size_t in_y_start, size_t in_y_end)
{
	x_start = in_x_start;
	x_end = in_x_end;
	y_start = in_y_start;
	y_end = in_y_end;

	width = x_end - x_start;
	height = y_end - y_start;
}

ImageRectangle::~ImageRectangle() { }

size_t ImageRectangle::get_width() const { return width; }
size_t ImageRectangle::get_height() const { return height; }
size_t ImageRectangle::get_x_start() const { return x_start; }
size_t ImageRectangle::get_x_end() const { return x_end; }
size_t ImageRectangle::get_y_start() const { return y_start; }
size_t ImageRectangle::get_y_end() const { return y_end; }

//----------------------------------------------------
//----------------------------------------------------

ImageBuffer::ImageBuffer()
{
	reset();
}

ImageBuffer::ImageBuffer(size_t in_width, size_t in_height, size_t in_channels)
{
	recreate(in_width, in_height, in_channels);
}

ImageBuffer::~ImageBuffer()
{
	pixels.clear();
	pixels.shrink_to_fit();
}

void ImageBuffer::reset()
{
	width = 0;
	height = 0;
	channels = 0;

	pixels_count = 0;
	pixels.resize(0);
}

void ImageBuffer::recreate(size_t in_width, size_t in_height, size_t in_channels)
{
	width = in_width;
	height = in_height;
	channels = in_channels;

	pixels_count = width * height;

	pixels.resize(pixels_count * channels, 0.0f);
}

size_t ImageBuffer::get_width()
{
	return width;
}

size_t ImageBuffer::get_height()
{
	return height;
}

size_t ImageBuffer::get_channels()
{
	return channels;
}

bool ImageBuffer::get_pixels(const ImageRectangle& rect, float* out_pixels)
{
	for (size_t y = rect.get_y_start(); y < rect.get_y_end(); y++)
	{
		for (size_t x = rect.get_x_start(); x < rect.get_x_end(); x++)
		{
			size_t  p = y * width + x;
			for (size_t c = 0; c < channels; c++)
			{
				*out_pixels = pixels[channels * p + c];
				out_pixels++;
			}
		}
	}
	return true;
}

bool ImageBuffer::set_pixels(const ImageRectangle& rect, const std::vector<float> &in_pixels)
{
	size_t iterator = 0;
	for (size_t y = rect.get_y_start(); y < rect.get_y_end(); y++)
	{
		for (size_t x = rect.get_x_start(); x < rect.get_x_end(); x++)
		{
			size_t  p = y * width + x;
			for (size_t c = 0; c < channels; c++)
			{
				pixels[channels * p + c] = in_pixels[iterator];
				iterator++;
			}
		}
	}

	return true;
}

bool ImageBuffer::set_pixel(size_t x, size_t y, const float* data, size_t in_channels)
{
	size_t p = y * width + x;
	for (size_t c = 0; c < std::min(in_channels, channels); c++)
	{
		pixels[channels * p + c] = *data;
		data++;
	}
	return true;
}

std::vector<float> ImageBuffer::convert_channel_pixels(size_t in_channels)
{
	std::vector<float> to_return(width * height * in_channels, 0.0f);

	for (size_t y = 0; y < height; y++)
	{
		for (size_t x = 0; x < width; x++)
		{
			size_t channels_min = std::min(channels, in_channels);
			size_t p = y * width + x;
			// copy alowed channels
			for (size_t c = 0; c < channels_min; c++)
			{
				to_return[p * in_channels + c] = pixels[p * channels + c];
			}
			// if original buffer contains more channels, nothing to do
			// for other case, we convert from 3 channels to 4, for example (or from 1 to 3)
			if (in_channels > channels)
			{
				// for original 1-channel image copy it to other channels
				// the first we already set
				if (channels == 1)
				{
					for (size_t c = 1; c < in_channels; c++)
					{
						to_return[p * in_channels + c] = pixels[p * channels];
					}
				}
			}
		}
	}

	return to_return;
}

void ImageBuffer::redefine_rgb(const std::vector<float>& rgb_pixels)
{
	for (size_t y = 0; y < height; y++)
	{
		for (size_t x = 0; x < width; x++)
		{
			size_t p = y * width + x;
			for (size_t c = 0; c < std::min(channels, (size_t)3); c++)
			{
				pixels[p * channels + c] = rgb_pixels[p * 3 + c];
			}
		}
	}
}

size_t ImageBuffer::get_pixels_count() { return pixels_count; }
size_t ImageBuffer::get_channels_count() { return channels; }
size_t ImageBuffer::get_buffer_size() { return pixels.size(); }
size_t ImageBuffer::get_pixel_index(size_t x, size_t y) { return y * width + x; }
std::vector<float> ImageBuffer::get_pixels() { return pixels; }
float* ImageBuffer::get_pixels_pointer() { return &pixels[0]; }