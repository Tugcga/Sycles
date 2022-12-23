#pragma once

#include <xsi_image.h>
#include <xsi_imageclip2.h>
#include <xsi_time.h>

#include "scene/image.h"

#include <string>

class XSIImageLoader : public ccl::ImageLoader
{
public:
	XSIImageLoader(XSI::ImageClip2 &xsi_clip, const XSI::CTime &eval_time);
	~XSIImageLoader();

	bool load_metadata(const ccl::ImageDeviceFeatures& features, ccl::ImageMetaData& metadata) override;
	bool load_pixels(const ccl::ImageMetaData& metadata, void* pixels, const size_t pixels_size, const bool associate_alpha) override;
	std::string name() const override;
	bool equals(const ImageLoader& other) const override;

	int get_tile_number() const override;

	ULONG get_clip_id() const;

private:
	XSI::CString m_xsi_clip_path;
	XSI::CString m_xsi_clip_name;
	ULONG m_xsi_clip_id;
	XSI::Image m_xsi_image;
	ULONG m_width;
	ULONG m_height;
	ULONG m_channels;
	ULONG m_channel_size;
	XSI::CString m_color_profile;
};