#pragma once
#include <xsi_dataarray.h>

#include "scene/pass.h"

#include <vector>

#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"

#include "../utilities/logs.h"

class RenderVisualBuffer
{
public:
	RenderVisualBuffer() 
	{ 
		buffer = new ccl::ImageBuf(); 
		buffer_pixels.resize(0);
		is_create = false;
	};

	void create(ULONG image_width, ULONG image_height, ULONG crop_left, ULONG crop_bottom, ULONG crop_width, ULONG crop_height, ULONG buffer_components)
	{
		full_width = image_width;
		full_height = image_height;
		corner_x = crop_left;
		corner_y = crop_bottom;
		width = crop_width;
		height = crop_height;
		components = buffer_components;

		ccl::ImageSpec buffer_spec = ccl::ImageSpec(crop_width, crop_height, buffer_components, ccl::TypeDesc::FLOAT);
		buffer_pixels.resize((size_t)crop_width * crop_height * buffer_components);
		buffer = new ccl::ImageBuf(buffer_spec, &buffer_pixels[0]);

		is_create = true;
	}

	~RenderVisualBuffer()
	{
		is_create = false;
		buffer->reset();
		delete buffer;
		buffer_pixels.clear();
		buffer_pixels.shrink_to_fit();
	};

	void clear()
	{
		buffer->reset();
		buffer = new ccl::ImageBuf();
		buffer_pixels.clear();
		buffer_pixels.shrink_to_fit();
		is_create = false;
	};

	void add_pixels(OIIO::ROI roi, std::vector<float>& pixels)
	{
		if (is_create && buffer_pixels.size() >= pixels.size())
		{
			buffer->set_pixels(roi, OIIO::TypeDesc::FLOAT, &pixels[0]);
		}
	}

	void add_pixels(ULONG corner_x, ULONG corner_y, ULONG width, ULONG height, int components, std::vector<float> &pixels)
	{
		OIIO::ROI target_roi = OIIO::ROI(corner_x, corner_x + width, corner_y, corner_y + height);
		add_pixels(target_roi, pixels);
	}

	std::vector<float> get_buffer_pixels(OIIO::ROI roi)
	{
		int roi_widhth = roi.xend - roi.xbegin;
		int roi_height = roi.yend - roi.ybegin;
		std::vector<float> to_return((size_t)roi_widhth * roi_height * components, 0.0f);
		if (buffer_pixels.size() >= to_return.size())
		{
			buffer->get_pixels(roi, OIIO::TypeDesc::FLOAT, &to_return[0]);
		}
		
		return to_return;
	}

	std::vector<float> get_buffer_pixels()
	{
		return get_buffer_pixels(OIIO::ROI(0, width, 0, height));
	}

	ULONG get_width()
	{
		return width;
	}

	ULONG get_height()
	{
		return height;
	}

	ULONG get_full_width()
	{
		return full_width;
	}

	ULONG get_full_height()
	{
		return full_height;
	}

	unsigned int get_corner_x()
	{
		return corner_x;
	}

	unsigned int get_corner_y()
	{
		return corner_y;
	}

private:
	unsigned int full_width, full_height;  // full frame size, used when we create a frame before render starts
	unsigned int corner_x, corner_y;  // coordinates of the left bottom corner
	unsigned int width, height;  // actual render size
	unsigned int components;
	ccl::ImageBuf* buffer;
	std::vector<float> buffer_pixels;
	bool is_create;
};