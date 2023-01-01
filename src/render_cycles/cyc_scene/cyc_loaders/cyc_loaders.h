#pragma once

#include <xsi_image.h>
#include <xsi_imageclip2.h>
#include <xsi_time.h>

#include "scene/image.h"

#include <string>

class XSIImageLoader : public ccl::ImageLoader
{
public:
	XSIImageLoader(XSI::ImageClip2 &xsi_clip, const ccl::ustring &selected_colorspace, int tile, const XSI::CString &image_path, const XSI::CTime &eval_time);
	~XSIImageLoader();

	bool load_metadata(const ccl::ImageDeviceFeatures& features, ccl::ImageMetaData& metadata) override;
	bool load_pixels(const ccl::ImageMetaData& metadata, void* pixels, const size_t pixels_size, const bool associate_alpha) override;
	std::string name() const override;
	bool equals(const ImageLoader& other) const override;

	int get_tile_number() const override;

	ULONG get_clip_id() const;
	XSI::CString get_image_path() const;

private:
	XSI::CString m_xsi_clip_path;
	XSI::CString m_xsi_clip_name;
	ULONG m_xsi_clip_id;
	XSI::Image m_xsi_image;
	ULONG m_width;
	ULONG m_height;
	ULONG m_channels;
	ULONG m_channel_size;
	ccl::ustring m_color_profile;

	int m_tile;
	// this string is non-empty for non-default tile images
	XSI::CString m_image_path;
	bool m_use_clip;  // treu if we should extract pixels from the clip, false if read manualy by using image path
	std::vector<float> m_image_pixels;
};