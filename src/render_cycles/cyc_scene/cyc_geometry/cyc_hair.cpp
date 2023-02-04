#include "scene/scene.h"
#include "scene/object.h"
#include "scene/hair.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_hairprimitive.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_floatarray.h>

#include "../../update_context.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../cyc_scene.h"
#include "cyc_geometry.h"

struct XsiHairUV
{
	std::vector<ccl::float2> data;
};

struct XsiHairVertexColor
{
	XSI::CString data_name;
	std::vector<ccl::float4> data;
};

struct XsiHairWeightMap
{
	XSI::CString data_name;
	std::vector<float> data;
};

void sync_hair_geom(ccl::Scene* scene, ccl::Hair* hair_geom, UpdateContext* update_context, const XSI::HairPrimitive &xsi_hair, bool use_motion_blur, ccl::vector<ccl::float4> &out_original_positions, LONG &out_num_keys)
{
	XSI::CTime eval_time = update_context->get_time();

	LONG hairs_count = xsi_hair.GetParameterValue("TotalHairs", eval_time);
	LONG strand_multiplicity = xsi_hair.GetParameterValue("StrandMult", eval_time);
	if (strand_multiplicity <= 1)
	{
		strand_multiplicity = 1;
	}
	hairs_count = hairs_count * strand_multiplicity;
	XSI::CRenderHairAccessor rha = xsi_hair.GetRenderHairAccessor(hairs_count);
	LONG num_uvs = rha.GetUVCount();
	LONG num_colors = rha.GetVertexColorCount();
	LONG num_weights = rha.GetWeightMapCount();
	LONG i = 0;

	// prepare attributes
	ccl::Attribute* attr_intercept = NULL;
	ccl::Attribute* attr_random = NULL;
	ccl::Attribute* attr_length = NULL;
	if (hair_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_INTERCEPT))
	{
		attr_intercept = hair_geom->attributes.add(ccl::ATTR_STD_CURVE_INTERCEPT);
	}
	if (hair_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_RANDOM))
	{
		attr_random = hair_geom->attributes.add(ccl::ATTR_STD_CURVE_RANDOM);
	}
	if (hair_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_LENGTH))
	{
		attr_length = hair_geom->attributes.add(ccl::ATTR_STD_CURVE_LENGTH);
	}

	// prepare store structures
	std::vector<XsiHairUV> uv_data;
	for (LONG uv_index = 0; uv_index < num_uvs; uv_index++)
	{
		XsiHairUV new_data;
		new_data.data.resize(0);
		uv_data.push_back(new_data);
	}
	std::vector<XsiHairVertexColor> color_data;
	for (LONG color_index = 0; color_index < num_colors; color_index++)
	{
		XsiHairVertexColor new_data;
		new_data.data.resize(0);
		color_data.push_back(new_data);
	}

	std::vector<XsiHairWeightMap> weight_data;
	for (LONG weight_index = 0; weight_index < num_weights; weight_index++)
	{
		XsiHairWeightMap new_data;
		new_data.data.resize(0);
		weight_data.push_back(new_data);
	}

	// get keys and curves data
	LONG num_curves = 0;
	while (rha.Next())
	{
		XSI::CLongArray vertices_count_array;
		rha.GetVerticesCount(vertices_count_array);
		LONG strands_count = vertices_count_array.GetCount();
		for (i = 0; i < strands_count; i++)
		{
			LONG n_count = vertices_count_array[i];
			out_num_keys += n_count;
		}
		num_curves += strands_count;
	}

	size_t original_index = 0;

	if (use_motion_blur)
	{
		out_original_positions.resize(out_num_keys);
	}

	hair_geom->reserve_curves(num_curves, out_num_keys);

	rha.Reset();
	// set hair data
	out_num_keys = 0;
	while (rha.Next())
	{
		XSI::CLongArray vertices_count_array;  // vertex count in each hair strand
		rha.GetVerticesCount(vertices_count_array);
		LONG strands_count = vertices_count_array.GetCount();
		XSI::CFloatArray pos_vals;
		rha.GetVertexPositions(pos_vals);  // actual vertex positions for all hairs
		XSI::CFloatArray rad_vals;
		rha.GetVertexRadiusValues(rad_vals);  // point radius
		LONG pos_k = 0;
		LONG radius_k = 0;
		// positions
		for (i = 0; i < strands_count; i++)
		{
			LONG n_count = vertices_count_array[i];
			float strand_length = 0.0f;
			for (LONG j = 0; j < n_count; j++)
			{
				hair_geom->add_curve_key(ccl::make_float3(pos_vals[pos_k], pos_vals[pos_k + 1], pos_vals[pos_k + 2]), rad_vals[radius_k]);
				if (use_motion_blur)
				{
					out_original_positions[original_index] = ccl::make_float4(pos_vals[pos_k], pos_vals[pos_k + 1], pos_vals[pos_k + 2], rad_vals[radius_k]);
					original_index++;
				}
				// increase strand length
				if (j > 0)
				{
					float dx = pos_vals[pos_k] - pos_vals[pos_k - 3];
					float dy = pos_vals[pos_k + 1] - pos_vals[pos_k - 2];
					float dz = pos_vals[pos_k + 2] - pos_vals[pos_k - 1];
					strand_length += sqrtf(dx * dx + dy * dy + dz * dz);
				}
				pos_k = pos_k + 3;
				radius_k = radius_k + 1;
				if (attr_intercept)
				{
					if (j == 0)
					{
						attr_intercept->add(0.0);
					}
					else
					{
						attr_intercept->add((float)j / (float)(n_count - 1));
					}
				}
			}
			if (attr_random != NULL)
			{
				attr_random->add(ccl::hash_uint2_to_float(i, 0));
			}
			if (attr_length != NULL)
			{
				attr_length->add(strand_length);
			}
			hair_geom->add_curve(out_num_keys, 0);
			out_num_keys += n_count;
		}

		// uvs
		for (i = 0; i < num_uvs; i++)
		{
			XSI::CFloatArray uv_vals;
			rha.GetUVValues(i, uv_vals);
			LONG uv_vals_count = uv_vals.GetCount();
			for (LONG j = 0; j < uv_vals_count; j += 3)
			{
				ccl::float2 new_uv_data = ccl::make_float2(uv_vals[j], uv_vals[j + 1]);
				uv_data[i].data.push_back(new_uv_data);
			}
			for (LONG j = uv_vals_count / 3; j < strands_count; j++)
			{
				uv_data[i].data.push_back(ccl::make_float2(0, 0));
			}
		}

		// vertex color
		for (i = 0; i < num_colors; i++)
		{
			XSI::CFloatArray color_values;
			rha.GetVertexColorValues(i, color_values);
			LONG colors_count = color_values.GetCount();
			XSI::CString colors_name = rha.GetVertexColorName(i);
			color_data[i].data_name = colors_name;
			for (LONG j = 0; j < colors_count; j += 4)
			{
				color_data[i].data.push_back(ccl::make_float4(color_values[j], color_values[j + 1], color_values[j + 2], 1.0));
			}
			for (LONG j = colors_count / 4; j < strands_count; j++)
			{
				color_data[i].data.push_back(ccl::make_float4(0, 0, 0, 1.0));
			}
		}

		// weight map
		for (i = 0; i < num_weights; i++)
		{
			XSI::CFloatArray weight_values;
			rha.GetWeightMapValues(i, weight_values);
			LONG weight_count = weight_values.GetCount();
			XSI::CString weight_name = rha.GetWeightMapName(i);
			weight_data[i].data_name = weight_name;
			for (LONG j = 0; j < weight_count; j++)
			{
				weight_data[i].data.push_back(weight_values[j]);
			}
			for (LONG j = weight_count; j < strands_count; j++)
			{
				weight_data[i].data.push_back(0);
			}
		}
	}

	// set attribute values
	for (size_t uv_set_index = 0; uv_set_index < uv_data.size(); uv_set_index++)
	{
		ccl::ustring attr_name = ccl::ustring("uv" + std::to_string(uv_set_index));
		if (hair_geom->need_attribute(scene, attr_name))
		{
			ccl::Attribute* new_uv_attribute = hair_geom->attributes.add(attr_name, ccl::TypeFloat2, ccl::ATTR_ELEMENT_CURVE);
			ccl::float2* attr_data = new_uv_attribute->data_float2();
			for (size_t d_index = 0; d_index < uv_data[uv_set_index].data.size(); d_index++)
			{
				*attr_data = uv_data[uv_set_index].data[d_index];
				attr_data++;
			}
		}
	}
	// default uv
	if (hair_geom->need_attribute(scene, ccl::ATTR_STD_UV) && uv_data.size() > 0)
	{
		ccl::Attribute* attr_uv = hair_geom->attributes.add(ccl::ATTR_STD_UV, ccl::ustring("std_uv"));
		ccl::float2* attr_data = attr_uv->data_float2();
		for (size_t d_index = 0; d_index < uv_data[0].data.size(); d_index++)
		{
			*attr_data = uv_data[0].data[d_index];
			attr_data++;
		}
	}

	// vertex color
	for (size_t color_index = 0; color_index < color_data.size(); color_index++)
	{
		ccl::ustring attr_name = ccl::ustring(color_data[color_index].data_name.GetAsciiString());
		if (hair_geom->need_attribute(scene, attr_name))
		{
			ccl::Attribute* color_attr = hair_geom->attributes.add(attr_name, ccl::TypeRGBA, ccl::ATTR_ELEMENT_CURVE);
			ccl::float4* attr_data = color_attr->data_float4();
			for (size_t d_index = 0; d_index < color_data[color_index].data.size(); d_index++)
			{
				*attr_data = color_data[color_index].data[d_index];
				attr_data++;
			}
		}
	}

	// weigh maps
	for (size_t weight_index = 0; weight_index < weight_data.size(); weight_index++)
	{
		ccl::ustring attr_name = ccl::ustring(weight_data[weight_index].data_name.GetAsciiString());
		if (hair_geom->need_attribute(scene, attr_name))
		{
			ccl::Attribute* weight_attr = hair_geom->attributes.add(attr_name, ccl::TypeFloat, ccl::ATTR_ELEMENT_CURVE);
			float* attr_data = weight_attr->data_float();
			for (size_t d_index = 0; d_index < weight_data[weight_index].data.size(); d_index++)
			{
				*attr_data = weight_data[weight_index].data[d_index];
				attr_data++;
			}
		}
	}

	// generate STD_GENERATED attribute
	ccl::Attribute* attr_generated = hair_geom->attributes.add(ccl::ATTR_STD_GENERATED);
	ccl::float3* generated = attr_generated->data_float3();

	for (size_t gen_i = 0; gen_i < hair_geom->num_curves(); gen_i++)
	{
		ccl::float3 co = hair_geom->get_curve_keys()[hair_geom->get_curve(gen_i).first_key];
		generated[gen_i] = co;
	}

	// clear temporary data
	for (size_t uv_index = 0; uv_index < uv_data.size(); uv_index++)
	{
		uv_data[uv_index].data.clear();
		uv_data[uv_index].data.shrink_to_fit();
	}
	uv_data.clear();
	uv_data.shrink_to_fit();

	for (size_t color_index = 0; color_index < color_data.size(); color_index++)
	{
		color_data[color_index].data.clear();
		color_data[color_index].data.shrink_to_fit();
	}
	color_data.clear();
	color_data.shrink_to_fit();

	for (size_t weight_index = 0; weight_index < weight_data.size(); weight_index++)
	{
		weight_data[weight_index].data.clear();
		weight_data[weight_index].data.shrink_to_fit();
	}
	weight_data.clear();
	weight_data.shrink_to_fit();
}

void sync_hair_motion_deform(ccl::Hair* hair, UpdateContext* update_context, const XSI::X3DObject &xsi_object, LONG num_keys, const ccl::vector<ccl::float4> &original_positions)
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
		else if(motion_position == MotionSettingsPosition::MotionPosition_Center)
		{// center
			if (mi >= motion_steps / 2)
			{
				time_motion_step++;
			}
		}

		float time = update_context->get_motion_time(time_motion_step);

		XSI::HairPrimitive time_primitive(xsi_object.GetActivePrimitive(time));
		LONG time_total_hairs = time_primitive.GetParameterValue("TotalHairs");
		LONG time_strands_mult = time_primitive.GetParameterValue("StrandMult");
		if (time_strands_mult <= 1)
		{
			time_strands_mult = 1;
		}
		time_total_hairs = time_total_hairs * time_strands_mult;
		XSI::CRenderHairAccessor time_rha = time_primitive.GetRenderHairAccessor(time_total_hairs);
		LONG time_keys_count = 0;
		while (time_rha.Next())
		{
			XSI::CLongArray time_vertex_count;
			time_rha.GetVerticesCount(time_vertex_count);
			LONG time_strands = time_vertex_count.GetCount();
			for (LONG i = 0; i < time_strands; i++)
			{
				LONG n_count = time_vertex_count[i];
				time_keys_count += n_count;
			}
		}

		time_rha.Reset();
		if (time_keys_count == num_keys)
		{
			while (time_rha.Next())
			{
				XSI::CLongArray time_vertices_count_array;
				time_rha.GetVerticesCount(time_vertices_count_array);
				LONG time_strands_count = time_vertices_count_array.GetCount();
				XSI::CFloatArray time_positions;
				time_rha.GetVertexPositions(time_positions);
				XSI::CFloatArray time_radiuses;
				time_rha.GetVertexRadiusValues(time_radiuses);
				LONG time_pos_key = 0;
				LONG time_rad_key = 0;
				// positions
				for (LONG i = 0; i < time_strands_count; i++)
				{
					LONG time_n_count = time_vertices_count_array[i];
					for (LONG j = 0; j < time_n_count; j++)
					{
						ccl::float4 val = ccl::make_float4(time_positions[time_pos_key], time_positions[time_pos_key + 1], time_positions[time_pos_key + 2], time_radiuses[time_rad_key]);
						motion_positions[attribute_index++] = val;
						time_pos_key = time_pos_key + 3;
						time_rad_key = time_rad_key + 1;
					}
				}
			}
		}
		else
		{// invalid data, the number of keys is nonequal to original
			for (size_t k_index = 0; k_index < original_positions.size(); k_index++)
			{
				motion_positions[attribute_index++] = original_positions[k_index];
			}
		}
	}
}

void sync_hair_geom_process(ccl::Scene* scene, ccl::Hair* hair_geom, UpdateContext* update_context, const XSI::HairPrimitive &xsi_hair, XSI::X3DObject &xsi_object, bool motion_deform)
{
	ccl::vector<ccl::float4> original_positions;  // save here positions for motion invalid attributes
	LONG num_keys = 0;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	sync_hair_geom(scene, hair_geom, update_context, xsi_hair, use_motion_blur, original_positions, num_keys);
	if (use_motion_blur)
	{
		sync_hair_motion_deform(hair_geom, update_context, xsi_object, num_keys, original_positions);
	}
	else
	{
		hair_geom->set_use_motion_blur(false);
	}

	original_positions.clear();
	original_positions.shrink_to_fit();
}


ccl::Hair* sync_hair_object(ccl::Scene* scene, ccl::Object* hair_object, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	// setup common object parameters
	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, hair_object, xsi_object, lightgroup, motion_deform, "CyclesHairs", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);
	
	// get or create hair geometry
	XSI::HairPrimitive xsi_hair(xsi_object.GetActivePrimitive(eval_time));
	ULONG xsi_hair_id = xsi_hair.GetObjectID();
	if (update_context->is_geometry_exists(xsi_hair_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_hair_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::HAIR)
		{
			return static_cast<ccl::Hair*>(scene->geometry[geo_index]);
		}
	}

	ccl::Hair* hair_geom = scene->create_node<ccl::Hair>();
	
	// set shaders
	// get id of the material
	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	// assign shaders
	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	hair_geom->set_used_shaders(used_shaders);

	// sync hair
	sync_hair_geom_process(scene, hair_geom, update_context, xsi_hair, xsi_object, motion_deform);

	// add to the update context
	update_context->add_geometry_index(xsi_hair_id, scene->geometry.size() - 1);

	return hair_geom;
}

XSI::CStatus update_hair(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::HairPrimitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();
	
	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		// update object properties for all instances
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesHairs", render_parameters, eval_time, false);  // at update ignore color reasign
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::HAIR)
			{
				ccl::Hair* hair_geom = static_cast<ccl::Hair*>(geometry);
				hair_geom->clear(true);

				sync_hair_geom_process(scene, hair_geom, update_context, xsi_prim, xsi_object, motion_deform);

				bool rebuild = hair_geom->curve_keys_is_modified() || hair_geom->curve_radius_is_modified();
				hair_geom->tag_update(scene, rebuild);
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

XSI::CStatus update_hair_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	ULONG xsi_object_id = xsi_object.GetObjectID();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::CTime eval_time = update_context->get_time();

	if (update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesHairs", render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}