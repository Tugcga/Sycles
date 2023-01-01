#include "scene/pointcloud.h"
#include "scene/object.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_parameter.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>
#include <xsi_primitive.h>
#include <xsi_material.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_geometry.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "../../update_context.h"
#include "../../../utilities/xsi_properties.h"
#include "cyc_geometry.h"
#include "../cyc_scene.h"
#include "../../../utilities/math.h"

bool is_pointcloud_points(XSI::X3DObject &xsi_object, const XSI::CTime &eval_time)
{
	XSI::Property xsi_property;
	bool use_property = get_xsi_object_property(xsi_object, "CyclesPointcloud", xsi_property);

	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();
		bool primitive_pc = xsi_params.GetValue("primitive_pc", eval_time);

		return primitive_pc;
	}
	else
	{
		return false;
	}
}

void sync_points_geom(ccl::Scene* scene, ccl::PointCloud* points_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, bool use_motion_blur, ccl::vector<ccl::float3>& out_original_positions)
{
	XSI::CTime eval_time = update_context->get_time();

	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);
	XSI::ICEAttribute position_attr = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::CICEAttributeDataArrayVector3f position_data;
	position_attr.GetDataArray(position_data);

	XSI::ICEAttribute size_attribute = xsi_geometry.GetICEAttributeFromName("Size");
	XSI::CICEAttributeDataArrayFloat size_data;
	size_attribute.GetDataArray(size_data);

	ULONG num_points = position_data.GetCount();
	ULONG size_count = size_data.GetCount();
	points_geom->reserve(num_points);

	ccl::Attribute* attr_random = NULL;
	if (points_geom->need_attribute(scene, ccl::ATTR_STD_POINT_RANDOM))
	{
		attr_random = points_geom->attributes.add(ccl::ATTR_STD_POINT_RANDOM);
	}

	out_original_positions.resize(num_points);
	for (size_t i = 0; i < num_points; i++)
	{
		XSI::MATH::CVector3f position = position_data[i];
		float size = i < size_count ? size_data[i] : 0.0f;
		ccl::float3 position_float3 = vector3_to_float3(position);
		points_geom->add_point(position_float3, size);
		out_original_positions[i] = position_float3;
		if (attr_random != NULL)
		{
			attr_random->add(ccl::hash_uint2_to_float(i, 0));
		}
	}

	XSI::CRefArray attributes = xsi_geometry.GetICEAttributes();
	size_t attributes_count = attributes.GetCount();
	ccl::AttributeSet& cycles_attributes = points_geom->attributes;
	const ccl::AttributeElement element = ccl::ATTR_ELEMENT_VERTEX;
	for (size_t i = 0; i < attributes_count; i++)
	{
		XSI::ICEAttribute ice_attribute(attributes[i]);
		XSI::CString attr_name = ice_attribute.GetName();
		ccl::ustring name = ccl::ustring(attr_name.GetAsciiString());
		if (points_geom->need_attribute(scene, ccl::ustring(attr_name.GetAsciiString())))
		{
			if (cycles_attributes.find(name))
			{
				continue;
			}
			XSI::siICENodeContextType attr_context = ice_attribute.GetContextType();
			XSI::siICENodeDataType attr_data_type = ice_attribute.GetDataType();
			XSI::siICENodeStructureType attr_structure = ice_attribute.GetStructureType();
			if (attr_context == XSI::siICENodeContextComponent0D && attr_structure == XSI::siICENodeStructureSingle)
			{// one value per point
				if (attr_data_type == XSI::siICENodeDataFloat)
				{
					XSI::CICEAttributeDataArrayFloat attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeFloat, element);
					float* data = attr->data_float();
					for (size_t v = 0; v < num_points; v++)
					{
						data[v] = attr_data[v];
					}
				}
				else if (attr_data_type == XSI::siICENodeDataBool)
				{
					XSI::CICEAttributeDataArrayBool attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeFloat, element);
					float* data = attr->data_float();
					for (size_t v = 0; v < num_points; v++)
					{
						data[v] = attr_data[v] ? 1.0 : 0.0;
					}
				}
				else if (attr_data_type == XSI::siICENodeDataLong)
				{
					XSI::CICEAttributeDataArrayLong attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeFloat, element);
					float* data = attr->data_float();
					for (size_t v = 0; v < num_points; v++)
					{
						data[v] = attr_data[v];
					}
				}
				else if (attr_data_type == XSI::siICENodeDataVector3)
				{
					XSI::CICEAttributeDataArrayVector3f attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeVector, element);
					ccl::float3* data = attr->data_float3();
					for (size_t v = 0; v < num_points; v++)
					{
						XSI::MATH::CVector3f vector = attr_data[v];
						data[v] = ccl::make_float3(vector.GetX(), vector.GetY(), vector.GetZ());
					}
				}
				else if (attr_data_type == XSI::siICENodeDataColor4)
				{
					XSI::CICEAttributeDataArrayColor4f attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeRGBA, element);
					ccl::float4* data = attr->data_float4();
					for (size_t v = 0; v < num_points; v++)
					{
						XSI::MATH::CColor4f color = attr_data[v];
						data[v] = ccl::make_float4(srgb_to_linear(color.GetR()), srgb_to_linear(color.GetG()), srgb_to_linear(color.GetB()), color.GetA());
					}
				}
				else if (attr_data_type == XSI::siICENodeDataVector2)
				{
					XSI::CICEAttributeDataArrayVector2f attr_data;
					ice_attribute.GetDataArray(attr_data);
					ccl::Attribute* attr = cycles_attributes.add(name, ccl::TypeFloat2, element);
					ccl::float2* data = attr->data_float2();
					for (size_t v = 0; v < num_points; v++)
					{
						XSI::MATH::CVector2f vector = attr_data[v];
						data[v] = ccl::make_float2(vector.GetX(), vector.GetY());
					}
				}
			}
		}
	}
}

void sync_points_deform(ccl::PointCloud* points_geom, UpdateContext* update_context, const XSI::X3DObject& xsi_object, const std::vector<ccl::float3> &original_positions)
{
	size_t motion_steps = update_context->get_motion_steps();
	ULONG original_points_count = original_positions.size();
	points_geom->set_motion_steps(motion_steps);
	points_geom->set_use_motion_blur(true);

	size_t attribute_index = 0;
	ccl::Attribute* attr_m_positions = points_geom->attributes.add(ccl::ATTR_STD_MOTION_VERTEX_POSITION, ccl::ustring("std_motion_points_position"));
	ccl::float3* motion_positions = attr_m_positions->data_float3();
	MotionSettingsPosition motion_position = update_context->get_motion_position();
	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		size_t time_motion_step = mi;
		if (motion_position == MotionSettingsPosition::MotionPosition_Start)
		{
			time_motion_step++;
		}
		else if (motion_position == MotionSettingsPosition::MotionPosition_Center)
		{// center
			if (mi >= motion_steps / 2)
			{
				time_motion_step++;
			}
		}

		float time = update_context->get_motion_time(time_motion_step);
		XSI::Geometry time_xsi_geometry = xsi_object.GetActivePrimitive(time).GetGeometry(time);

		XSI::ICEAttribute position_attr = time_xsi_geometry.GetICEAttributeFromName("PointPosition");
		XSI::CICEAttributeDataArrayVector3f position_data;
		position_attr.GetDataArray(position_data);
		ULONG time_points_count = position_attr.GetElementCount();
		if (time_points_count == original_points_count)
		{
			for (size_t p = 0; p < original_points_count; p++)
			{
				motion_positions[attribute_index++] = vector3_to_float3(position_data[p]);
			}
		}
		else
		{
			// current points count differs from original one
			// copy original positions to time positions
			for (size_t p = 0; p < original_points_count; p++)
			{
				motion_positions[attribute_index++] = original_positions[p];
			}
		}
	}
}

void sync_points_geom_process(ccl::Scene* scene, ccl::PointCloud* points_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, bool motion_deform)
{
	ccl::vector<ccl::float3> original_positions;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	sync_points_geom(scene, points_geom, update_context, xsi_primitive, use_motion_blur, original_positions);

	if (use_motion_blur)
	{
		sync_points_deform(points_geom, update_context, xsi_object, original_positions);
	}
	else
	{
		points_geom->set_use_motion_blur(false);
	}

	original_positions.clear();
	original_positions.shrink_to_fit();
}

ccl::PointCloud* sync_points_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive(xsi_object.GetActivePrimitive(eval_time));
	ULONG xsi_primitive_id = xsi_primitive.GetObjectID();
	if (update_context->is_geometry_exists(xsi_primitive_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_primitive_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::POINTCLOUD)
		{
			return static_cast<ccl::PointCloud*>(scene->geometry[geo_index]);
		}
	}

	ccl::PointCloud* points_geom = scene->create_node<ccl::PointCloud>();

	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	points_geom->set_used_shaders(used_shaders);

	sync_points_geom_process(scene, points_geom, update_context, xsi_primitive, xsi_object, motion_deform);

	sync_transform(object, update_context, xsi_object.GetKinematics().GetGlobal());

	update_context->add_geometry_index(xsi_primitive_id, scene->geometry.size() - 1);

	return points_geom;
}

XSI::CStatus update_points(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::POINTCLOUD)
			{
				ccl::PointCloud* points_geom = static_cast<ccl::PointCloud*>(geometry);
				points_geom->clear(true);

				sync_points_geom_process(scene, points_geom, update_context, xsi_prim, xsi_object, motion_deform);

				points_geom->tag_update(scene, true);
			}
			else
			{
				return XSI::CStatus::Abort;
			}
		}
		else
		{
			return XSI::CStatus::Abort;
		}
	}
	else
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

XSI::CStatus update_points_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);
		}

		update_context->add_lightgroup(lightgroup);
	}
	else
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}