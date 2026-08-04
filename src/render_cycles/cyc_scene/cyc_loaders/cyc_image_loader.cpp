#include <xsi_arrayparameter.h>
#include <xsi_parameter.h>
#include <xsi_source.h>

#include "cyc_loaders.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/files_io.h"

// WARNING: these values should synced with txture_limit_combo in cycles_ui.cpp
std::vector<int> limit_values = { INT_MAX, 131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024, 512, 256, 128, 64, 32, 16 };
XSI::siImageRatio limit_to_scale_ration(int size, TextureLimits texture_limit) {
	// we should find the divisor for the size such that the result is less or equal to the limit value
	// we can devide to 2, 4, 8, 16 only
	int limit_index = static_cast<int>(texture_limit);
	if (size <= limit_values[texture_limit]) {
		return XSI::siImageRatio::siImageRatio1x1;
	}
	else if (size / 2 <= limit_values[texture_limit]) {
		return XSI::siImageRatio::siImageRatio2x1;
	}
	else if (size / 4 <= limit_values[texture_limit]) {
		return XSI::siImageRatio::siImageRatio4x1;
	}
	else if (size / 8 <= limit_values[texture_limit]) {
		return XSI::siImageRatio::siImageRatio8x1;
	}
	else {
		return XSI::siImageRatio::siImageRatio16x1;
	}
}

XSIImageLoader::XSIImageLoader(
	XSI::ImageClip2& xsi_clip, 
	const ccl::ustring& selected_colorspace, 
	int tile, 
	const XSI::CString& image_path, 
	bool use_texture_cache, 
	std::map<std::string, XSI::Image>& path_to_image, 
	TextureLimits texture_limits,
	const XSI::CTime& eval_time) {
	// if image_path is empty, then use clip as source image
	// in other case, load pixels from the image_path
	m_tile = tile;
	m_image_path = image_path;

	bool proper_source = false;
	if (image_path.Length() == 0) {
		m_use_clip = true;
		XSI::Source clip_source(xsi_clip.GetSource());
		m_xsi_clip_name = clip_source.GetParameterValue("path");
		int x_res = clip_source.GetParameterValue("XRes");
		int y_res = clip_source.GetParameterValue("YRes");

		std::string clip_file_path = m_xsi_clip_name.GetAsciiString();
		if (use_texture_cache && path_to_image.contains(clip_file_path)) {
			m_xsi_image = path_to_image[clip_file_path];
		} 
		else{
			// m_xsi_image = xsi_clip.GetImage(eval_time);
			XSI::siImageRatio downscale = limit_to_scale_ration(std::max(x_res, y_res), texture_limits);
			// WARNING: may be it's Softimage bug
			// but with non-trivial downsample value the obtained Image have non-reduced size
			// siImageRatio2x1 and siImageRatio4x1 are not effected
			// siImageRatio8x1 and siImageRatio16x1 are the same (siImageRatio16x1)
			m_xsi_image = xsi_clip.GetScaledDownImage(downscale, eval_time);
			if (use_texture_cache) {
				path_to_image[clip_file_path] = m_xsi_image;
			}
		}

		m_width = m_xsi_image.GetResX();
		m_height = m_xsi_image.GetResY();
		m_channels = m_xsi_image.GetNumChannels();
		m_channel_size = m_xsi_image.GetChannelSize();  // 1 for ldr, 4 for float hdr
		proper_source = true;
	}
	else {
		m_use_clip = false;

		// read pixels and override texture size
		bool is_sucess = false;
		ULONG out_width = 0;
		ULONG out_height = 0;
		ULONG out_channels = 0;
		m_image_pixels = load_image(image_path, out_width, out_height, out_channels, is_sucess);

		if (is_sucess)
		{
			m_width = out_width;
			m_height = out_height;
			m_channels = out_channels;
			m_use_clip = false;
			proper_source = true;
		}
	}

	if (!proper_source) {
		m_width = 1;
		m_height = 1;
		m_channels = 1;
		m_image_pixels.resize(1);
		m_image_pixels[0] = 0.0;
	}

	m_color_profile = selected_colorspace;
}

XSIImageLoader::~XSIImageLoader()
{
	m_image_pixels.clear();
	m_image_pixels.shrink_to_fit();
}

bool XSIImageLoader::load_metadata(ccl::ImageMetaData& metadata, const ccl::ImageLoaderParams& params, ccl::Progress& progress)
{
	metadata.width = m_width;
	metadata.height = m_height;
	metadata.channels = m_channels;
	metadata.colorspace = m_color_profile;
	// TODO: should we define is_unassociated_alpha and what value

	if (m_use_clip)
	{
		if (m_channel_size == 1)
		{
			metadata.type = m_channels == 1 ? ccl::IMAGE_DATA_TYPE_BYTE : ccl::IMAGE_DATA_TYPE_BYTE4;
		}
		else if (m_channel_size == 2)
		{
			metadata.type = m_channels == 1 ? ccl::IMAGE_DATA_TYPE_HALF : ccl::IMAGE_DATA_TYPE_HALF4;
		}
		else if (m_channel_size == 4)
		{
			metadata.type = m_channels == 1 ? ccl::IMAGE_DATA_TYPE_FLOAT : ccl::IMAGE_DATA_TYPE_FLOAT4;
		}
	}
	else
	{
		metadata.type = m_channels == 1 ? ccl::IMAGE_DATA_TYPE_FLOAT : ccl::IMAGE_DATA_TYPE_FLOAT4;
	}

	return true;
}

bool XSIImageLoader::load_pixels(const ccl::ImageMetaData& metadata, void* pixels)
{
	// our image may contains some channels, but output pixels always either 1 or 4 channels
	// TODO: implement the method
	/*ULONG pixels_count = m_width * m_height;
	ULONG output_channels = pixels_size / pixels_count;

	if (m_use_clip)
	{
		// use pixels from the clip
		if (m_channel_size == 1)
		{// image pixels are bytes
			// in this case output pixels also set as bytes	
			// get image pixels
			const unsigned char* image_pixels = (unsigned char*)m_xsi_image.GetPixelArray();

			if (output_channels == m_channels)
			{
				// copy pixels data
				memcpy(pixels, image_pixels, pixels_size * sizeof(unsigned char));
			}
			else
			{// output channels are greater than image channels
				// this means that output has 4 channels, but image has less
				// so, we should copy the data and fill other channels by 1.0 (or 255)
				unsigned char* out_pixel = (unsigned char*)pixels;
				for (ULONG i = 0; i < pixels_count; i++)
				{
					for (ULONG c = 0; c < m_channels; c++)
					{
						out_pixel[c] = image_pixels[m_channels * i + c];
					}
					for (ULONG c = m_channels; 4; c++)
					{
						out_pixel[c] = 255;
					}

					out_pixel += 4;
				}
			}

			if (associate_alpha && m_channels == 4)
			{
				// like in Blender, premultiply alpha
				unsigned char* out_pixel = (unsigned char*)pixels;
				for (size_t i = 0; i < pixels_count; i++)
				{
					out_pixel[0] = (out_pixel[0] * out_pixel[3]) / 255;
					out_pixel[1] = (out_pixel[1] * out_pixel[3]) / 255;
					out_pixel[2] = (out_pixel[2] * out_pixel[3]) / 255;

					out_pixel += 4;
				}
			}

			return true;
		}
		else if (m_channel_size == 2)
		{// image pixels are half
			// does not support this
			memset(pixels, 0, pixels_size * sizeof(ccl::half));
			return false;
		}
		else if (m_channel_size == 4)
		{// image pixels are float
			const float* image_pixels = (float*)m_xsi_image.GetPixelArray();

			if (output_channels == m_channels)
			{
				memcpy(pixels, image_pixels, pixels_size * sizeof(float));
			}
			else
			{
				float* out_pixel = (float*)pixels;
				for (ULONG i = 0; i < pixels_count; i++)
				{
					for (ULONG c = 0; c < m_channels; c++)
					{
						out_pixel[c] = image_pixels[m_channels * i + c];
					}
					for (ULONG c = m_channels; 4; c++)
					{
						out_pixel[c] = 1.0f;
					}

					out_pixel += 4;
				}
			}

			if (associate_alpha && m_channels == 4)
			{
				// like in Blender, premultiply alpha
				unsigned char* out_pixel = (unsigned char*)pixels;
				for (size_t i = 0; i < pixels_count; i++)
				{
					out_pixel[0] = (out_pixel[0] * out_pixel[3]);
					out_pixel[1] = (out_pixel[1] * out_pixel[3]);
					out_pixel[2] = (out_pixel[2] * out_pixel[3]);

					out_pixel += 4;
				}
			}

			return true;
		}
	}
	else
	{
		// use already loaded pixels
		if (m_channels == output_channels)
		{
			// loaded pixels and output pixels contains the same number of channels (1 or 4)
			memcpy(pixels, &m_image_pixels[0], pixels_size * sizeof(float));
		}
		else
		{
			// output pixel channels different with loaded pixle channels
			// loaded image may have 2 or 3 channels and output requires 4 channels
			float* out_pixel = (float*)pixels;
			for (ULONG i = 0; i < pixels_count; i++)
			{
				for (ULONG c = 0; c < m_channels; c++)
				{
					out_pixel[c] = m_image_pixels[m_channels * i + c];
				}
				for (ULONG c = m_channels; 4; c++)
				{
					out_pixel[c] = 1.0f;
				}

				out_pixel += 4;
			}
		}

		return true;
	}*/

	return false;
}

void XSIImageLoader::cleanup()
{
	m_image_pixels.clear();
	m_image_pixels.shrink_to_fit();
}

std::string XSIImageLoader::name() const
{
	return m_xsi_clip_name.GetAsciiString();
}

XSI::CString XSIImageLoader::get_clip_id() const
{
	return m_xsi_clip_name;
}

XSI::CString XSIImageLoader::get_image_path() const
{
	return m_image_path;
}

bool XSIImageLoader::equals(const ImageLoader& other) const
{
	const XSIImageLoader& other_loader = (const XSIImageLoader&)other;
	return m_xsi_clip_name == other_loader.get_clip_id() && m_tile == other_loader.get_tile_number() && m_image_path == other_loader.get_image_path();
}

int XSIImageLoader::get_tile_number() const
{
	return m_tile;
}