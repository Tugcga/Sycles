#include "OpenImageIO/imagebufalgo.h"

#include "util/murmurhash.h"

#include "output_context.h"
#include "../cyc_session/cyc_pass_utils.h"
#include "../../utilities/logs.h"
#include "../../utilities/strings.h"
#include "../../utilities/math.h"
#include "../../output/write_image.h"

OutputContext::OutputContext()
{
	is_labels = false;
	labels_buffer_pixels.resize(0);
	labels_buffer.reset();

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

	output_passes_count = 0;
	render_type = RenderType::RenderType_Unknown;
	common_path = "";
	render_frame = 0;

	crypto_buffer_indices.resize(0);
	is_cryptomatte = false;
	is_crypto_object = false;
	is_crypto_material = false;
	is_crypto_asset = false;

	crypto_keys.resize(0);
	crypto_values.resize(0);
}

OutputContext::~OutputContext()
{
	reset();
}

void OutputContext::reset()
{
	labels_buffer_pixels.clear();
	labels_buffer_pixels.shrink_to_fit();
	is_labels = false;
	labels_buffer.reset();

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

	crypto_buffer_indices.clear();
	crypto_buffer_indices.shrink_to_fit();
	is_cryptomatte = false;
	is_crypto_object = false;
	is_crypto_material = false;
	is_crypto_asset = false;

	crypto_keys.clear();
	crypto_keys.shrink_to_fit();
	crypto_values.clear();
	crypto_values.shrink_to_fit();
}

RenderType OutputContext::get_render_type()
{
	return render_type;
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

	extract_channel(image_width, image_height, channel, components, flip_verticaly, pixels, output);
}

void OutputContext::extract_lables_channel(int channel, float* output, bool flip_verticaly)
{
	extract_channel(image_width, image_height, channel, 4, flip_verticaly, &labels_buffer_pixels[0], output);
}

OIIO::ImageBuf OutputContext::get_output_buffer(int index)
{
	return output_buffers[index];
}

bool OutputContext::get_is_labels()
{
	return is_labels;
}

float* OutputContext::get_labels_pixels()
{
	return &labels_buffer_pixels[0];
}

bool OutputContext::add_output_pixels(ccl::ROI roi, int index, std::vector<float>& pixels)
{
	return output_buffers[index].set_pixels(roi, ccl::TypeDesc::FLOAT, &pixels[0]);
}

void OutputContext::set_render_type(RenderType type)
{
	render_type = type;
}

void OutputContext::set_output_formats(const XSI::CStringArray &paths, 
	const XSI::CStringArray &formats, 
	const XSI::CStringArray &data_types, 
	const XSI::CStringArray &channels, 
	const std::vector<int> &bits, 
	const XSI::CTime& eval_time)
{
	output_paths = paths;
	output_formats = formats;
	output_data_types = data_types;
	output_channels = channels;
	output_bits = bits;

	common_path = get_common_substring(paths);
	render_frame = get_frame(eval_time);
}

void OutputContext::set_cryptomatte_settings(bool object, bool material, bool asset, int levels)
{
	is_crypto_object = object;
	is_crypto_material = material;
	is_crypto_asset = asset;
	crypto_levels = levels;

	// output cryptomatte passes only for Pass tendering
	is_cryptomatte = (is_crypto_object || is_crypto_material || is_crypto_asset) && (render_type == RenderType_Pass);
}

void OutputContext::set_labels_buffer(LabelsContext* labels_context)
{
	if (labels_context->is_labels())
	{
		is_labels = true;
		labels_buffer_pixels.resize((size_t)image_width * image_height * 4);
		labels_buffer = OIIO::ImageBuf(ccl::ImageSpec(image_width, image_height, 4, ccl::TypeDesc::FLOAT), &labels_buffer_pixels[0]);
		
		build_labels_buffer(labels_buffer, labels_context->get_string(), image_width, image_height,
			0,  // horisontal shift
			20,  // height
			0.2, 0.2, 0.2, 0.6,  // back color
			0);  // bottom row
	}
	else
	{
		is_labels = false;
		labels_buffer_pixels.clear();
		labels_buffer.reset();
	}
}

void OutputContext::overlay_labels()
{
	if (is_labels)
	{
		for (size_t i = 0; i < output_passes_count; i++)
		{
			ccl::PassType pass_type = get_output_pass_type(i);
			if (pass_type == ccl::PASS_COMBINED)
			{
				overlay_pixels(image_width, image_height, get_labels_pixels(), get_output_pass_pixels(i));
			}
		}
	}
}

bool OutputContext::get_is_cryptomatte()
{
	return is_cryptomatte;
}

int OutputContext::get_ctypto_depth()
{
	return crypto_depth;
}

ccl::CryptomatteType OutputContext::get_crypto_passes()
{
	return crypto_passes;
}

std::vector<size_t> OutputContext::get_crypto_buffer_indices()
{
	return crypto_buffer_indices;
}

std::vector<std::string> OutputContext::get_crypto_keys()
{
	return crypto_keys;
}

std::vector<std::string> OutputContext::get_crypto_values()
{
	return crypto_values;
}

void OutputContext::add_cryptomatte_metadata(std::string name, std::string manifest)
{
	std::string identifier = ccl::string_printf("%08x", ccl::util_murmur_hash3(name.c_str(), name.length(), 0));
	std::string prefix = "cryptomatte/" + identifier.substr(0, 7) + "/";
	crypto_keys.push_back(prefix + "name");
	crypto_values.push_back(name);

	crypto_keys.push_back(prefix + "hash");
	crypto_values.push_back("MurmurHash3_32");

	crypto_keys.push_back(prefix + "conversion");
	crypto_values.push_back("uint32_to_float32");

	crypto_keys.push_back(prefix + "manifest");
	crypto_values.push_back(manifest);
}

void OutputContext::add_one_pass_data(ccl::PassType pass_type, const XSI::CString &pass_name, int pass_components, int index, const XSI::CString &output_path)
{
	// add data to arrays
	output_pass_types.push_back(pass_type);
	output_pass_names.push_back(ccl::ustring(pass_name.GetAsciiString()));
	output_pass_components.push_back(pass_components);
	output_pass_paths.push_back(ccl::ustring(output_path.GetAsciiString()));
	if (index >= 0)
	{
		output_pass_formats.push_back(ccl::ustring(output_formats[index].GetAsciiString()));
		output_pass_write_components.push_back(output_data_types[index].Length());
		output_pass_bits.push_back(output_bits[index]);
	}
	else
	{// if we set index = -1, then it means that we should setup format, write components and bits manually
		// in fact we set it only for cryptomatte passes, because these passe does not come from output channels list
		output_pass_formats.push_back(ccl::ustring(""));  // empty extension, because we will save it manually
		// output path is also should be set empty
		output_pass_write_components.push_back(4);  // always 4 components
		output_pass_bits.push_back(21);  // always float 32
	}
	
	output_passes_count++;
}

// this method calls after we sync the scene, but before we setup all render passes
// data from the object after this method is used for setting all render passes
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
	crypto_buffer_indices.clear();

	crypto_keys.clear();
	crypto_values.clear();

	// at the first enumerate we count total number of components for all output passes and save all data except buffers
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
		if (pass_type != ccl::PASS_NONE && output_paths[i].Length() > 0)
		{
			XSI::CString pass_name = pass_to_name(pass_type);  // cover from Cycles pass type to the standart name
			int pass_components = get_pass_components(pass_type);
			total_components += pass_components;

			if (pass_type == ccl::PASS_AOV_COLOR || pass_type == ccl::PASS_AOV_VALUE)
			{// we should add several output passes, for each name in the input array
				XSI::CStringArray aov_names = pass_type == ccl::PASS_AOV_COLOR ? aov_color_names : aov_value_names;
				for (size_t aov_index = 0; aov_index < aov_names.GetCount(); aov_index++)
				{
					// we should convert aov name by adding some specific prefix
					// this will guarantee that all pass names are unique
					XSI::CString aov_name = add_prefix_to_aov_name(aov_names[aov_index], pass_type == ccl::PASS_AOV_COLOR);
					// we assume that input arrays contains aov names without repetitions

					int pass_components = get_pass_components(pass_type);
					total_components += pass_components;

					// we should modify output pass, add at the end the name of the pass (original name, not modified)
					add_one_pass_data(pass_type, aov_name, pass_components, i, add_aov_name_to_path(output_paths[i], aov_names[aov_index]));
				}
			}
			else
			{
				add_one_pass_data(pass_type, pass_name, pass_components, i, output_paths[i]);
			}
		}
		else
		{
			if (pass_type == ccl::PASS_NONE)
			{
				// fails to convert channel name to the pass
				log_message("Fails to convert channel " + output_channels[i] + " to the render pass, skip it", XSI::siWarningMsg);
			}
			if (output_paths[i].Length() == 0)
			{
				log_message("There is empty output path, skip it", XSI::siWarningMsg);
			}
		}
	}

	// next add cryptomatte passes
	crypto_passes = ccl::CRYPT_NONE;
	const char* cryptomatte_prefix = "Crypto";
	if (is_cryptomatte)
	{
		crypto_depth = ccl::divide_up(ccl::min(16, crypto_levels), 2);
		int crypto_components = 4;

		if (is_crypto_object)
		{
			for (int i = 0; i < crypto_depth; i++)
			{
				XSI::CString pass_name = XSI::CString((cryptomatte_prefix + ccl::string_printf("Object%02d", i)).c_str());
				add_one_pass_data(ccl::PASS_CRYPTOMATTE, pass_name, crypto_components, -1, "");
				crypto_buffer_indices.push_back(output_passes_count - 1);
				total_components += crypto_components;
			}
			crypto_passes = (ccl::CryptomatteType)(crypto_passes | ccl::CRYPT_OBJECT);
		}

		if (is_crypto_material)
		{
			for (int i = 0; i < crypto_depth; i++)
			{
				XSI::CString pass_name = XSI::CString((cryptomatte_prefix + ccl::string_printf("Material%02d", i)).c_str());
				add_one_pass_data(ccl::PASS_CRYPTOMATTE, pass_name, crypto_components, -1, "");
				crypto_buffer_indices.push_back(output_passes_count - 1);
				total_components += crypto_components;
			}
			crypto_passes = (ccl::CryptomatteType)(crypto_passes | ccl::CRYPT_MATERIAL);
		}

		if (is_crypto_asset)
		{
			for (int i = 0; i < crypto_depth; i++)
			{
				XSI::CString pass_name = XSI::CString((cryptomatte_prefix + ccl::string_printf("Asset%02d", i)).c_str());
				add_one_pass_data(ccl::PASS_CRYPTOMATTE, pass_name, crypto_components, -1, "");
				crypto_buffer_indices.push_back(output_passes_count - 1);
				total_components += crypto_components;
			}
			crypto_passes = (ccl::CryptomatteType)(crypto_passes | ccl::CRYPT_ASSET);
		}
	}

	// resize output pixels
	output_pixels.resize((size_t)image_width * image_height * total_components, 0.0f);

	// next wraps all buffers
	total_components = 0;  // use this variable as components shift for buffer pixels
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