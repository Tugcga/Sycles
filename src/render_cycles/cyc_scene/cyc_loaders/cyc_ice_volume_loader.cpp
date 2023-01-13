#include <xsi_arrayparameter.h>
#include <xsi_parameter.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>

#include "cyc_loaders.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/files_io.h"
#include "../../../render_base/type_enums.h"

ICEVolumeLoader::ICEVolumeLoader(VolumeAttributeType attribute_type, const XSI::Primitive& xsi_primitive, const std::string& attribute_name, const XSI::CTime& eval_time)
{
	m_xsi_primitive_id = xsi_primitive.GetObjectID();
	m_xsi_attribute_name = XSI::CString(attribute_name.c_str());
	m_attribute_type = attribute_type;

	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);
	m_xsi_attribute = xsi_geometry.GetICEAttributeFromName(m_xsi_attribute_name);

	// get size and corners attributes
	XSI::ICEAttribute size_attribute = xsi_geometry.GetICEAttributeFromName(m_xsi_attribute_name + "_size");
	XSI::ICEAttribute min_attribute = xsi_geometry.GetICEAttributeFromName(m_xsi_attribute_name + "_min");
	XSI::ICEAttribute max_attribute = xsi_geometry.GetICEAttributeFromName(m_xsi_attribute_name + "_max");

	XSI::CICEAttributeDataArrayVector3f size_data;
	size_attribute.GetDataArray(size_data);

	XSI::CICEAttributeDataArrayVector3f min_data;
	min_attribute.GetDataArray(min_data);

	XSI::CICEAttributeDataArrayVector3f max_data;
	max_attribute.GetDataArray(max_data);

	XSI::MATH::CVector3f size_value = size_data[0];
	XSI::MATH::CVector3f min_value = min_data[0];
	XSI::MATH::CVector3f max_value = max_data[0];

	m_size_x = (size_t)(std::max(size_value.GetX(), 0.0f) + 0.5);
	m_size_y = (size_t)(std::max(size_value.GetY(), 0.0f) + 0.5);
	m_size_z = (size_t)(std::max(size_value.GetZ(), 0.0f) + 0.5);

	m_min_x = min_value.GetX();
	m_min_y = min_value.GetY();
	m_min_z = min_value.GetZ();
	m_max_x = max_value.GetX();
	m_max_y = max_value.GetY();
	m_max_z = max_value.GetZ();

	m_is_empty = (m_size_x == 0 || m_size_y == 0 || m_size_z == 0);
}

ICEVolumeLoader::~ICEVolumeLoader()
{

}

bool ICEVolumeLoader::load_metadata(const ccl::ImageDeviceFeatures& features, ccl::ImageMetaData& metadata)
{
	if(m_attribute_type == VolumeAttributeType::VolumeAttributeType_Float)
	{
		metadata.channels = 1;
		metadata.type = ccl::IMAGE_DATA_TYPE_FLOAT;
	}
	else if (m_attribute_type == VolumeAttributeType::VolumeAttributeType_Vector)
	{
		metadata.channels = 3;
		metadata.type = ccl::IMAGE_DATA_TYPE_FLOAT4;
	}
	else
	{// color
		metadata.channels = 4;
		metadata.type = ccl::IMAGE_DATA_TYPE_FLOAT4;
	}
	metadata.width = m_size_x;
	metadata.height = m_size_y;
	metadata.depth = m_size_z;

	metadata.use_transform_3d = true;
	metadata.transform_3d =
		ccl::transform_translate(ccl::make_float3(0.5, 0.5, 0.5)) *
		ccl::transform_scale(ccl::make_float3(0.5, 0.5, 0.5)) *
		ccl::transform_scale(ccl::make_float3(2.0f / (m_max_x - m_min_x), 2.0f / (m_max_y - m_min_y), 2.0f / (m_max_z - m_min_z))) *
		ccl::transform_translate(ccl::make_float3(-(m_max_x + m_min_x) / 2.0f, -(m_max_y + m_min_y) / 2.0f, -(m_max_z + m_min_z) / 2.0f));
	return true;
}

bool ICEVolumeLoader::load_pixels(const ccl::ImageMetaData& metadata, void* pixels, const size_t pixels_size, const bool associate_alpha)
{
	if (m_attribute_type == VolumeAttributeType::VolumeAttributeType_Float)
	{
		XSI::CICEAttributeDataArray2DFloat float_array_data;
		XSI::CStatus read_status = m_xsi_attribute.GetDataArray2D(float_array_data);
		XSI::CICEAttributeDataArrayFloat float_data;
		float_array_data.GetSubArray(0, float_data);

		size_t floats_count = float_data.GetCount();
		if (read_status == XSI::CStatus::OK && floats_count == pixels_size)
		{
			float* fpixels = (float*)pixels;
			for (size_t i = 0; i < floats_count; i++)
			{
				*fpixels = float_data[i];
				fpixels++;
			}

			return true;
		}
	}
	else if(m_attribute_type == VolumeAttributeType::VolumeAttributeType_Vector)
	{
		XSI::CICEAttributeDataArray2DVector3f vector_array_data;
		XSI::CStatus read_status = m_xsi_attribute.GetDataArray2D(vector_array_data);
		XSI::CICEAttributeDataArrayVector3f vector_data;
		vector_array_data.GetSubArray(0, vector_data);

		size_t vectors_count = vector_data.GetCount();
		if (read_status == XSI::CStatus::OK && vectors_count * 3 == pixels_size)
		{
			float* fpixels = (float*)pixels;
			for (size_t i = 0; i < vectors_count; i++)
			{
				XSI::MATH::CVector3f vector = vector_data[i];
				fpixels[0] = vector.GetX();
				fpixels[1] = vector.GetY();
				fpixels[2] = vector.GetZ();
				fpixels += 3;
			}

			return true;
		}
	}
	else
	{// color
		XSI::CICEAttributeDataArray2DColor4f color_array_data;
		XSI::CStatus read_status = m_xsi_attribute.GetDataArray2D(color_array_data);
		XSI::CICEAttributeDataArrayColor4f color_data;
		color_array_data.GetSubArray(0, color_data);

		size_t colors_count = color_data.GetCount();
		if (read_status == XSI::CStatus::OK && colors_count * 4 == pixels_size)
		{
			float* fpixels = (float*)pixels;
			for (size_t i = 0; i < colors_count; i++)
			{
				XSI::MATH::CColor4f color = color_data[i];
				fpixels[0] = color.GetR();
				fpixels[1] = color.GetG();
				fpixels[2] = color.GetB();
				fpixels[3] = color.GetA();
				fpixels += 4;
			}

			return true;
		}
	}

	return false;
}

std::string ICEVolumeLoader::name() const
{
	return m_xsi_attribute_name.GetAsciiString();
}

bool ICEVolumeLoader::equals(const ImageLoader& other) const
{
	const ICEVolumeLoader& other_loader = (const ICEVolumeLoader&)other;

	return m_size_x == other_loader.get_size_x()
		&& m_size_y == other_loader.get_size_y()
		&& m_size_z == other_loader.get_size_z()
		&& m_min_x == other_loader.get_min_x()
		&& m_min_y == other_loader.get_min_y()
		&& m_min_z == other_loader.get_min_z()
		&& m_max_x == other_loader.get_max_x()
		&& m_max_y == other_loader.get_max_y()
		&& m_max_z == other_loader.get_max_z()
		&& m_xsi_attribute_name == other_loader.get_attribute_name()
		&& m_xsi_primitive_id == other_loader.get_primitive_id()
		&& m_attribute_type == other_loader.get_attribute_type();
}

void ICEVolumeLoader::cleanup()
{

}

bool ICEVolumeLoader::is_vdb_loader() const
{
	return false;
}

size_t ICEVolumeLoader::get_size_x() const { return m_size_x; }
size_t ICEVolumeLoader::get_size_y() const { return m_size_y; }
size_t ICEVolumeLoader::get_size_z() const { return m_size_z; }
float ICEVolumeLoader::get_min_x() const { return m_min_x; }
float ICEVolumeLoader::get_min_y() const { return m_min_y; }
float ICEVolumeLoader::get_min_z() const { return m_min_z; }
float ICEVolumeLoader::get_max_x() const { return m_max_x; }
float ICEVolumeLoader::get_max_y() const { return m_max_y; }
float ICEVolumeLoader::get_max_z() const { return m_max_z; }
XSI::CString ICEVolumeLoader::get_attribute_name() const { return m_xsi_attribute_name; }
ULONG ICEVolumeLoader::get_primitive_id() const { return m_xsi_primitive_id; }
VolumeAttributeType ICEVolumeLoader::get_attribute_type() const { return m_attribute_type; }
bool ICEVolumeLoader::is_empty() const
{
	return m_is_empty;
}