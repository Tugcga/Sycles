#include <xsi_property.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>

#include "scene/scene.h"
#include "scene/object.h"
#include "util/hash.h"

#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"

ccl::uint get_ray_visibility(const XSI::CParameterRefArray &property_params, const XSI::CTime &eval_time)
{
	ccl::uint flag = 0;
	flag |= bool(property_params.GetValue("ray_visibility_camera", eval_time)) ? ccl::PATH_RAY_CAMERA : 0;
	flag |= bool(property_params.GetValue("ray_visibility_diffuse", eval_time)) ? ccl::PATH_RAY_DIFFUSE : 0;
	flag |= bool(property_params.GetValue("ray_visibility_glossy", eval_time)) ? ccl::PATH_RAY_GLOSSY : 0;
	flag |= bool(property_params.GetValue("ray_visibility_transmission", eval_time)) ? ccl::PATH_RAY_TRANSMIT : 0;
	flag |= bool(property_params.GetValue("ray_visibility_shadow", eval_time)) ? ccl::PATH_RAY_SHADOW : 0;
	flag |= bool(property_params.GetValue("ray_visibility_volume_scatter", eval_time)) ? ccl::PATH_RAY_VOLUME_SCATTER : 0;

	if (bool(property_params.GetValue("is_holdout", eval_time)))
	{
		flag &= ~(ccl::PATH_RAY_ALL_VISIBILITY - ccl::PATH_RAY_CAMERA);
	}
	return flag;
}

void sync_geometry_object_parameters(ccl::Scene* scene, ccl::Object* object, XSI::X3DObject &xsi_object, XSI::CString &lightgroup, bool &out_motion_deform, const XSI::CString &property_name, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
{
	// set unique pass id
	bool output_pass_assign_unique_pass_id = render_parameters.GetValue("output_pass_assign_unique_pass_id", eval_time);
	if (output_pass_assign_unique_pass_id)
	{
		object->set_pass_id(scene->objects.size());
	}
	else
	{
		object->set_pass_id(0);
	}

	// try to set custom hair property
	XSI::Property xsi_property;
	bool use_property = get_xsi_object_property(xsi_object, property_name, xsi_property);
	out_motion_deform = false;
	lightgroup = "";
	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		if (!output_pass_assign_unique_pass_id)
		{
			object->set_pass_id(xsi_params.GetValue("pass_id", eval_time));
		}

		out_motion_deform = xsi_params.GetValue("motion_blur_deformation", eval_time);

		object->set_visibility(get_ray_visibility(xsi_params, eval_time));

		// TODO: sometimes, when we change shadow catcher parameter in update, the effect is not visible
		object->set_is_shadow_catcher(xsi_params.GetValue("shadow_catcher", eval_time));
		object->set_use_holdout(xsi_params.GetValue("is_holdout", eval_time));
		object->set_shadow_terminator_shading_offset(xsi_params.GetValue("shadow_terminator", eval_time));
		object->set_shadow_terminator_geometry_offset(xsi_params.GetValue("shadow_terminator_geometry", eval_time));
		object->set_ao_distance(xsi_params.GetValue("ao_distance", eval_time));

		object->set_is_caustics_caster(xsi_params.GetValue("caustics_cast", eval_time));
		object->set_is_caustics_receiver(xsi_params.GetValue("caustics_receive", eval_time));

		lightgroup = xsi_params.GetValue("lightgroup", eval_time);
		object->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));

		object->tag_visibility_modified();
		object->tag_is_shadow_catcher_modified();
		object->tag_use_holdout_modified();
		object->tag_shadow_terminator_shading_offset_modified();
		object->tag_shadow_terminator_geometry_offset_modified();
		object->tag_ao_distance_modified();
	}

	// next object settings
	object->name = xsi_object.GetName().GetAsciiString();
	object->set_asset_name(OIIO::ustring(get_asset_name(xsi_object)));
	object->set_color(vector3_to_float3(get_object_color(xsi_object, eval_time)));
	object->set_alpha(1.0);

	object->tag_pass_id_modified();
	object->tag_color_modified();
	object->tag_alpha_modified();

	object->set_random_id(ccl::hash_string(object->name.c_str()));
}