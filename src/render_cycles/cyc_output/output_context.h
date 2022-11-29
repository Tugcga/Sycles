#pragma once
#include "scene/pass.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_string.h>

#include "../../render_base/type_enums.h"
#include "../cyc_scene/cyc_labels.h"

class OutputContext
{
public:
	OutputContext();
	~OutputContext();

	void reset();

	void set_render_type(RenderType type);
	void set_output_passes(const XSI::CStringArray &aov_color_names, const XSI::CStringArray& aov_value_names, const XSI::CStringArray& lightgroup_names);
	RenderType get_render_type();
	int get_output_passes_count();
	ULONG get_width();
	ULONG get_height();
	XSI::CString get_common_path();
	int get_render_frame();
	ccl::ustring get_output_pass_name(int index);
	ccl::PassType get_output_pass_type(int index);
	ccl::ustring get_output_pass_path(int index);
	ccl::ustring get_output_pass_format(int index);
	int get_output_pass_write_components(int index);
	int get_output_pass_components(int index);
	int get_output_pass_bits(int index);
	float* get_output_pass_pixels(int index);
	float* get_labels_pixels();
	void extract_output_channel(int index, int channel, float* output, bool flip_verticaly = false);
	void extract_lables_channel(int channel, float* output, bool flip_verticaly = false);
	OIIO::ImageBuf get_output_buffer(int index);
	bool get_is_labels();

	bool add_output_pixels(ccl::ROI roi, int index, std::vector<float> &pixels);

	void set_output_size(ULONG width, ULONG height);
	void set_output_formats(const XSI::CStringArray& paths, const XSI::CStringArray& formats, const XSI::CStringArray& data_types, const XSI::CStringArray& channels, const std::vector<int>& bits, const XSI::CTime& eval_time);
	void set_labels_buffer(LabelsContext* labels_context);
	void overlay_labels();  // this method bake labels into each combined output pass, it should be called after all saves before output separate images

private:
	void add_one_pass_data(ccl::PassType pass_type, const XSI::CString& pass_name, int pass_components, int i, const XSI::CString& output_path);

	ULONG image_width;  // these sizes contains full size (without crop)
	ULONG image_height;  // for preview rendering these values does not used, but for pass rendeering it contains full image
	// these values copied from the base renderer
	RenderType render_type;
	XSI::CStringArray output_paths;
	XSI::CStringArray output_formats;
	XSI::CStringArray output_data_types;
	XSI::CStringArray output_channels;
	std::vector<int> output_bits;
	XSI::CString common_path;
	int render_frame;

	// labels
	bool is_labels;
	std::vector<float> labels_buffer_pixels;
	ccl::ImageBuf labels_buffer;

	// for each output channel we should create several oputput Cycles passes
	// for most of them this pass is unique, but for aovs and lightgroups several output Cycles passes may corresponds one output channel
	// so, store output passes in separate arrays but with identical indexing
	size_t output_passes_count;
	ccl::vector<ccl::PassType> output_pass_types;
	ccl::vector<ccl::ustring> output_pass_names;  // these names used for geting pixels from render tiles
	ccl::vector<int> output_pass_components;  // the number of components for each pass (this value use for the buffer)
	ccl::vector<ccl::ustring> output_pass_paths;  // full save path
	ccl::vector<ccl::ustring> output_pass_formats;  // jpg or png and so on
	ccl::vector<int> output_pass_write_components;  // how many components selected for save image (get as length of the string RGB or RGBA)
	ccl::vector<int> output_pass_bits;  // selected bit depth of the image to save
	ccl::vector<ccl::ImageBuf> output_buffers;  // store here buffers with pixels for each output pass
	std::vector<float> output_pixels;  // store pixels for all output passes in one array, buffers wraps different sections of this array
	std::vector<size_t> output_buffer_pixels_start;  // contains the first index in the common pixels array of pixels for the current buffer
};