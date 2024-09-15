#pragma once
#include <vector>

class ImageRectangle
{
public:
	ImageRectangle(size_t in_x_start, size_t in_x_end, size_t in_y_start, size_t in_y_end);
	~ImageRectangle();

	size_t get_width() const;
	size_t get_height() const;
	size_t get_x_start() const;
	size_t get_x_end() const;
	size_t get_y_start() const;
	size_t get_y_end() const;

private:
	size_t x_start;
	size_t x_end;
	size_t y_start;
	size_t y_end;

	size_t width;
	size_t height;
};

class ImageBuffer
{
public:
	ImageBuffer(size_t in_width, size_t in_height, size_t in_channels);
	ImageBuffer();
	~ImageBuffer();
	void reset();
	void recreate(size_t in_width, size_t in_height, size_t in_channels);

	size_t get_width();
	size_t get_height();
	size_t get_channels();

	bool get_pixels(const ImageRectangle &rect, float* out_pixels);
	bool set_pixels(const ImageRectangle &rect, const std::vector<float>& in_pixels);
	bool set_pixel(size_t x, size_t y, const float* data, size_t in_channels);
	size_t get_pixels_count();
	size_t get_buffer_size();  // return the size of the pixels array (inf fact pixels_count * channels)
	size_t get_pixel_index(size_t x, size_t y);

	std::vector<float> get_pixels();  // create the copy
	std::vector<float> convert_channel_pixels(size_t in_channels);  // convert pixels of the image buffer to input component number, for example, convert from 4 channels to 3 (forget alpha)
	void redefine_rgb(const std::vector<float> &rgb_pixels);  // we assume that the dimension of the rgb pixels is the same as in original image
	float* get_pixels_pointer();

private:
	size_t width;
	size_t height;
	size_t channels;

	size_t pixels_count;

	std::vector<float> pixels;
};
