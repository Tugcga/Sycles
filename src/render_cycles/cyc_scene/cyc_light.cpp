#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/light.h"
#include "scene/background.h"
#include "util/hash.h"

#include <xsi_light.h>
#include <xsi_x3dobject.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_transformation.h>
#include <xsi_project.h>
#include <xsi_projectitem.h>
#include <xsi_scene.h>
#include <xsi_model.h>
#include <xsi_property.h>
#include <xsi_status.h>

#include "cyc_materials/cyc_materials.h"
#include "cyc_scene.h"
#include "../../render_cycles/update_context.h"
#include "../../utilities/logs.h"
#include "../../utilities/xsi_shaders.h"
#include "../../utilities/math.h"
#include "../../render_base/type_enums.h"

ccl::PathRayVisibility light_visibility_flag(bool use_camera, bool use_diffuse, bool use_glossy, bool use_transmission, bool use_scatter) {
	ccl::PathRayVisibility visibility = ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= use_camera ? ccl::PATH_RAY_VISIBILITY_CAMERA : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= use_diffuse ? ccl::PATH_RAY_VISIBILITY_DIFFUSE : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= use_glossy ? ccl::PATH_RAY_VISIBILITY_GLOSSY : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= use_transmission ? ccl::PATH_RAY_VISIBILITY_TRANSMIT : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= use_scatter ? ccl::PATH_RAY_VISIBILITY_VOLUME_SCATTER : ccl::PATH_RAY_VISIBILITY_NONE;

	return visibility;
}

ccl::Shader* build_xsi_light_shader(ccl::Scene* scene, const XSI::Light& xsi_light, UpdateContext* update_context)
{
	XSI::CRefArray xsi_shaders = xsi_light.GetShaders();
	XSI::ShaderParameter root_parameter = get_root_shader_parameter(xsi_shaders, "LightShader", false);
	XSI::ShaderParameter color_parameter;
	XSI::ShaderParameter intensity_parameter;
	ULONG xsi_light_shader_id = 0;

	if (root_parameter.IsValid())
	{
		XSI::Shader xsi_light_node = get_input_node(root_parameter);
		if (xsi_light_node.IsValid() && xsi_light_node.GetProgID() == "Softimage.soft_light.1.0")
		{
			// get color and intensity of the shader
			XSI::CParameterRefArray all_params = xsi_light_node.GetParameters();
			color_parameter = all_params.GetItem("color");
			intensity_parameter = all_params.GetItem("intensity");
			xsi_light_shader_id = xsi_light_node.GetObjectID();
		}
	}

	ccl::Shader* light_shader = scene->create_node<ccl::Shader>();
	light_shader->name = "Light " + std::to_string(xsi_light.GetObjectID()) + " shader";
	std::unique_ptr<ccl::ShaderGraph> graph = std::make_unique<ccl::ShaderGraph>();
	ccl::EmissionNode* node = graph->create_node<ccl::EmissionNode>();

	std::unordered_map<ULONG, ccl::ShaderNode*> nodes_map;

	// emission node has input Color and Strength ports
	// so, we should try to connect it to nodes, connected to original color and intensity ports
	// we should get shader node, connected to the color shader parameter (if it valid)
	
	if (color_parameter.IsValid())
	{
		sync_float3_parameter(scene, graph.get(), node, color_parameter, "Color", update_context);
	}
	else
	{
		node->set_color(ccl::make_float3(0.0, 0.0, 0.0));
	}

	if (intensity_parameter.IsValid())
	{
		sync_float_parameter(scene, graph.get(), node, intensity_parameter, "Strength", update_context);
	}
	else
	{
		node->set_strength(0.0f);
	}
	
	ccl::ShaderNode* out = graph->output();
	graph->connect(node->output("Emission"), out->input("Surface"));
	light_shader->set_graph(std::move(graph));
	light_shader->tag_update(scene);

	return light_shader;
}

// common method as for xsi lights and custom lights
void sync_light_tfm(ccl::Object* light_object, const XSI::MATH::CMatrix4 &xsi_tfm_matrix)
{
	light_object->set_tfm(xsi_matrix_to_transform(xsi_tfm_matrix));
}

XSI::MATH::CTransformation tweak_xsi_light_transform(const XSI::MATH::CTransformation &xsi_tfm, const XSI::Light& xsi_light, const XSI::CTime& eval_time)
{
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);

	XSI::MATH::CTransformation rotate_tfm;
	rotate_tfm.SetIdentity();
	if (xsi_area && xsi_area_shape != 3)
	{
		float xsi_rotation_x = xsi_light.GetParameterValue("LightAreaXformRX", eval_time);
		float xsi_rotation_y = xsi_light.GetParameterValue("LightAreaXformRY", eval_time);
		float xsi_rotation_z = xsi_light.GetParameterValue("LightAreaXformRZ", eval_time);

		// rectangle can be rotated, so, change transform matrix
		rotate_tfm.SetRotX(xsi_rotation_x);
		rotate_tfm.SetRotY(xsi_rotation_y);
		rotate_tfm.SetRotZ(xsi_rotation_z);
	}

	XSI::MATH::CTransformation to_return;
	to_return.Mul(rotate_tfm, xsi_tfm);
	to_return.SetScalingFromValues(1.0, 1.0, 1.0);

	return to_return;
}

void sync_xsi_light_tfm(ccl::Object* light_object, const XSI::Light& xsi_light, const XSI::CTime& eval_time)
{
	XSI::MATH::CTransformation xsi_tfm = xsi_light.GetKinematics().GetGlobal().GetTransform(eval_time);
	XSI::MATH::CTransformation xsi_tweaked_tfm = tweak_xsi_light_transform(xsi_tfm, xsi_light, eval_time);
	
	sync_light_tfm(light_object, xsi_tweaked_tfm.GetMatrix4());
}

ccl::LightType recognise_light_type(const XSI::Light& xsi_light, const XSI::CTime &eval_time) {
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);
	int xsi_light_type = xsi_light.GetParameterValue("Type", eval_time);

	if (xsi_area && xsi_area_shape != 3) {
		return ccl::LightType::LIGHT_AREA;
	}
	else {
		if ((xsi_area && xsi_area_shape == 3) || xsi_light_type == 0) {
			return ccl::LightType::LIGHT_POINT;
		}
		else if (xsi_light_type == 1) {
			return ccl::LightType::LIGHT_SUN;
		}
		else if (xsi_light_type == 2) {
			return ccl::LightType::LIGHT_SPOT;
		}
		else {
			// use triangle as unknown light type
			return ccl::LightType::LIGHT_TRIANGLE;
		}
	}

	return ccl::LightType::LIGHT_TRIANGLE;
}

void sync_xsi_light_geometry(ccl::Scene* scene, ccl::Light* light, ccl::LightType light_type, ccl::Shader* light_shader, const XSI::Light &xsi_light, const XSI::CTime &eval_time)
{
	// get light shader
	// here we need shader only for light parameters (spread and umbra)
	// actual shader created in another place
	XSI::CRefArray xsi_shaders = xsi_light.GetShaders();
	XSI::ShaderParameter root_parameter = get_root_shader_parameter(xsi_shaders, "LightShader", false);
	float xsi_spread = 1.0f;  // in degrees, used only for infinite light, define the size of the sun
	float xsi_power = 250.0f;
	float xsi_umbra = 1.0f;  // use for spot cone hardness
	if (root_parameter.IsValid())
	{
		XSI::Shader xsi_light_node = get_input_node(root_parameter);
		if (xsi_light_node.IsValid() && xsi_light_node.GetProgID() == "Softimage.soft_light.1.0")
		{
			XSI::CParameterRefArray all_params = xsi_light_node.GetParameters();

			// some params from the shader
			xsi_spread = get_float_parameter_value(all_params, "spread", eval_time);
			xsi_umbra = get_float_parameter_value(all_params, "factor", eval_time);
		}
	}

	// assign the shader
	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(light_shader);
	light->set_used_shaders(used_shaders);
	light->set_is_enabled(true);

	// next setup parameters from xsi light properties and for different light types
	int xsi_light_type = xsi_light.GetParameterValue("Type", eval_time);
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	float cone = xsi_light.GetParameterValue("LightCone", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);
	float xsi_size_x = xsi_light.GetParameterValue("LightAreaXformSX", eval_time);
	float xsi_size_y = xsi_light.GetParameterValue("LightAreaXformSY", eval_time);

	if (light_type == ccl::LightType::LIGHT_AREA)
	{// active area, but shape is not a sphere (sphere is a simple point light)
		light->set_light_type(ccl::LightType::LIGHT_AREA);
		ccl::AreaLight* area_light = dynamic_cast<ccl::AreaLight*>(light);

		// consider different shapes
		if (xsi_area_shape == 2)
		{// disc
			// use only x size
			area_light->set_ellipse(true);
			area_light->set_sizeu(xsi_size_x * 2.0f);
			area_light->set_sizev(xsi_size_x * 2.0f);
		}
		else
		{// all other shapes are rectangles
			area_light->set_ellipse(false);
			area_light->set_sizeu(xsi_size_x);
			area_light->set_sizev(xsi_size_y);
		}
		
		area_light->set_is_portal(false);
		area_light->set_spread(XSI::MATH::PI);
	}
	else
	{
		if (light_type == ccl::LightType::LIGHT_POINT)
		{// point light
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			ccl::PointLight* point_light = dynamic_cast<ccl::PointLight*>(light);
			if (xsi_area)
			{
				point_light->set_radius(xsi_size_x);
				point_light->set_is_sphere(true);  // always set true, in Blender this paramete defined by Soft Fallof, and there is no difference
			}
			else
			{
				point_light->set_radius(0.001f);  // use constant value
			}
		}
		else if (light_type == ccl::LightType::LIGHT_SUN)
		{// infinite light
			light->set_light_type(ccl::LightType::LIGHT_SUN);
			ccl::SunLight* sun_light = dynamic_cast<ccl::SunLight*>(light);
			// set angle from spread value
			sun_light->set_angle(xsi_spread * XSI::MATH::PI / 180.0f);
			// for infinite light set unit power
			xsi_power = 1.0f;
		}
		else if (light_type == ccl::LightType::LIGHT_SPOT)
		{// spot light
			light->set_light_type(ccl::LightType::LIGHT_SPOT);
			ccl::SpotLight* spot_light = dynamic_cast<ccl::SpotLight*>(light);

			spot_light->set_angle(cone * XSI::MATH::PI / 180.0);
			spot_light->set_smooth(xsi_umbra);
		}
		else //light_type == ccl::LightType::LIGHT_TRIANGLE
		{// unknown light type
			// set as point
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			ccl::PointLight* point_light = dynamic_cast<ccl::PointLight*>(light);
			point_light->set_radius(0.001f);
		}
	}

	// setup common light parameters
	light->set_strength(ccl::one_float3() * xsi_power);  // for all light except infinite set constant power
	light->set_cast_shadow(true);
	light->set_use_mis(true);
	light->set_max_bounces(1024);
}

void sync_xsi_light_object(ccl::Object* light_object, const XSI::Light& xsi_light, UpdateContext* update_context) {
	XSI::CTime eval_time = update_context->get_time();

	sync_xsi_light_tfm(light_object, xsi_light, eval_time);

	bool xsi_visible = xsi_light.GetParameterValue("LightAreaVisible", eval_time);
	light_object->set_visibility(light_visibility_flag(xsi_visible, true, true, true, true));
	// for xsi-light always mark it as shadow catcher
	light_object->set_is_shadow_catcher(true);

	light_object->set_random_id(ccl::hash_uint2(ccl::hash_string(xsi_light.GetUniqueName().GetAsciiString()), 0));
	light_object->tag_visibility_modified();
	light_object->tag_random_id_modified();
}

void sync_xsi_light(ccl::Scene* scene, const XSI::Light &xsi_light, UpdateContext* update_context)
{
	update_context->add_sync_profiler_time_start(SyncType::Light, xsi_light.GetObjectID(), xsi_light.GetFullName());

	XSI::CTime eval_time = update_context->get_time();

	ccl::Object* light_object = scene->create_node<ccl::Object>();
	// save connection between light object in the Softimage scene and in the Cycles scene
	update_context->add_light_index(xsi_light.GetObjectID(), scene->objects.size() - 1);

	// create Cycles light
	ccl::LightType light_type = recognise_light_type(xsi_light, eval_time);
	ccl::Light* light = NULL;
	if (light_type == ccl::LIGHT_POINT) {
		light = scene->create_light_node<ccl::PointLight>();
	}
	else if (light_type == ccl::LIGHT_SPOT) {
		light = scene->create_light_node<ccl::SpotLight>();
	}
	else if (light_type == ccl::LIGHT_SUN) {
		light = scene->create_light_node<ccl::SunLight>();
	}
	else if (light_type == ccl::LIGHT_AREA) {
		light = scene->create_light_node<ccl::AreaLight>();
	}
	if (light != NULL) {
		light_object->set_geometry(light);

		ccl::Shader* light_shader = build_xsi_light_shader(scene, xsi_light, update_context);

		// setup light parameters
		sync_xsi_light_geometry(scene, light, light_type, light_shader, xsi_light, eval_time);
		sync_xsi_light_object(light_object, xsi_light, update_context);

		light->tag_update(scene);
		light_object->tag_update(scene);

		update_context->add_sync_profiler_time_finish(SyncType::Light, xsi_light.GetObjectID());
	}
}

void sync_custom_light_geometry(ccl::Light* light, CustomLightType light_type, const XSI::CParameterRefArray &xsi_parameters, const XSI::CTime &eval_time)
{
	light->set_is_enabled(true);

	if (light_type == CustomLightType_Point)
	{
		light->set_light_type(ccl::LightType::LIGHT_POINT);
		ccl::PointLight* point_light = dynamic_cast<ccl::PointLight*>(light);

		point_light->set_radius(xsi_parameters.GetValue("size", eval_time));
		point_light->set_is_sphere(true);
	}
	else if (light_type == CustomLightType_Sun)
	{
		light->set_light_type(ccl::LightType::LIGHT_SUN);
		ccl::SunLight* sun_light = dynamic_cast<ccl::SunLight*>(light);

		sun_light->set_angle((float)xsi_parameters.GetValue("angle", eval_time) * XSI::MATH::PI / 180.0f);
	}
	else if (light_type == CustomLightType_Spot)
	{
		light->set_light_type(ccl::LightType::LIGHT_SPOT);
		ccl::SpotLight* spot_light = dynamic_cast<ccl::SpotLight*>(light);

		spot_light->set_radius(xsi_parameters.GetValue("size", eval_time));
		spot_light->set_angle(DEG2RADF((float)xsi_parameters.GetValue("spot_angle", eval_time)));
		spot_light->set_smooth(xsi_parameters.GetValue("spot_smooth", eval_time));
	}
	else if (light_type == CustomLightType_Area)
	{
		light->set_light_type(ccl::LightType::LIGHT_AREA);
		ccl::AreaLight* area_light = dynamic_cast<ccl::AreaLight*>(light);

		area_light->set_sizeu(xsi_parameters.GetValue("sizeU", eval_time));
		area_light->set_sizev(xsi_parameters.GetValue("sizeV", eval_time));
		area_light->set_is_portal(xsi_parameters.GetValue("is_portal", eval_time));
		area_light->set_spread(DEG2RADF((float)xsi_parameters.GetValue("spread", eval_time)));
		XSI::CValue shape_value = xsi_parameters.GetValue("shape", eval_time);
		bool is_round = false;
		if (!shape_value.IsEmpty())
		{
			is_round = static_cast<int>(shape_value) != 0;
		}
		area_light->set_ellipse(is_round);
	}

	light->set_strength(ccl::one_float3() * (float)xsi_parameters.GetValue("power", eval_time));
	light->set_cast_shadow(xsi_parameters.GetValue("cast_shadow", eval_time));
	light->set_use_mis(xsi_parameters.GetValue("multiple_importance", eval_time));
	light->set_max_bounces(xsi_parameters.GetValue("max_bounces", eval_time));
}

void sync_light_tfm(ccl::Object* light_object, const XSI::MATH::CTransformation& xsi_tfm)
{
	XSI::MATH::CMatrix4 xsi_matrix = xsi_tfm.GetMatrix4();
	sync_light_tfm(light_object, xsi_matrix);

	light_object->tag_tfm_modified();
}

void sync_custom_light_tfm(ccl::Object* light_object, const XSI::KinematicState &xsi_kine, const XSI::CTime& eval_time)
{
	XSI::MATH::CMatrix4 xsi_matrix = xsi_kine.GetTransform(eval_time).GetMatrix4();
	sync_light_tfm(light_object, xsi_matrix);

	light_object->tag_tfm_modified();
}

CustomLightType get_custom_light_type(const XSI::CString &type_str)
{
	if (type_str == "cyclesPoint") { return CustomLightType_Point; }
	else if (type_str == "cyclesSun") { return CustomLightType_Sun; }
	else if (type_str == "cyclesSpot") { return CustomLightType_Spot; }
	else if (type_str == "cyclesArea") { return CustomLightType_Area; }
	else if (type_str == "cyclesBackground") { return CustomLightType_Background; }
	else { return CustomLightType_Unknown; }
}

void set_background_params(ccl::Background* background, ccl::Shader* bg_shader, const XSI::CParameterRefArray& render_parameters, const XSI::CString &lightgroup, const XSI::CTime& eval_time)
{
	background->set_transparent(render_parameters.GetValue("film_transparent", eval_time));
	background->set_transparent_glass(background->get_transparent() ? (bool)render_parameters.GetValue("film_transparent_glass", eval_time) : false);
	background->set_transparent_roughness_threshold(background->get_transparent() ? (bool)render_parameters.GetValue("film_transparent_roughness", eval_time) : 0.0);

	background->set_visibility(light_visibility_flag(
		render_parameters.GetValue("background_ray_visibility_camera", eval_time),
		render_parameters.GetValue("background_ray_visibility_diffuse", eval_time),
		render_parameters.GetValue("background_ray_visibility_glossy", eval_time),
		render_parameters.GetValue("background_ray_visibility_transmission", eval_time),
		render_parameters.GetValue("background_ray_visibility_scatter", eval_time)));
	background->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));
	// all other shader parameters will be setup for all shaders array
}

void set_background_light_params(ccl::Scene* scene, ccl::BackgroundLight* light, ccl::Shader* bg_shader, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	int background_surface_sampling_method = render_parameters.GetValue("background_surface_sampling_method", eval_time);
	int background_surface_resolution = render_parameters.GetValue("background_surface_resolution", eval_time);
	int background_surface_max_bounces = render_parameters.GetValue("background_surface_max_bounces", eval_time);
	bool background_surface_shadow_caustics = render_parameters.GetValue("background_surface_shadow_caustics", eval_time);
	bool background_surface_cast_shadow = render_parameters.GetValue("background_surface_cast_shadow", eval_time);
	light->set_use_mis(background_surface_sampling_method != 0);
	light->set_map_resolution(background_surface_sampling_method == 2 ? background_surface_resolution : 0);
	light->set_max_bounces(background_surface_max_bounces);
	light->set_use_caustics(background_surface_shadow_caustics);
	light->set_cast_shadow(background_surface_cast_shadow);

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(bg_shader);
	light->set_used_shaders(used_shaders);

	light->tag_map_resolution_modified();
	light->tag_max_bounces_modified();
	light->tag_use_caustics_modified();
	light->tag_cast_shadow_modified();

	light->tag_update(scene);
}

void set_background_light(ccl::Scene* scene, ccl::Background* background, ccl::Shader* bg_shader, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	XSI::CString lightgroup = render_parameters.GetValue("background_lightgroup", eval_time);
	update_context->add_lightgroup(lightgroup);
	set_background_params(background, bg_shader, render_parameters, lightgroup, eval_time);

	// add background light to the scene
	int bg_index = update_context->get_background_light_index();
	ccl::Object* bg_object = NULL;
	if (bg_index >= 0) {
		bg_object = scene->objects[bg_index];
	}
	else {
		bg_object = scene->create_node<ccl::Object>();
		update_context->set_background_light_index(scene->objects.size() - 1);
	}

	ccl::BackgroundLight* light = scene->create_node<ccl::BackgroundLight>();
	bg_object->set_geometry(light);

	light->set_is_enabled(true);
	light->set_light_type(ccl::LightType::LIGHT_BACKGROUND);
	bg_object->set_is_shadow_catcher(true);

	set_background_light_params(scene, light, bg_shader, render_parameters, eval_time);
}

void sync_custom_background(ccl::Scene* scene, const XSI::X3DObject &xsi_object, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	update_context->add_sync_profiler_time_start(SyncType::Light, xsi_object.GetObjectID(), xsi_object.GetFullName());
	// at first we should get the shader
	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	if (update_context->is_material_exists(xsi_material_id))
	{
		size_t shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
		if (shader_index >= 0 && shader_index < scene->shaders.size())
		{
			ccl::Shader* shader = scene->shaders[shader_index];
			scene->default_background = shader;
			set_background_light(scene, scene->background, scene->default_background, update_context, render_parameters, eval_time);
			update_context->set_use_background_light(shader_index, xsi_material.GetObjectID());
		}
	}

	update_context->add_sync_profiler_time_finish(SyncType::Light, xsi_object.GetObjectID());
}

void sync_custom_light_object(ccl::Object* light_object, const XSI::X3DObject& xsi_object, UpdateContext* update_context) {
	XSI::CTime eval_time = update_context->get_time();

	sync_custom_light_tfm(light_object, xsi_object.GetKinematics().GetGlobal(), eval_time);

	XSI::CString xsi_name = xsi_object.GetUniqueName();
	XSI::CParameterRefArray xsi_parameters = xsi_object.GetParameters();
	XSI::CString lightgroup = xsi_parameters.GetValue("lightgroup", eval_time);
	
	light_object->set_visibility(light_visibility_flag(
		xsi_parameters.GetValue("use_camera", eval_time),
		xsi_parameters.GetValue("use_diffuse", eval_time),
		xsi_parameters.GetValue("use_glossy", eval_time),
		xsi_parameters.GetValue("use_transmission", eval_time),
		xsi_parameters.GetValue("use_scatter", eval_time)));
	light_object->set_is_shadow_catcher(xsi_parameters.GetValue("is_shadow_catcher", eval_time));
	light_object->set_random_id(ccl::hash_uint2(ccl::hash_string(xsi_name.GetAsciiString()), 0));
	light_object->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));
	update_context->add_lightgroup(lightgroup);
}

void sync_custom_light(ccl::Scene* scene, const XSI::X3DObject & xsi_object, UpdateContext* update_context)
{
	update_context->add_sync_profiler_time_start(SyncType::Light, xsi_object.GetObjectID(), xsi_object.GetFullName());

	XSI::CTime eval_time = update_context->get_time();
	CustomLightType light_type = get_custom_light_type(xsi_object.GetType());
	if (light_type != CustomLightType_Unknown)
	{
		if (light_type != CustomLightType_Background && light_type != CustomLightType_Unknown)
		{
			// get light shader index
			XSI::Material xsi_material = xsi_object.GetMaterial();
			ULONG xsi_material_id = xsi_material.GetObjectID();
			if (update_context->is_material_exists(xsi_material_id))
			{
				size_t shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);

				ccl::Object* light_object = scene->create_node<ccl::Object>();
				update_context->add_light_index(xsi_object.GetObjectID(), scene->objects.size() - 1);

				ccl::Light* light = NULL;
				if (light_type == CustomLightType_Point) {
					light = scene->create_light_node<ccl::PointLight>();
				}
				else if (light_type == CustomLightType_Spot) {
					light = scene->create_light_node<ccl::SpotLight>();
				}
				else if (light_type == CustomLightType_Sun) {
					light = scene->create_light_node<ccl::SunLight>();
				}
				else if (light_type == CustomLightType_Area) {
					light = scene->create_light_node<ccl::AreaLight>();
				}

				light_object->set_geometry(light);
				ccl::Shader* shader = scene->shaders[shader_index];
				ccl::array<ccl::Node*> used_shaders;
				used_shaders.push_back_slow(shader);
				light->set_used_shaders(used_shaders);

				XSI::CParameterRefArray xsi_parameters = xsi_object.GetParameters();
				sync_custom_light_geometry(light, light_type, xsi_parameters, eval_time);
				sync_custom_light_object(light_object, xsi_object, update_context);
				light->set_use_caustics(xsi_parameters.GetValue("shadow_caustics", eval_time));
			}
		}
	}

	update_context->add_sync_profiler_time_finish(SyncType::Light, xsi_object.GetObjectID());
}

void build_backgound_default_graph(ccl::ShaderGraph* bg_graph, const XSI::MATH::CColor4f &color)
{
	ccl::BackgroundNode* bg_node = bg_graph->create_node<ccl::BackgroundNode>();
	bg_node->input("Color")->set(color4_to_float3(color));
	bg_node->input("Strength")->set(1.0f);
	ccl::ShaderNode* bg_out = bg_graph->output();
	bg_graph->connect(bg_node->output("Background"), bg_out->input("Surface"));
}

XSI::MATH::CColor4f get_xsi_ambience(const XSI::CTime &eval_time)
{
	XSI::MATH::CColor4f to_return(0.0, 0.0, 0.0, 1.0);

	XSI::Project xsi_project = XSI::Application().GetActiveProject();
	XSI::Scene xsi_scene = xsi_project.GetActiveScene();
	XSI::Model xsi_root = xsi_scene.GetRoot();
	XSI::Property xsi_prop;
	xsi_root.GetPropertyFromName("AmbientLighting", xsi_prop);
	if (xsi_prop.IsValid())
	{
		XSI::Parameter xsi_param = xsi_prop.GetParameter("ambience");
		XSI::CParameterRefArray color_params = xsi_param.GetParameters();
		float r = color_params.GetValue("red", eval_time);
		float g = color_params.GetValue("green", eval_time);
		float b = color_params.GetValue("blue", eval_time);

		to_return.Set(r, g, b, 1.0f);
	}

	return to_return;
}

// called at the scene creation process, if there are no background lights in custom lights array
void sync_background_color(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::MATH::CColor4f ambience_color = get_xsi_ambience(eval_time);

	// setup shader
	ccl::Shader* bg_shader = scene->default_background;
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	build_backgound_default_graph(shader_graph.get(), ambience_color);
	bg_shader->set_graph(std::move(shader_graph));
	bg_shader->tag_update(scene);

	// add light and set it properties
	set_background_light(scene, scene->background, bg_shader, update_context, render_parameters, eval_time);
}

// called when we change ambience color of the scene
XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context)
{
	if (!update_context->get_use_background_light())
	{
		XSI::CTime eval_time = update_context->get_time();
		XSI::MATH::CColor4f ambience_color = get_xsi_ambience(eval_time);

		ccl::Shader* bg_shader = scene->default_background;
		std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
		build_backgound_default_graph(shader_graph.get(), ambience_color);
		bg_shader->set_graph(std::move(shader_graph));
		bg_shader->tag_update(scene);

		// update the shader, no need to do this again
		update_context->reset_need_update_background();
	}

	return XSI::CStatus::OK;
}

// called in post scene if we change background parameters from render settings
XSI::CStatus update_background_parameters(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	set_background_light(scene, scene->background, scene->default_background, update_context, render_parameters, eval_time);

	return XSI::CStatus::OK;
}

XSI::CStatus update_xsi_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light &xsi_light)
{
	ULONG xsi_id = xsi_light.GetObjectID();
	if (update_context->is_xsi_light_exists(xsi_id))
	{
		std::vector<size_t> light_indexes = update_context->get_xsi_light_cycles_indexes(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		for (size_t i = 0; i < light_indexes.size(); i++)
		{
			size_t light_index = light_indexes[i];
			ccl::Object* light_object = scene->objects[light_index];
			ccl::Geometry* object_geometry = light_object->get_geometry();
			if (object_geometry->geometry_type == ccl::Geometry::POINT_LIGHT || 
				object_geometry->geometry_type == ccl::Geometry::SPOT_LIGHT || 
				object_geometry->geometry_type == ccl::Geometry::SUN_LIGHT || 
				object_geometry->geometry_type == ccl::Geometry::AREA_LIGHT) {
				ccl::Light* light = static_cast<ccl::Light*>(object_geometry);
				ccl::LightType light_type = recognise_light_type(xsi_light, eval_time);
				sync_xsi_light_geometry(scene, light, light_type, build_xsi_light_shader(scene, xsi_light, update_context), xsi_light, eval_time);
				sync_xsi_light_object(light_object, xsi_light, update_context);
				
				light->tag_update(scene);
				light_object->tag_update(scene);
			}
			else {
				return XSI::CStatus::Abort;
			}
		}

		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else {
		return XSI::CStatus::Abort;
	}
}

XSI::CStatus update_custom_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object)
{
	ULONG xsi_id = xsi_object.GetObjectID();
	CustomLightType light_type = get_custom_light_type(xsi_object.GetType());
	if (update_context->is_xsi_light_exists(xsi_id) && light_type != CustomLightType_Unknown && light_type != CustomLightType_Background)
	{
		// get all cycles light indexes which corresponds to the given xsi light
		std::vector<size_t> light_indexes = update_context->get_xsi_light_cycles_indexes(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		for (size_t i = 0; i < light_indexes.size(); i++)
		{
			size_t light_index = light_indexes[i];

			ccl::Object* light_object = scene->objects[light_index];
			ccl::Geometry* object_geometry = light_object->get_geometry();
			if (object_geometry->geometry_type == ccl::Geometry::POINT_LIGHT ||
				object_geometry->geometry_type == ccl::Geometry::SPOT_LIGHT ||
				object_geometry->geometry_type == ccl::Geometry::SUN_LIGHT ||
				object_geometry->geometry_type == ccl::Geometry::AREA_LIGHT) {
				ccl::Light* light = static_cast<ccl::Light*>(object_geometry);

				sync_custom_light_geometry(light, light_type, xsi_object.GetParameters(), eval_time);
				sync_custom_light_object(light_object, xsi_object, update_context);

				XSI::CParameterRefArray xsi_parameters = xsi_object.GetParameters();
				XSI::CString lightgroup = xsi_parameters.GetValue("lightgroup", eval_time);
				update_context->add_lightgroup(lightgroup);
				light->tag_update(scene);
				light_object->tag_update(scene);
			}
			else {
				return XSI::CStatus::Abort;
			}
		}

		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}

XSI::CStatus update_xsi_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light)
{
	ULONG xsi_id = xsi_light.GetObjectID();
	if (update_context->is_xsi_light_exists(xsi_id))
	{
		// change transform of the light in the scene
		// this light can corresponds different lights in cycles, because it's possible to exists instances

		std::vector<size_t> light_indexes = update_context->get_xsi_light_cycles_indexes(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		// here we update transforms of all instances, but all of them will coincide
		// later we will update instnce transforms by using additional data
		for (size_t i = 0; i < light_indexes.size(); i++)
		{
			size_t light_index = light_indexes[i];
			ccl::Object* light_object = scene->objects[light_index];
			sync_xsi_light_tfm(light_object, xsi_light, eval_time);

			light_object->tag_update(scene);
		}

		// we may change position of the light for sun in background sky
		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

XSI::CStatus update_custom_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object)
{
	ULONG xsi_id = xsi_object.GetObjectID();
	if (update_context->is_xsi_light_exists(xsi_id))
	{
		std::vector<size_t> light_indexes = update_context->get_xsi_light_cycles_indexes(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		for(size_t i = 0; i < light_indexes.size(); i++)
		{
			size_t light_index = light_indexes[i];

			ccl::Object* light_object = scene->objects[light_index];
			sync_custom_light_tfm(light_object, xsi_object.GetKinematics().GetGlobal(), eval_time);
			light_object->tag_update(scene);
		}
		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else {
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

void update_background(ccl::Scene* scene, UpdateContext* update_context)
{
	if (update_context->get_use_background_light())
	{
		size_t shader_index = update_context->get_background_shader_index();
		ULONG material_id = update_context->get_background_xsi_material_id();
		XSI::ProjectItem item = XSI::Application().GetObjectFromID(material_id);
		XSI::Material xsi_material(item);

		XSI::CStatus is_update = update_material(scene, xsi_material, shader_index, update_context);
		set_background_light(scene, scene->background, scene->default_background, update_context, update_context->get_current_render_parameters(), update_context->get_time());
	}
}