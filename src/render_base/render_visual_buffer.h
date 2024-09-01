#pragma once
#include <xsi_dataarray.h>
#include <xsi_arrayparameter.h>

#include "scene/pass.h"

#include <vector>

#include "../utilities/logs.h"
#include "../render_cycles/cyc_session/cyc_pass_utils.h"
#include "../render_base/image_buffer.h"

class RenderVisualBuffer
{
public:
	RenderVisualBuffer() 
	{
		buffer = new ImageBuffer(); 
		is_create = false;
	};

	bool is_coincide(ULONG image_width, ULONG image_height, ULONG crop_left, ULONG crop_bottom, ULONG crop_width, ULONG crop_height, const XSI::CString& display_pass_name, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
	{
		return full_width == image_width &&
			full_height == image_height &&
			corner_x == crop_left &&
			corner_y == crop_bottom &&
			width == crop_width &&
			height == crop_height &&
			pass_name == ccl::ustring(display_pass_name.GetAsciiString());
	}

	void setup(ULONG image_width, ULONG image_height, ULONG crop_left, ULONG crop_bottom, ULONG crop_width, ULONG crop_height, const XSI::CString &channel_name, const XSI::CString &display_pass_name, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
	{
		pass_type = channel_to_pass_type(channel_name);  // for lightgroups it returns Combined
		if (pass_type == ccl::PASS_NONE)
		{
			pass_type = ccl::PASS_COMBINED;
		}
		pass_name = ccl::ustring(display_pass_name.GetAsciiString());
		components = get_pass_components(pass_type, channel_name == "Cycles Lightgroup");

		full_width = image_width;
		full_height = image_height;
		corner_x = crop_left;
		corner_y = crop_bottom;
		width = crop_width;
		height = crop_height;

		buffer->recreate(crop_width, crop_height, components);
		is_create = true;
	}

	~RenderVisualBuffer()
	{
		is_create = false;
		buffer->reset();
		delete buffer;
	};

	void clear()
	{
		buffer->reset();
		is_create = false;
	};

	void add_pixels(const ImageRectangle &roi, std::vector<float>& pixels)
	{
		if (is_create && buffer->get_buffer_size() >= pixels.size())
		{
			buffer->set_pixels(roi, &pixels[0]);
		}
	}

	void add_pixels(ULONG corner_x, ULONG corner_y, ULONG width, ULONG height, int components, std::vector<float> &pixels)
	{
		ImageRectangle target_roi = ImageRectangle(corner_x, corner_x + width, corner_y, corner_y + height);
		add_pixels(target_roi, pixels);
	}

	std::vector<float> get_buffer_pixels(ImageRectangle &roi)
	{
		size_t roi_widhth = roi.get_width();
		size_t roi_height = roi.get_height();
		std::vector<float> to_return(roi_widhth * roi_height * components, 0.0f);
		if (buffer->get_buffer_size() >= to_return.size())
		{
			buffer->get_pixels(roi, &to_return[0]);
		}
		
		return to_return;
	}

	std::vector<float> get_buffer_pixels()
	{
		return buffer->get_pixels();
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

	ccl::ustring get_pass_name()
	{
		return pass_name;
	}

	void set_pass_name(const XSI::CString &name)
	{
		pass_name = ccl::ustring(name.GetAsciiString());
	}

	ccl::PassType get_pass_type()
	{
		return pass_type;
	}

	size_t get_components()
	{
		return components;
	}

	std::vector<float> convert_channel_pixels(size_t in_channels)
	{
		return buffer->convert_channel_pixels(in_channels);
	}
	void redefine_rgb(const std::vector<float>& rgb_pixels)
	{
		buffer->redefine_rgb(rgb_pixels);
	}

private:
	unsigned int full_width, full_height;  // full frame size, used when we create a frame before render starts
	unsigned int corner_x, corner_y;  // coordinates of the left bottom corner
	unsigned int width, height;  // actual render size
	size_t components;
	ImageBuffer* buffer;
	bool is_create;

	ccl::PassType pass_type;  // what pass should be visualised into the screen
	ccl::ustring pass_name;
};