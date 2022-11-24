#include "output_context.h"

#include "../cyc_session/cyc_session.h"
#include "../../utilities/logs.h"
#include "../../utilities/strings.h"
#include "../../utilities/math.h"

OutputContext::OutputContext()
{
	output_paths.Clear();
	output_formats.Clear();
	output_data_types.Clear();
	output_channels.Clear();
	output_bits.resize(0);

	output_pass_types.resize(0);
	output_pass_names.resize(0);
	output_pass_components.resize(0);

	output_pass_paths.resize(0);
	output_pass_formats.resize(0);
	output_pass_write_components.resize(0);
	output_pass_bits.resize(0);

	output_buffers.resize(0);
	output_pixels.resize(0);

	output_buffer_pixels_start.resize(0);
}

OutputContext::~OutputContext()
{
	reset();
}

void OutputContext::reset()
{
	output_paths.Clear();
	output_formats.Clear();
	output_data_types.Clear();
	output_channels.Clear();
	output_bits.clear();
	output_bits.shrink_to_fit();

	output_pass_types.clear();
	output_pass_types.shrink_to_fit();
	output_pass_names.clear();
	output_pass_names.shrink_to_fit();
	output_pass_components.clear();
	output_pass_components.shrink_to_fit();

	output_pass_paths.clear();
	output_pass_paths.shrink_to_fit();
	output_pass_formats.clear();
	output_pass_formats.shrink_to_fit();
	output_pass_write_components.clear();
	output_pass_write_components.shrink_to_fit();
	output_pass_bits.clear();
	output_pass_bits.shrink_to_fit();

	output_buffers.clear();
	output_buffers.shrink_to_fit();
	output_pixels.clear();
	output_pixels.shrink_to_fit();

	output_buffer_pixels_start.clear();
	output_buffer_pixels_start.shrink_to_fit();

	output_passes_count = 0;
	render_type = RenderType::RenderType_Unknown;
	common_path = "";
	render_frame = 0;
}

RenderType OutputContext::get_render_type()
{
	return render_type;
}

ccl::ustring OutputContext::get_visual_pass_name()
{
	return visual_pass_name;
}

ccl::PassType OutputContext::get_visal_pass_type()
{
	return visual_pass;
}

int OutputContext::get_visual_pass_components()
{
	return visual_pass_components;
}

int OutputContext::get_output_passes_count()
{
	return output_passes_count;
}

ULONG OutputContext::get_width()
{
	return image_width;
}

ULONG OutputContext::get_height()
{
	return image_height;
}

XSI::CString OutputContext::get_common_path()
{
	return common_path;
}

int OutputContext::get_render_frame()
{
	return render_frame;
}

ccl::ustring OutputContext::get_output_pass_name(int index)
{
	return output_pass_names[index];
}

ccl::PassType OutputContext::get_output_pass_type(int index)
{
	return output_pass_types[index];
}

void OutputContext::set_output_size(ULONG width, ULONG height)
{
	image_width = width;
	image_height = height;
}

ccl::ustring OutputContext::get_output_pass_path(int index)
{
	return output_pass_paths[index];
}

ccl::ustring OutputContext::get_output_pass_format(int index)
{
	return output_pass_formats[index];
}

int OutputContext::get_output_pass_write_components(int index)
{
	return output_pass_write_components[index];
}

int OutputContext::get_output_pass_components(int index)
{
	return output_pass_components[index];
}

int OutputContext::get_output_pass_bits(int index)
{
	return output_pass_bits[index];
}

float* OutputContext::get_output_pass_pixels(int index)
{
	return &output_pixels[output_buffer_pixels_start[index]];
}

void OutputContext::extract_output_channel(int index, int channel, float* output, bool flip_verticaly)
{
	float* pixels = get_output_pass_pixels(index);
	int components = get_output_pass_components(index);

	size_t pixel_index = 0;
	size_t row_index = 1;
	for (size_t i = 0; i < (size_t)image_width * image_height; i++)
	{
		size_t p = flip_verticaly ? (image_width * (image_height - row_index) + pixel_index) : i;
		output[p] = pixels[components * i + channel];

		pixel_index++;
		if (pixel_index == image_width)
		{
			pixel_index = 0;
			row_index++;
		}
	}
}

OIIO::ImageBuf OutputContext::get_output_buffer(int index)
{
	return output_buffers[index];
}

bool OutputContext::add_output_pixels(ccl::ROI roi, int index, std::vector<float>& pixels)
{
	return output_buffers[index].set_pixels(roi, ccl::TypeDesc::FLOAT, &pixels[0]);
}

void OutputContext::set_render_type(RenderType type)
{
	render_type = type;
}

void OutputContext::set_output_formats(const XSI::CStringArray &paths, const XSI::CStringArray &formats, const XSI::CStringArray &data_types, const XSI::CStringArray &channels, const std::vector<int> &bits, const XSI::CTime& eval_time)
{
	output_paths = paths;
	output_formats = formats;
	output_data_types = data_types;
	output_channels = channels;
	output_bits = bits;

	common_path = get_common_substring(paths);
	render_frame = get_frame(eval_time);
}

void OutputContext::set_visual_pass(const XSI::CString& channel_name)
{
	visual_pass = channel_to_pass_type(channel_name);
	if(visual_pass == ccl::PASS_NONE)
	{
		log_message("Select unknown output channel, switch to Combined", XSI::siWarningMsg);
		visual_pass = ccl::PASS_COMBINED;
	}
	visual_pass_name = ccl::ustring(pass_to_name(visual_pass).GetAsciiString());
	visual_pass_components = get_pass_components(visual_pass);  // don't forget: when visual pass is lightgroup, then pass type is Combined, but it has only 3 components
}

void OutputContext::set_output_passes(const XSI::CStringArray& aov_color_names, const XSI::CStringArray& aov_value_names, const XSI::CStringArray& lightgroup_names)
{
	output_passes_count = 0;
	output_pass_types.clear();
	output_pass_names.clear();
	output_pass_components.clear();
	output_pass_paths.clear();
	output_pass_formats.clear();
	output_pass_write_components.clear();
	output_pass_bits.clear();

	output_buffers.clear();
	output_buffer_pixels_start.clear();

	// at the first enumerate we count total number of components for all output passesand save all data except buffers
	// in most cases one output channel corresponds to one output pass
	// but it's possible to select one channel several times and save it with different names
	// also aovs correspond to several different passes
	int total_components = 0;
	for (size_t i = 0; i < output_channels.GetCount(); i++)
	{
		// channel for visual pass has proper name (Sycles Depth, for example)
		// but name from FrameBuffer name of the render context contains names with _ instead of spaces
		// so, we should replace _ to spaces for all names
		ccl::PassType pass_type = channel_to_pass_type(replace_symbols(output_channels[i], "_", " "));  // convert from XSI channel name to Cycles pass type
		if (pass_type != ccl::PASS_NONE)
		{
			ccl::ustring pass_name = ccl::ustring(pass_to_name(pass_type).GetAsciiString());  // cover from Cycles pass type to the standart name

			// create buffers for the pass
			// we will use float buffer but the number of components is the same as Cycles can generate (4 for Combined but 1 for Depth and so on)
			int pass_components = get_pass_components(pass_type);
			total_components += pass_components;

			// TODO: for aov and lightgroup passes modify names to use different passes for different names

			// add data ot arrays
			output_pass_types.push_back(pass_type);
			output_pass_names.push_back(pass_name);
			output_pass_components.push_back(pass_components);
			output_pass_paths.push_back(ccl::ustring(output_paths[i].GetAsciiString()));
			output_pass_formats.push_back(ccl::ustring(output_formats[i].GetAsciiString()));
			output_pass_write_components.push_back(output_data_types[i].Length());
			output_pass_bits.push_back(output_bits[i]);
			output_passes_count++;
		}
		else
		{
			// fails to convert channel name to the pass
			log_message("Fails to convert channel " + output_channels[i] + " to the render pass, skip it", XSI::siWarningMsg);
		}
	}
	// resize output pixels
	output_pixels.resize((size_t)image_width * image_height * total_components, 0.0f);

	// next wraps all buffers
	total_components = 0;  // use this variable as componets shift for buffer pixels
	for (size_t i = 0; i < output_passes_count; i++)
	{
		int pass_components = output_pass_components[i];
		size_t start_pixels_index = (size_t)image_width * image_height * total_components;
		ccl::ImageBuf pass_buffer = ccl::ImageBuf(ccl::ImageSpec(image_width, image_height, pass_components, ccl::TypeDesc::FLOAT), &output_pixels[start_pixels_index]);
		total_components += pass_components;

		output_buffers.push_back(pass_buffer);
		output_buffer_pixels_start.push_back(start_pixels_index);
	}
}