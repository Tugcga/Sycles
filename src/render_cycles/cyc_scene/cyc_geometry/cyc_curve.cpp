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
#include <xsi_nurbscurvelist.h>
#include <xsi_nurbscurve.h>

#include "../../update_context.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/strings.h"
#include "../cyc_scene.h"
#include "cyc_geometry.h"

void sync_curve_motion_deform(ccl::Hair* curves, UpdateContext* update_context, const XSI::X3DObject& xsi_object, float curve_size, float sample_step, int curve_samples)
{
	size_t motion_steps = update_context->get_motion_steps();
	curves->set_motion_steps(motion_steps);
	curves->set_use_motion_blur(true);

	ccl::Attribute* attr_position = curves->attributes.find(ccl::ATTR_STD_POSITION);
	ccl::Attribute* attr_radius = curves->attributes.find(ccl::ATTR_STD_RADIUS);

	attr_position->add_motion(curves);
	attr_radius->add_motion(curves);

	MotionSettingsPosition motion_position = update_context->get_motion_position();
	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		ccl::packed_float3* position_ptr = attr_position->data_for_write<ccl::packed_float3>(mi + 1);
		float* radius_ptr = attr_radius->data_for_write<float>(mi + 1);
		size_t attribute_index = 0;

		size_t time_motion_step = calc_time_motion_step(mi, motion_steps, motion_position);
		
		float time = update_context->get_motion_time(time_motion_step);

		XSI::Primitive time_primitive(xsi_object.GetActivePrimitive(time));
		XSI::NurbsCurveList time_curve_geometry = time_primitive.GetGeometry(time);
		XSI::CNurbsCurveRefArray time_curves = time_curve_geometry.GetCurves();
		size_t time_curves_count = time_curves.GetCount();
		for (size_t i = 0; i < time_curves_count; i++) {
			XSI::NurbsCurve time_curve = time_curves.GetItem(i);

			for (size_t s = 0; s < curve_samples; s++) {
				XSI::MATH::CVector3 position;
				XSI::MATH::CVector3 tangent;
				XSI::MATH::CVector3 normal;
				XSI::MATH::CVector3 binormal;

				time_curve.EvaluateNormalizedPosition((float)s * sample_step, position, tangent, normal, binormal);
				ccl::float4 val = ccl::make_float4(position.GetX(), position.GetY(), position.GetZ(), curve_size);
				position_ptr[attribute_index] = ccl::make_float3(val.x, val.y, val.z);
				radius_ptr[attribute_index] = val.w;

				attribute_index++;
			}
		}
	}

	curves->tag_position_modified();
	curves->tag_radius_modified();
	curves->tag_motion_steps_modified();
}

void sync_curve_geom(ccl::Scene* scene, ccl::Hair* curve_geom, UpdateContext* update_context, const XSI::CNurbsCurveRefArray& xsi_curves, float curve_size, float sample_step, int curve_samples) {
	XSI::CTime eval_time = update_context->get_time();
	
	ULONG curves_count = xsi_curves.GetCount();

	curve_geom->resize_curves(curves_count, curves_count * curve_samples);
	ccl::Attribute* attr_position = curve_geom->attributes.find(ccl::ATTR_STD_POSITION);
	ccl::Attribute* attr_radius = curve_geom->attributes.find(ccl::ATTR_STD_RADIUS);
	ccl::packed_float3* position_ptr = attr_position->data_for_write<ccl::packed_float3>();
	float* radius_ptr = attr_radius->data_for_write<float>();
	int* first_key_data = curve_geom->get_curve_first_key().data();
	int* shader_data = curve_geom->get_curve_shader().data();

	ccl::Attribute* attr_intercept = NULL;
	ccl::Attribute* attr_random = NULL;
	ccl::Attribute* attr_length = NULL;

	if (curve_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_INTERCEPT)) {
		attr_intercept = curve_geom->attributes.add(ccl::ATTR_STD_CURVE_INTERCEPT);
	}
	if (curve_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_RANDOM)) {
		attr_random = curve_geom->attributes.add(ccl::ATTR_STD_CURVE_RANDOM);
	}
	if (curve_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_LENGTH)) {
		attr_length = curve_geom->attributes.add(ccl::ATTR_STD_CURVE_LENGTH);
	}

	size_t num_keys = 0;
	for (size_t i = 0; i < curves_count; i++) {
		first_key_data[i] = num_keys;

		XSI::NurbsCurve xsi_curve = xsi_curves.GetItem(i);

		for (size_t s = 0; s < curve_samples; s++) {
			XSI::MATH::CVector3 position;
			XSI::MATH::CVector3 tangent;
			XSI::MATH::CVector3 normal;
			XSI::MATH::CVector3 binormal;

			xsi_curve.EvaluateNormalizedPosition((float)s * sample_step, position, tangent, normal, binormal);
			position_ptr[num_keys] = vector3_to_float3(position);
			radius_ptr[num_keys] = curve_size;

			if (attr_intercept) {
				float* intercept_ptr = attr_intercept->data_for_write<float>();
				intercept_ptr[num_keys] = (float)s / (float)(curve_samples - 1);
			}

			num_keys += 1;
		}

		if (attr_random != NULL) {
			float* randomt_ptr = attr_random->data_for_write<float>();
			randomt_ptr[i] = ccl::hash_uint2_to_float(i, 0);
		}
		if (attr_length != NULL) {
			double curve_length;
			xsi_curve.GetLength(curve_length);
			float* length_ptr = attr_length->data_for_write<float>();
			length_ptr[i] = curve_length;
		}
		shader_data[i] = 0;
	}

	// Check that no std generated attribute
	ccl::Attribute* attr_generated = curve_geom->attributes.add(ccl::ATTR_STD_GENERATED);
	ccl::packed_float3* generated = attr_generated->data_for_write<ccl::packed_float3>();

	for (size_t gen_i = 0; gen_i < curve_geom->num_curves(); gen_i++)
	{
		ccl::float3 co = position_ptr[first_key_data[gen_i]];
		generated[gen_i] = co;
	}

	curve_geom->tag_position_modified();
	curve_geom->tag_radius_modified();
	curve_geom->tag_curve_first_key_modified();
}

void sync_curve_geom_process(ccl::Scene* scene, ccl::Hair* curve_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, const XSI::Property& curve_property, bool motion_deform) {
	curve_geom->name = combine_geometry_name(xsi_object, xsi_primitive).GetAsciiString();

	LONG num_keys = 0;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	XSI::CTime eval_time = update_context->get_time();
	XSI::NurbsCurveList xsi_curve_geometry = xsi_primitive.GetGeometry(eval_time);
	XSI::CNurbsCurveRefArray xsi_curves = xsi_curve_geometry.GetCurves();

	float curve_size = curve_property.GetParameterValue("curve_size", eval_time);
	int curve_samples = curve_property.GetParameterValue("curve_samples", eval_time);
	float sample_step = 1.0f / (float)curve_samples;
	curve_samples = std::max(curve_samples, 1) + 1;

	sync_curve_geom(scene, curve_geom, update_context, xsi_curves, curve_size, sample_step, curve_samples);
	if (use_motion_blur)
	{
		sync_curve_motion_deform(curve_geom, update_context, xsi_object, curve_size, sample_step, curve_samples);
	}
	else
	{
		curve_geom->set_use_motion_blur(false);
	}
}

ccl::Hair* sync_curve_object(ccl::Scene* scene, ccl::Object* curve_object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::Property& curve_property)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, curve_object, xsi_object, lightgroup, motion_deform, "CyclesCurve", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	ULONG xsi_curve_id = xsi_primitive.GetObjectID();
	if (update_context->is_geometry_exists(xsi_curve_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_curve_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::HAIR)
		{
			return static_cast<ccl::Hair*>(scene->geometry[geo_index]);
		}
	}

	ccl::Hair* curve_geom = scene->create_node<ccl::Hair>();

	XSI::CString curve_mat_iddentificator = curve_property.GetParameterValue("curve_material", eval_time);
	// curve_mat_id is a string of the form: library.materialName
	// we should get actual material ID from this name and theck it
	ULONG curve_mat_id = 0;
	bool is_get = get_material_id_from_name(curve_mat_iddentificator, curve_mat_id);
	XSI::CStatus valid_material;
	if (is_get) {
		if (!update_context->is_material_exists(curve_mat_id)) {
			// material is not exported
			// try to make it
			valid_material = sync_missed_material(scene, update_context, curve_mat_id);
		}
		else {
			// already exported
			valid_material = XSI::CStatus::OK;
		}
	}

	if (is_get && valid_material == XSI::CStatus::OK && update_context->is_material_exists(curve_mat_id)) {
		size_t shader_index = update_context->get_xsi_material_cycles_index(curve_mat_id);

		ccl::array<ccl::Node*> used_shaders;
		used_shaders.push_back_slow(scene->shaders[shader_index]);
		curve_geom->set_used_shaders(used_shaders);
	}
	else {
		// fail to export missed material
		// so, output the warning and does not assign the shader
		log_warning("Curve object " + xsi_object.GetFullName() + " requred missed material.");

		ccl::array<ccl::Node*> used_shaders;
		used_shaders.push_back_slow(scene->shaders[0]);
		curve_geom->set_used_shaders(used_shaders);
	}
	curve_geom->curve_shape = scene->params.hair_shape;

	sync_curve_geom_process(scene, curve_geom, update_context, xsi_primitive, xsi_object, curve_property, motion_deform);

	update_context->add_geometry_index(xsi_curve_id, scene->geometry.size() - 1);

	return curve_geom;
}

XSI::CStatus update_curve(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
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

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesCurve", render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			// try to get xsi object property
			XSI::Property curve_prop = get_xsi_object_property(xsi_object, "CycleCurve");

			if (curve_prop.IsValid() && geometry->geometry_type == ccl::Geometry::Type::HAIR)
			{
				ccl::Hair* curve_geom = static_cast<ccl::Hair*>(geometry);
				curve_geom->clear(true);

				sync_curve_geom_process(scene, curve_geom, update_context, xsi_prim, xsi_object, curve_prop, motion_deform);

				curve_geom->tag_update(scene, true);
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
