#pragma once
#include <xsi_dataarray.h>
#include <xsi_arrayparameter.h>

#include "scene/pass.h"

#include <vector>

#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"

#include "../utilities/logs.h"
#include "../render_cycles/cyc_session/cyc_pass_utils.h"

class RenderVisualBuffer
{
public:
	RenderVisualBuffer() 
	{ 
		buffer = new ccl::ImageBuf(); 
		buffer_pixels.resize(0);
		is_create = false;
	};

	void setup(ULONG image_width, ULONG image_height, ULONG crop_left, ULONG crop_bottom, ULONG crop_width, ULONG crop_height, const XSI::CString &channel_name, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
	{
		pass_type = channel_to_pass_type(channel_name);
		if (pass_type == ccl::PASS_NONE)
		{
			log_message("Select unknown output channel, switch to Combined", XSI::siWarningMsg);
			pass_type = ccl::PASS_COMBINED;
		}
		pass_name = ccl::ustring(pass_to_name(pass_type).GetAsciiString());
		if (pass_type == ccl::PASS_AOV_COLOR || pass_type == ccl::PASS_AOV_VALUE)
		{// change the name of the pass into name from render parameters
			// at present time we does not know is this name correct or not
			// so, does not change it to correct name, but later show warning message, if the name is incorrect
			pass_name = add_prefix_to_aov_name(render_parameters.GetValue("output_pass_preview_name", eval_time), pass_type == ccl::PASS_AOV_COLOR).GetAsciiString();
		}
		components = get_pass_components(pass_type);  // don't forget: when visual pass is lightgroup, then pass type is Combined, but it has only 3 components

		full_width = image_width;
		full_height = image_height;
		corner_x = crop_left;
		corner_y = crop_bottom;
		width = crop_width;
		height = crop_height;

		ccl::ImageSpec buffer_spec = ccl::ImageSpec(crop_width, crop_height, components, ccl::TypeDesc::FLOAT);
		buffer_pixels.resize((size_t)crop_width * crop_height * components);
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
		return buffer_pixels;
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

private:
	unsigned int full_width, full_height;  // full frame size, used when we create a frame before render starts
	unsigned int corner_x, corner_y;  // coordinates of the left bottom corner
	unsigned int width, height;  // actual render size
	size_t components;
	ccl::ImageBuf* buffer;
	std::vector<float> buffer_pixels;
	bool is_create;

	ccl::PassType pass_type;  // what pass should be visualised into the screen
	ccl::ustring pass_name;
};