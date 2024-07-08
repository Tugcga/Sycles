#include <xsi_x3dobject.h>
#include <xsi_primitive.h>
#include <xsi_geometry.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "../../../render_base/type_enums.h"
#include "cyc_geometry.h"
#include "../cyc_scene.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/math.h"

bool is_em_fluid(const XSI::X3DObject &xsi_object, const XSI::CTime &eval_time)
{
	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);
	
	XSI::ICEAttribute emfluid_attribute = xsi_geometry.GetICEAttributeFromName("__emFluid5_Enable");
	return emfluid_attribute.IsValid();
}

bool is_explosia(const XSI::X3DObject& xsi_object, const XSI::CTime& eval_time)
{
	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);

	XSI::ICEAttribute efx_attribute = xsi_geometry.GetICEAttributeFromName("efx_vxsz");
	return efx_attribute.IsValid();
}

PointcloudType get_pointcloud_type(XSI::X3DObject &xsi_object, const XSI::CTime& eval_time)
{
	if (is_pointcloud_points(xsi_object, eval_time))
	{
		return PointcloudType::PointcloudType_Points;
	}
	else
	{
		bool is_emf = is_em_fluid(xsi_object, eval_time);
		if (!is_emf)
		{
			if (is_pointcloud_strands(xsi_object))
			{
				return PointcloudType::PointcloudType_Strands;
			}
			else if (is_pointcloud_instances(xsi_object, eval_time))
			{
				return PointcloudType::PointcloudType_Instances;
			}
			else if (is_pointcloud_volume(xsi_object, eval_time))
			{
				return PointcloudType::PointcloudType_Volume;
			}
			else
			{
				return PointcloudType::PointcloudType_Unknown;
			}
		}
		else
		{
			if (is_pointcloud_volume(xsi_object, eval_time))
			{
				return PointcloudType::PointcloudType_Volume;
			}
			else
			{
				return PointcloudType::PointcloudType_Unknown;
			}
		}
	}
}

bool is_pointcloud_instances(const XSI::X3DObject& xsi_object, const XSI::CTime& eval_time)
{
	XSI::Geometry xsi_geometry = xsi_object.GetActivePrimitive(eval_time).GetGeometry(eval_time);
	XSI::ICEAttribute shape_attribute = xsi_geometry.GetICEAttributeFromName("Shape");

	if (shape_attribute.IsValid() && shape_attribute.IsDefined())
	{
		XSI::CICEAttributeDataArrayShape shape_data;
		shape_attribute.GetDataArray(shape_data);

		// the number of shapes is not equal to the number of points
		// for example, if we create points and then emit particles from these points, then the number of shapes will be x2
		// but the first half has point shape (0)

		// also, if all shapes are point, then the coound of shapes = 1

		ULONG shapes_count = shape_data.GetCount();
		
		return shapes_count > 0;
	}

	return false;
}

bool is_valid_shape(XSI::siICEShapeType shape_type)
{
	// we have primitives: cube, plane, cylinder, sphere, cone, disc
	if (shape_type == XSI::siICEShapeReference || 
		shape_type == XSI::siICEShapeDisc ||
		shape_type == XSI::siICEShapeRectangle ||
		shape_type == XSI::siICEShapeSphere ||
		shape_type == XSI::siICEShapeBox ||
		shape_type == XSI::siICEShapeCylinder ||
		shape_type == XSI::siICEShapeCone)
	{
		return true;
	}
	else
	{
		return false;
	}
}

XSI::MATH::CTransformation build_point_transform(const XSI::MATH::CVector3f& position, const XSI::MATH::CRotationf& rotation, const XSI::MATH::CVector3f& size)
{
	XSI::MATH::CTransformation point_tfm;
	point_tfm.SetIdentity();
	point_tfm.SetTranslationFromValues(position.GetX(), position.GetY(), position.GetZ());
	float rot_x; float rot_y; float rot_z;
	rotation.GetXYZAngles(rot_x, rot_y, rot_z);
	point_tfm.SetRotationFromXYZAnglesValues(rot_x, rot_y, rot_z);
	point_tfm.SetScalingFromValues(size.GetX(), size.GetY(), size.GetZ());

	return point_tfm;
}

XSI::MATH::CTransformation build_point_transform(const XSI::MATH::CVector3f& position, const XSI::MATH::CRotationf& rotation, float size, const XSI::MATH::CVector3f& scale, bool is_scale_define)
{
	float scale_x = 1.0f;
	float scale_y = 1.0f;
	float scale_z = 1.0f;
	if (is_scale_define)
	{
		scale_x = scale.GetX();
		scale_y = scale.GetY();
		scale_z = scale.GetZ();
	}
	XSI::MATH::CVector3f size_vector(scale_x * size, scale_y * size, scale_z * size);

	return build_point_transform(position, rotation, size_vector);
}

std::vector<std::vector<XSI::MATH::CTransformation>> build_time_points_transforms(const XSI::X3DObject &xsi_object, const std::vector<double> &motion_times)
{
	size_t motion_times_count = motion_times.size();
	// get geometries for all times
	std::vector<XSI::Geometry> xsi_time_geometies(motion_times_count);
	for (size_t i = 0; i < motion_times_count; i++)
	{
		float time = motion_times[i];
		xsi_time_geometies[i] = xsi_object.GetActivePrimitive(time).GetGeometry(time);
	}

	std::vector<std::vector<XSI::MATH::CTransformation>> time_points_tfms(motion_times_count);
	for (size_t time_index = 0; time_index < motion_times_count; time_index++)
	{
		XSI::Geometry time_geometry = xsi_time_geometies[time_index];
		XSI::ICEAttribute time_shape_attribute = time_geometry.GetICEAttributeFromName("Shape");
		XSI::CICEAttributeDataArrayShape time_shape_data;
		time_shape_attribute.GetDataArray(time_shape_data);

		XSI::ICEAttribute time_position_attribute = time_geometry.GetICEAttributeFromName("PointPosition");
		XSI::CICEAttributeDataArrayVector3f time_position_data;
		time_position_attribute.GetDataArray(time_position_data);

		XSI::ICEAttribute time_orientation_attribute = time_geometry.GetICEAttributeFromName("Orientation");
		XSI::CICEAttributeDataArrayRotationf time_rotation_data;
		time_orientation_attribute.GetDataArray(time_rotation_data);

		XSI::ICEAttribute time_size_attribute = time_geometry.GetICEAttributeFromName("Size");
		XSI::CICEAttributeDataArrayFloat time_size_data;
		time_size_attribute.GetDataArray(time_size_data);

		XSI::ICEAttribute time_scale_attribute = time_geometry.GetICEAttributeFromName("Scale");
		XSI::CICEAttributeDataArrayVector3f time_scale_data;
		time_scale_attribute.GetDataArray(time_scale_data);

		size_t time_shape_data_count = time_shape_data.GetCount();
		int time_scale_count = time_scale_attribute.GetElementCount();
		bool is_time_scale_define = time_scale_attribute.IsDefined();

		// at this time the number of points can be different with respect to current time
		// but we will create array of arrays of transforms for each point at each time
		for (size_t time_i = 0; time_i < time_shape_data_count; time_i++)
		{
			XSI::siICEShapeType time_shape_type = time_shape_data[time_i].GetType();
			if (is_valid_shape(time_shape_type))
			{
				XSI::MATH::CTransformation time_point_tfm = build_point_transform(time_position_data[time_i], time_rotation_data[time_i], time_size_data[time_i], time_i < time_scale_count ? time_scale_data[time_i] : XSI::MATH::CVector3f(1.0f, 1.0f, 1.0f), is_time_scale_define);
				time_points_tfms[time_index].push_back(time_point_tfm);
			}
		}
	}

	return time_points_tfms;
}

void sync_point_primitive_shape(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::siICEShapeType shape_type, const XSI::MATH::CColor4f &color, const std::vector<XSI::MATH::CTransformation> &point_tfms, XSI::X3DObject &xsi_pointcloud, const XSI::CTime &eval_time)
{
	XSI::CString lightgroup = "";
	bool motion_deform = false;  // ignore this, primitives can move only as particles
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	sync_geometry_object_parameters(scene, object, xsi_pointcloud, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	// get material shader index
	XSI::Material xsi_material = xsi_pointcloud.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}
	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);

	ccl::Mesh* mesh = NULL;
	if (update_context->is_primitive_shape_exists(shape_type, shader_index))
	{
		mesh = static_cast<ccl::Mesh*>(scene->geometry[update_context->get_primitive_shape(shape_type, shader_index)]);
	}
	else
	{
		mesh = build_primitive(scene, shape_type);
		mesh->set_used_shaders(used_shaders);
		update_context->add_primitive_shape(shape_type, shader_index, scene->geometry.size() - 1);
	}

	object->set_geometry(mesh);
	// write object color
	object->set_color(color4_to_float3(color));
	object->set_alpha(color.GetA());

	// transforms
	sync_transforms(object, point_tfms, update_context->get_main_motion_step());
}