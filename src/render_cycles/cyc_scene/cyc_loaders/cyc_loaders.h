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

class ICEVDBLoader : public ccl::VDBImageLoader {
public:
	ICEVDBLoader(VolumeAttributeType attribute_type, const XSI::Primitive& xsi_primitive, const std::string& attribute_name, size_t update_generation, const XSI::CTime& eval_time);
	~ICEVDBLoader();

	std::string name() const override;
	bool equals(const ImageLoader& other) const override;
	bool is_empty();

	ULONG m_primitive_id;
	XSI::CString m_attribute_name;
	bool m_is_empty;
	size_t m_generation;
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