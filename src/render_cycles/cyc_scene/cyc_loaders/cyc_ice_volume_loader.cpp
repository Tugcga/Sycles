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

#include "openvdb/tools/Dense.h"
#include <openvdb/openvdb.h>
#include "util/openvdb.h"

const float EPSILON = 0.0001f;

ICEVDBLoader::ICEVDBLoader(VolumeAttributeType attribute_type, const XSI::Primitive& xsi_primitive, const std::string& attribute_name, size_t update_generation, const XSI::CTime& eval_time) : ccl::VDBImageLoader(attribute_name) {
	m_attribute_name = XSI::CString(attribute_name.c_str());
	m_primitive_id = xsi_primitive.GetObjectID();
	m_generation = update_generation;

	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);
	XSI::ICEAttribute xsi_attribute = xsi_geometry.GetICEAttributeFromName(m_attribute_name);

	// get size and corners attributes
	XSI::ICEAttribute size_attribute = xsi_geometry.GetICEAttributeFromName(m_attribute_name + "_size");
	XSI::ICEAttribute min_attribute = xsi_geometry.GetICEAttributeFromName(m_attribute_name + "_min");
	XSI::ICEAttribute max_attribute = xsi_geometry.GetICEAttributeFromName(m_attribute_name + "_max");

	XSI::CICEAttributeDataArrayVector3f size_data;
	size_attribute.GetDataArray(size_data);

	XSI::CICEAttributeDataArrayVector3f min_data;
	min_attribute.GetDataArray(min_data);

	XSI::CICEAttributeDataArrayVector3f max_data;
	max_attribute.GetDataArray(max_data);

	XSI::MATH::CVector3f size_value = size_data[0];
	XSI::MATH::CVector3f min_value = min_data[0];
	XSI::MATH::CVector3f max_value = max_data[0];


	if (size_value.GetX() < EPSILON || size_value.GetY() < EPSILON || size_value.GetZ() < EPSILON) {
		m_is_empty = true;
	}
	else {
		m_is_empty = false;

		const openvdb::CoordBBox dense_bbox(0, 0, 0, (int)size_value.GetX() - 1, (int)size_value.GetY() - 1, (int)size_value.GetZ() - 1);
		size_t voxels_count = (int)size_value.GetX() * (int)size_value.GetY() * (int)size_value.GetZ();
		ccl::Transform transform_3d =
			ccl::transform_translate(ccl::make_float3(0.5, 0.5, 0.5)) *
			ccl::transform_scale(ccl::make_float3(0.5, 0.5, 0.5)) *
			ccl::transform_scale(ccl::make_float3(2.0f / (max_value.GetX() - min_value.GetX()), 2.0f / (max_value.GetY() - min_value.GetY()), 2.0f / (max_value.GetZ() - min_value.GetZ()))) *
			ccl::transform_translate(ccl::make_float3(-(max_value.GetX() + min_value.GetX()) / 2.0f, -(max_value.GetY() + min_value.GetY()) / 2.0f, -(max_value.GetZ() + min_value.GetZ()) / 2.0f));
		const ccl::float3 voxel_size = ccl::make_float3(1.0f / size_value.GetX(), 1.0f / size_value.GetY(), 1.0f / size_value.GetZ());

		transform_3d = transform_inverse(transform_3d);
		const openvdb::Mat4R index_to_world_mat((double)(voxel_size.x * transform_3d[0][0]),
			0.0,
			0.0,
			0.0,
			0.0,
			(double)(voxel_size.y * transform_3d[1][1]),
			0.0,
			0.0,
			0.0,
			0.0,
			(double)(voxel_size.z * transform_3d[2][2]),
			0.0,
			(double)transform_3d[0][3],
			(double)transform_3d[1][3],
			(double)transform_3d[2][3],
			1.0);
		const openvdb::math::Transform::Ptr index_to_world_tfm = openvdb::math::Transform::createLinearTransform(index_to_world_mat);

		if (attribute_type == VolumeAttributeType_Float) {
			openvdb::FloatGrid::Ptr sparse = openvdb::FloatGrid::create(openvdb::FloatGrid::ValueType(0.0f));

			XSI::CICEAttributeDataArray2DFloat float_array_data;
			XSI::CStatus read_status = xsi_attribute.GetDataArray2D(float_array_data);
			XSI::CICEAttributeDataArrayFloat float_data;
			float_array_data.GetSubArray(0, float_data);
			if (float_data.GetCount() != voxels_count) {
				m_is_empty = true;
				grid = sparse;
			}
			else {
				const openvdb::tools::Dense<const openvdb::FloatGrid::ValueType, openvdb::tools::MemoryLayout::LayoutXYZ> dense(dense_bbox, reinterpret_cast<const openvdb::FloatGrid::ValueType*>(&float_data[0]));
				openvdb::tools::copyFromDense(dense, *sparse, openvdb::FloatGrid::ValueType(clipping));

				sparse->setTransform(index_to_world_tfm);
				grid = sparse;
			}
		}
		else if (attribute_type == VolumeAttributeType_Vector) {
			openvdb::Vec3fGrid::Ptr sparse = openvdb::Vec3fGrid::create();

			XSI::CICEAttributeDataArray2DVector3f vector_array_data;
			XSI::CStatus read_status = xsi_attribute.GetDataArray2D(vector_array_data);
			XSI::CICEAttributeDataArrayVector3f vector_data;
			vector_array_data.GetSubArray(0, vector_data);
			if (vector_data.GetCount() != voxels_count) {
				m_is_empty = true;
				grid = sparse;
			}
			else {
				const openvdb::tools::Dense<const openvdb::Vec3fGrid::ValueType, openvdb::tools::MemoryLayout::LayoutXYZ> dense(dense_bbox, reinterpret_cast<const openvdb::Vec3fGrid::ValueType*>(&vector_data[0]));
				openvdb::tools::copyFromDense(dense, *sparse, openvdb::Vec3fGrid::ValueType(clipping));

				sparse->setTransform(index_to_world_tfm);
				grid = sparse;
			}
		}
		else if (attribute_type == VolumeAttributeType_Color) {
			openvdb::Vec4fGrid::Ptr sparse = openvdb::Vec4fGrid::create();

			XSI::CICEAttributeDataArray2DColor4f color_array_data;
			XSI::CStatus read_status = xsi_attribute.GetDataArray2D(color_array_data);
			XSI::CICEAttributeDataArrayColor4f color_data;
			color_array_data.GetSubArray(0, color_data);
			if (color_data.GetCount() != voxels_count) {
				m_is_empty = true;
				grid = sparse;
			}
			else {
				const openvdb::tools::Dense<const openvdb::Vec4fGrid::ValueType, openvdb::tools::MemoryLayout::LayoutXYZ> dense(dense_bbox, reinterpret_cast<const openvdb::Vec4fGrid::ValueType*>(&color_data[0]));
				openvdb::tools::copyFromDense(dense, *sparse, openvdb::Vec4fGrid::ValueType(clipping));

				sparse->setTransform(index_to_world_tfm);
				grid = sparse;
			}
		}
	}
}

ICEVDBLoader::~ICEVDBLoader() {

}

std::string ICEVDBLoader::name() const {
	return m_attribute_name.GetAsciiString();
}

bool ICEVDBLoader::equals(const ImageLoader& other) const {
	const ICEVDBLoader& other_loader = (const ICEVDBLoader&)other;
	return m_primitive_id == other_loader.m_primitive_id && m_attribute_name == other_loader.m_attribute_name && m_generation == other_loader.m_generation;
}

bool ICEVDBLoader::is_empty() {
	return m_is_empty;
}