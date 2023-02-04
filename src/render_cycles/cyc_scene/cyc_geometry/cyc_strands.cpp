#include "scene/scene.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_material.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "../../update_context.h"
#include "cyc_geometry.h"
#include "../cyc_scene.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/xsi_properties.h"

bool is_pointcloud_strands(const XSI::X3DObject& xsi_object)
{
	XSI::Geometry xsi_geometry = xsi_object.GetActivePrimitive().GetGeometry();
	XSI::ICEAttribute strand_count_attr = xsi_geometry.GetICEAttributeFromName("StrandCount");
	XSI::ICEAttribute strand_position_attr = xsi_geometry.GetICEAttributeFromName("StrandPosition");
	XSI::ICEAttribute point_position_attr = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::CICEAttributeDataArray2DVector3f strand_position_data;
	strand_position_attr.GetDataArray2D(strand_position_data);

	if (strand_position_data.GetCount() > 0 && strand_count_attr.GetElementCount() > 0 && strand_position_attr.GetElementCount() > 0 && point_position_attr.GetElementCount() > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void sync_strands_geom(ccl::Scene* scene, ccl::Hair* strands_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, bool use_motion_blur, float tip_prop, ccl::vector<ccl::float4>& out_original_positions, LONG& out_num_curves)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);

	// get strands data from ICE
	XSI::ICEAttribute point_position_attr = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::ICEAttribute strand_position_atr = xsi_geometry.GetICEAttributeFromName("StrandPosition");
	XSI::ICEAttribute size_atr = xsi_geometry.GetICEAttributeFromName("Size");
	// fill the data
	XSI::CICEAttributeDataArrayVector3f point_position_data;
	point_position_attr.GetDataArray(point_position_data);

	XSI::CICEAttributeDataArray2DVector3f strand_position_data;
	strand_position_atr.GetDataArray2D(strand_position_data);

	XSI::CICEAttributeDataArrayFloat size_data;
	size_atr.GetDataArray(size_data);

	XSI::CICEAttributeDataArrayVector3f one_strand_data;
	strand_position_data.GetSubArray(0, one_strand_data);

	ULONG curves_count = point_position_data.GetCount();
	ULONG keys_count = one_strand_data.GetCount();

	out_num_curves = curves_count;

	strands_geom->reserve_curves(curves_count, curves_count * (keys_count + 1));
	ccl::Attribute* attr_intercept = NULL;
	ccl::Attribute* attr_random = NULL;
	ccl::Attribute* attr_length = NULL;
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_INTERCEPT))
	{
		attr_intercept = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_INTERCEPT);
	}
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_RANDOM))
	{
		attr_random = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_RANDOM);
	}
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_LENGTH))
	{
		attr_length = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_LENGTH);
	}
	ULONG original_index = 0;
	
	if (use_motion_blur)
	{
		out_original_positions.resize(curves_count * (keys_count + 1));
	}

	for (size_t curve_index = 0; curve_index < curves_count; curve_index++)
	{
		strands_geom->add_curve_key(vector3_to_float3(point_position_data[curve_index]), size_data[curve_index]);
		if (use_motion_blur)
		{
			out_original_positions[original_index] = ccl::make_float4(point_position_data[curve_index].GetX(), point_position_data[curve_index].GetY(), point_position_data[curve_index].GetZ(), size_data[curve_index]);
			original_index++;
		}
		if (attr_intercept)
		{
			attr_intercept->add(0.0);
		}

		strand_position_data.GetSubArray(curve_index, one_strand_data);
		float total_time = one_strand_data.GetCount() + 1;
		float strand_length = 0.0f;
		for (ULONG k_index = 0; k_index < one_strand_data.GetCount(); k_index++)
		{
			float time = static_cast<float>(k_index + 1) / total_time;
			float radius = (1 - time) * size_data[curve_index] + time * size_data[curve_index] * tip_prop;
			if (k_index + 1 == one_strand_data.GetCount() && tip_prop < 0.0001f)
			{
				radius = 0.0;
			}
			strands_geom->add_curve_key(vector3_to_float3(one_strand_data[k_index]), radius);
			if (k_index > 0)
			{
				float dx = one_strand_data[k_index].GetX() - one_strand_data[k_index - 1].GetX();
				float dy = one_strand_data[k_index].GetY() - one_strand_data[k_index - 1].GetY();
				float dz = one_strand_data[k_index].GetZ() - one_strand_data[k_index - 1].GetZ();
				strand_length += sqrtf(dx * dx + dy * dy + dz * dz);
			}

			if (use_motion_blur)
			{
				out_original_positions[original_index] = ccl::make_float4(one_strand_data[k_index].GetX(), one_strand_data[k_index].GetY(), one_strand_data[k_index].GetZ(), radius);
				original_index++;
			}

			if (attr_intercept)
			{
				attr_intercept->add(time);
			}
		}
		if (attr_random != NULL)
		{
			attr_random->add(ccl::hash_uint2_to_float(curve_index, 0));
		}
		if (attr_length != NULL)
		{
			attr_length->add(strand_length);
		}
		strands_geom->add_curve(curve_index * (one_strand_data.GetCount() + 1), 0);
	}

	ccl::Attribute* attr_generated = strands_geom->attributes.add(ccl::ATTR_STD_GENERATED);
	ccl::float3* generated = attr_generated->data_float3();
	for (size_t i = 0; i < strands_geom->num_curves(); i++)
	{
		ccl::float3 co = strands_geom->get_curve_keys()[strands_geom->get_curve(i).first_key];
		generated[i] = co;
	}

	// add ICE attributes
	// each attribute is per curve
	XSI::CRefArray xsi_ice_attributes = xsi_geometry.GetICEAttributes();

	for (LONG i = 0; i < xsi_ice_attributes.GetCount(); i++)
	{
		XSI::ICEAttribute xsi_attribute(xsi_ice_attributes[i]);
		XSI::CString xsi_attr_name = xsi_attribute.GetName();
		ccl::ustring attr_name = ccl::ustring(xsi_attr_name.GetAsciiString());

		if (strands_geom->need_attribute(scene, attr_name))
		{
			XSI::siICENodeContextType attr_context = xsi_attribute.GetContextType();
			XSI::siICENodeStructureType attr_structure = xsi_attribute.GetStructureType();

			if (attr_context == XSI::siICENodeContextComponent0D && attr_structure == XSI::siICENodeStructureSingle)
			{
				XSI::siICENodeDataType attr_data_type = xsi_attribute.GetDataType();

				if (attr_data_type == XSI::siICENodeDataVector3)
				{
					XSI::CICEAttributeDataArrayVector3f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeVector, ccl::ATTR_ELEMENT_CURVE);

					ccl::float3* cyc_attr_data = cycles_attribute->data_float3();
					for (size_t v_index = 0; v_index < curves_count; v_index++)
					{
						*cyc_attr_data = vector3_to_float3(attr_data[v_index]);
						cyc_attr_data++;
					}
				}
				else if (attr_data_type == XSI::siICENodeDataColor4)
				{
					XSI::CICEAttributeDataArrayColor4f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeColor, ccl::ATTR_ELEMENT_CURVE);

					ccl::float4* cyc_attr_data = cycles_attribute->data_float4();
					for (size_t v_index = 0; v_index < curves_count; v_index++)
					{
						*cyc_attr_data = color4_to_float4(attr_data[v_index]);
						cyc_attr_data++;
					}
				}
				else if (attr_data_type == XSI::siICENodeDataFloat)
				{
					XSI::CICEAttributeDataArrayFloat attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeFloat, ccl::ATTR_ELEMENT_CURVE);

					float* cyc_attr_data = cycles_attribute->data_float();
					for (size_t v_index = 0; v_index < curves_count; v_index++)
					{
						*cyc_attr_data = attr_data[v_index];
						cyc_attr_data++;
					}
				}
			}
		}
	}
}

void sync_strands_deform(ccl::Hair* hair, UpdateContext* update_context, const XSI::X3DObject &xsi_object, LONG num_curves, float tip_prop, const ccl::vector<ccl::float4>& original_positions)
{
	size_t motion_steps = update_context->get_motion_steps();
	hair->set_motion_steps(motion_steps);
	hair->set_use_motion_blur(true);

	size_t attribute_index = 0;
	ccl::Attribute* attr_m_positions = hair->attributes.add(ccl::ATTR_STD_MOTION_VERTEX_POSITION, ccl::ustring("std_motion_strand_position"));
	ccl::float4* motion_positions = attr_m_positions->data_float4();
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
		XSI::Geometry time_xsi_geometry = xsi_object.GetActivePrimitive(time).GetGeometry();

		XSI::ICEAttribute time_pos = time_xsi_geometry.GetICEAttributeFromName("PointPosition");
		XSI::ICEAttribute time_strand = time_xsi_geometry.GetICEAttributeFromName("StrandPosition");
		XSI::ICEAttribute time_size = time_xsi_geometry.GetICEAttributeFromName("Size");
		if (time_pos.GetElementCount() > 0 && time_strand.GetElementCount() > 0 && time_size.GetElementCount() > 0)
		{// attributes exists, get the data
			XSI::CICEAttributeDataArrayVector3f time_pos_data;
			XSI::CICEAttributeDataArray2DVector3f time_strand_data;
			XSI::CICEAttributeDataArrayFloat time_size_data;
			time_pos.GetDataArray(time_pos_data);
			time_strand.GetDataArray2D(time_strand_data);
			time_size.GetDataArray(time_size_data);
			// iterate throw strands
			for (size_t c_index = 0; c_index < num_curves; c_index++)
			{
				XSI::MATH::CVector3f p = time_pos_data[c_index];
				float s = time_size_data[c_index];
				motion_positions[attribute_index++] = ccl::make_float4(p.GetX(), p.GetY(), p.GetZ(), s);
				// next keys for strand positions
				XSI::CICEAttributeDataArrayVector3f strand;
				time_strand_data.GetSubArray(c_index, strand);
				float strand_prop_length = strand.GetCount() + 1;
				for (ULONG k_index = 0; k_index < strand.GetCount(); k_index++)
				{
					float prop = static_cast<float>(k_index + 1) / strand_prop_length;
					s = (1 - prop) * time_size_data[c_index] + prop * time_size_data[c_index] * tip_prop;
					if (k_index == strand.GetCount() - 1 && tip_prop < 0.0001f)
					{
						s = 0.0;
					}
					p = strand[k_index];
					motion_positions[attribute_index++] = ccl::make_float4(p.GetX(), p.GetY(), p.GetZ(), s);
				}
			}
		}
		else
		{// invald attribute at the time, set default positions
			for (size_t k_index = 0; k_index < original_positions.size(); k_index++)
			{
				motion_positions[attribute_index++] = original_positions[k_index];
			}
		}
	}
}

void sync_strands_property(XSI::X3DObject& xsi_object, float& io_tip_prop, const XSI::CTime &eval_time)
{
	XSI::Property xsi_property;
	bool use_property = get_xsi_object_property(xsi_object, "CyclesPointcloud", xsi_property);
	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		io_tip_prop = xsi_params.GetValue("tip_prop", eval_time);
	}
}

// in this function we actually create geometry
// and also define motion deform, if we need it
void sync_strands_geom_process(ccl::Scene* scene, ccl::Hair* strands_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, bool motion_deform)
{
	ccl::vector<ccl::float4> original_positions;
	LONG num_curves = 0;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	float tip_prop = 0.0f;
	sync_strands_property(xsi_object, tip_prop, update_context->get_time());

	sync_strands_geom(scene, strands_geom, update_context, xsi_primitive, use_motion_blur, tip_prop, original_positions, num_curves);

	if (use_motion_blur)
	{
		sync_strands_deform(strands_geom, update_context, xsi_object, num_curves, tip_prop, original_positions);
	}
	else
	{
		strands_geom->set_use_motion_blur(false);
	}

	original_positions.clear();
	original_positions.shrink_to_fit();
}

// here we setup object, create or assign geometry, setup transform (with object motion)
ccl::Hair* sync_strands_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject &xsi_object)
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
		if (cyc_geo->geometry_type == ccl::Geometry::Type::HAIR)
		{
			return static_cast<ccl::Hair*>(scene->geometry[geo_index]);
		}
	}

	ccl::Hair* hair_geom = scene->create_node<ccl::Hair>();

	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	hair_geom->set_used_shaders(used_shaders);

	sync_strands_geom_process(scene, hair_geom, update_context, xsi_primitive, xsi_object, motion_deform);

	update_context->add_geometry_index(xsi_primitive_id, scene->geometry.size() - 1);

	return hair_geom;
}

XSI::CStatus update_strands(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
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

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::HAIR)
			{
				ccl::Hair* strands_geom = static_cast<ccl::Hair*>(geometry);
				strands_geom->clear(true);

				sync_strands_geom_process(scene, strands_geom, update_context, xsi_prim, xsi_object, motion_deform);

				bool rebuild = strands_geom->curve_keys_is_modified() || strands_geom->curve_radius_is_modified();
				strands_geom->tag_update(scene, rebuild);
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