#pragma once
#include <xsi_image.h>
#include <xsi_imageclip2.h>
#include <xsi_time.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>

#include "scene/image.h"
#include "scene/image_vdb.h"

#include <string>

#include "../../../render_base/type_enums.h"
#include "../../../utilities/logs.h"

class XSIImageLoader : public ccl::ImageLoader
{
public:
	XSIImageLoader(XSI::ImageClip2 &xsi_clip, const ccl::ustring &selected_colorspace, int tile, const XSI::CString &image_path, const XSI::CTime &eval_time);
	~XSIImageLoader();

	bool load_metadata(const ccl::ImageDeviceFeatures& features, ccl::ImageMetaData& metadata) override;
	bool load_pixels(const ccl::ImageMetaData& metadata, void* pixels, const size_t pixels_size, const bool associate_alpha) override;
	std::string name() const override;
	bool equals(const ImageLoader& other) const override;
	void cleanup() override;

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

class ICEVolumeLoader : public ccl::ImageLoader
{
public:
	ICEVolumeLoader(VolumeAttributeType attribute_type, const XSI::Primitive &xsi_primitive, const std::string &attribute_name, const XSI::CTime &eval_time);
	~ICEVolumeLoader();

	bool load_metadata(const ccl::ImageDeviceFeatures& features, ccl::ImageMetaData& metadata) override;
	bool load_pixels(const ccl::ImageMetaData& metadata, void* pixels, const size_t pixels_size, const bool associate_alpha) override;
	std::string name() const override;
	bool equals(const ImageLoader& other) const override;
	void cleanup() override;
	bool is_vdb_loader() const;

	size_t get_size_x() const;
	size_t get_size_y() const;
	size_t get_size_z() const;

	float get_min_x() const;
	float get_min_y() const;
	float get_min_z() const;

	float get_max_x() const;
	float get_max_y() const;
	float get_max_z() const;

	XSI::CString get_attribute_name() const;
	ULONG get_primitive_id() const;
	VolumeAttributeType get_attribute_type() const;

	bool is_empty() const;

private:
	size_t m_size_x;
	size_t m_size_y;
	size_t m_size_z;
	float m_min_x;
	float m_min_y;
	float m_min_z;
	float m_max_x;
	float m_max_y;
	float m_max_z;

	VolumeAttributeType m_attribute_type;
	ULONG m_xsi_primitive_id;
	XSI::CString m_xsi_attribute_name;
	XSI::ICEAttribute m_xsi_attribute;  // contains pixels data

	bool m_is_empty;
};

class XSIVDBLoader : public ccl::VDBImageLoader
{
public:
	XSIVDBLoader(openvdb::GridBase::ConstPtr vdb_grid, const ULONG id, const XSI::CString& file, const ULONG& index, const std::string& grid_name) : VDBImageLoader(grid_name), primitive_id(id), grid_index(index), file_name(file)
	{
		grid = vdb_grid;
	}

	bool equals(const ImageLoader& other) const override
	{
		const XSIVDBLoader& other_loader = (const XSIVDBLoader&)other;
		return primitive_id == other_loader.primitive_id &&
			grid_index == other_loader.grid_index &&
			file_name == other_loader.file_name;
	}

	ULONG primitive_id;
	ULONG grid_index;
	XSI::CString file_name;
};