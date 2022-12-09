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

#include "../../render_cycles/update_context.h"
#include "../../utilities/logs.h"
#include "../../utilities/xsi_shaders.h"
#include "../../utilities/math.h"

ccl::Shader* build_xsi_light_shader(ccl::Scene* scene, const XSI::Light& xsi_light, const XSI::CTime& eval_time)
{
	XSI::CRefArray xsi_shaders = xsi_light.GetShaders();
	XSI::ShaderParameter root_parameter = get_root_shader_parameter(xsi_shaders, "LightShader", false);
	XSI::MATH::CColor4f xsi_color(0.0, 0.0, 0.0, 0.0);
	float xsi_intensity = 0.0f;

	if (root_parameter.IsValid())
	{
		XSI::Shader xsi_light_node = get_input_node(root_parameter);
		if (xsi_light_node.IsValid() && xsi_light_node.GetProgID() == "Softimage.soft_light.1.0")
		{
			// TODO: supports shader networks here, read connections to color and intensity

			// get color and intensity of the shader
			XSI::CParameterRefArray all_params = xsi_light_node.GetParameters();
			xsi_color = get_color_parameter_value(all_params, "color", eval_time);
			xsi_intensity = get_float_parameter_value(all_params, "intensity", eval_time);
		}
	}

	ccl::Shader* light_shader = new ccl::Shader();
	light_shader->name = "Light " + std::to_string(xsi_light.GetObjectID()) + " shader";
	ccl::ShaderGraph* graph = new ccl::ShaderGraph();
	ccl::EmissionNode* node = new ccl::EmissionNode();
	node->set_color(color4_to_float3(xsi_color));
	node->set_strength(xsi_intensity);
	graph->add(node);
	ccl::ShaderNode* out = graph->output();
	graph->connect(node->output("Emission"), out->input("Surface"));
	light_shader->set_graph(graph);
	light_shader->tag_update(scene);

	scene->shaders.push_back(light_shader);

	return light_shader;
}

void sync_xsi_light_tfm(ccl::Light* light, const XSI::Light& xsi_light, const XSI::CTime& eval_time)
{
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);

	XSI::MATH::CTransformation xsi_tfm = xsi_light.GetKinematics().GetGlobal().GetTransform(eval_time);
	xsi_tfm.SetScalingFromValues(1.0, 1.0, 1.0);  // set neutral scaling

	XSI::MATH::CMatrix4 xsi_tfm_matrix = xsi_tfm.GetMatrix4();
	if (xsi_area && xsi_area_shape != 3)
	{
		float xsi_rotation_x = xsi_light.GetParameterValue("LightAreaXformRX", eval_time);
		float xsi_rotation_y = xsi_light.GetParameterValue("LightAreaXformRY", eval_time);
		float xsi_rotation_z = xsi_light.GetParameterValue("LightAreaXformRZ", eval_time);

		// rectangle can be rotated, so, change transform matrix
		XSI::MATH::CTransformation rotate_tfm;
		rotate_tfm.SetIdentity();
		rotate_tfm.SetRotX(xsi_rotation_x);
		rotate_tfm.SetRotY(xsi_rotation_y);
		rotate_tfm.SetRotZ(xsi_rotation_z);
		XSI::MATH::CMatrix4 rotate_matrix = rotate_tfm.GetMatrix4();
		xsi_tfm_matrix.Mul(rotate_matrix, xsi_tfm_matrix);
	}

	XSI::MATH::CVector3 xsi_tfm_position = xsi_tfm.GetTranslation();  // translation does not changed by previous rotation, so, we can use it
	XSI::MATH::CVector3 xsi_tfm_direction = XSI::MATH::CVector3(-1 * xsi_tfm_matrix.GetValue(2, 0), -1 * xsi_tfm_matrix.GetValue(2, 1), -1 * xsi_tfm_matrix.GetValue(2, 2));

	light->set_dir(vector3_to_float3(xsi_tfm_direction));
	light->set_co(vector3_to_float3(xsi_tfm_position));

	// also here set axis u and v
	XSI::MATH::CVector3 xsi_axis_u = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(0, 0), xsi_tfm_matrix.GetValue(0, 1), xsi_tfm_matrix.GetValue(0, 2));
	XSI::MATH::CVector3 xsi_axis_v = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(1, 0), xsi_tfm_matrix.GetValue(1, 1), xsi_tfm_matrix.GetValue(1, 2));
	light->set_axisu(vector3_to_float3(xsi_axis_u));
	light->set_axisv(vector3_to_float3(xsi_axis_v));

}

void sync_xsi_light(ccl::Scene* scene, ccl::Light* light, ccl::Shader* light_shader, const XSI::Light &xsi_light, const XSI::CTime &eval_time)
{
	// get light shader
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
	light->set_shader(light_shader);
	light->set_is_enabled(true);

	// next setup parameters from xsi light properties and for different light types
	int xsi_light_type = xsi_light.GetParameterValue("Type", eval_time);
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	float cone = xsi_light.GetParameterValue("LightCone", eval_time);
	bool xsi_visible = xsi_light.GetParameterValue("LightAreaVisible", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);
	float xsi_size_x = xsi_light.GetParameterValue("LightAreaXformSX", eval_time);
	float xsi_size_y = xsi_light.GetParameterValue("LightAreaXformSY", eval_time);

	if (xsi_area && xsi_area_shape != 3)
	{// active area, but shape is not a sphere (sphere is a simple point light)
		light->set_light_type(ccl::LightType::LIGHT_AREA);

		// consider different shapes
		if (xsi_area_shape == 2)
		{// disc
			// use only x size
			light->set_round(true);
			light->set_sizeu(xsi_size_x * 2.0f);
			light->set_sizev(xsi_size_x * 2.0f);
		}
		else
		{// all other shapes are rectangles
			light->set_round(false);
			light->set_sizeu(xsi_size_x);
			light->set_sizev(xsi_size_y);
		}
		
		light->set_size(1);
		light->set_is_portal(false);
		light->set_spread(XSI::MATH::PI);
	}
	else
	{
		if ((xsi_area && xsi_area_shape == 3) || xsi_light_type == 0)
		{// point light
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			if (xsi_area)
			{
				light->set_size(xsi_size_x);
			}
			else
			{
				light->set_size(0.001f);  // use constant value
			}
		}
		else if (xsi_light_type == 1)
		{// infinite light
			light->set_light_type(ccl::LightType::LIGHT_DISTANT);
			// set angle from spread value
			light->set_angle(xsi_spread * XSI::MATH::PI / 180.0f);
			// for infinite light set unit power
			xsi_power = 1.0f;
		}
		else if (xsi_light_type == 2)
		{// spot light
			light->set_light_type(ccl::LightType::LIGHT_SPOT);
			light->set_size(0.001f);

			light->set_spot_angle(cone * XSI::MATH::PI / 180.0);
			light->set_spot_smooth(xsi_umbra);
		}
		else
		{// unknown light type
			// set as point
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			light->set_size(0.001f);
		}
	}

	// setup common light parameters
	light->set_strength(ccl::one_float3() * xsi_power);  // for all light except infinite set constant power
	light->set_cast_shadow(true);
	light->set_use_mis(true);
	light->set_max_bounces(1024);
	light->set_use_camera(xsi_visible);
	light->set_use_diffuse(true);
	light->set_use_glossy(true);
	light->set_use_transmission(true);
	light->set_use_scatter(true);
	light->set_is_shadow_catcher(false);

	// set transform
	sync_xsi_light_tfm(light, xsi_light, eval_time);

	light->set_random_id(ccl::hash_uint2(ccl::hash_string(xsi_light.GetUniqueName().GetAsciiString()), 0));
}

void sync_xsi_lights(ccl::Scene* scene, const std::vector<XSI::Light> &xsi_lights, UpdateContext* update_context)
{
	XSI::CTime eval_time = update_context->get_time();
	for (size_t i = 0; i < xsi_lights.size(); i++)
	{
		XSI::Light xsi_light = xsi_lights[i];
		 // crete Cycles light
		ccl::Light* light = scene->create_node<ccl::Light>();

		ccl::Shader* light_shader = build_xsi_light_shader(scene, xsi_light, eval_time);
		// setup light parameters
		sync_xsi_light(scene, light, light_shader, xsi_light, eval_time);

		// save connection between light object in the Softimage scene and in the Cycles scene
		update_context->add_light_index(xsi_light.GetObjectID(), scene->lights.size() - 1);
	}
}

ccl::ShaderGraph* build_backgound_default_graph(const XSI::MATH::CColor4f &color)
{
	ccl::ShaderGraph* bg_graph = new ccl::ShaderGraph();
	ccl::BackgroundNode* bg_node = bg_graph->create_node<ccl::BackgroundNode>();
	bg_node->input("Color")->set(color4_to_float3(color));
	bg_node->input("Strength")->set(1.0f);
	ccl::ShaderNode* bg_out = bg_graph->output();
	bg_graph->add(bg_node);
	bg_graph->connect(bg_node->output("Background"), bg_out->input("Surface"));

	return bg_graph;
}

void set_background_params(ccl::Background* background, ccl::Shader* bg_shader, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
{
	background->set_transparent(render_parameters.GetValue("film_transparent", eval_time));
	background->set_transparent_glass(background->get_transparent() ? (bool)render_parameters.GetValue("film_transparent_glass", eval_time) : false);
	background->set_transparent_roughness_threshold(background->get_transparent() ? (bool)render_parameters.GetValue("film_transparent_roughness", eval_time) : 0.0);
	// set ray visibility flags
	ccl::uint visibility = 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_camera", eval_time)) ? ccl::PATH_RAY_CAMERA : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_diffuse", eval_time)) ? ccl::PATH_RAY_DIFFUSE : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_glossy", eval_time)) ? ccl::PATH_RAY_GLOSSY : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_transmission", eval_time)) ? ccl::PATH_RAY_TRANSMIT : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_scatter", eval_time)) ? ccl::PATH_RAY_VOLUME_SCATTER : 0;
	background->set_visibility(visibility);

	bg_shader->set_heterogeneous_volume(render_parameters.GetValue("background_volume_homogeneous", eval_time));
	int background_volume_sampling = render_parameters.GetValue("background_volume_sampling", eval_time);
	bg_shader->set_volume_sampling_method(background_volume_sampling == 2 ? ccl::VolumeSampling::VOLUME_SAMPLING_MULTIPLE_IMPORTANCE : (background_volume_sampling == 1 ? ccl::VolumeSampling::VOLUME_SAMPLING_EQUIANGULAR : ccl::VolumeSampling::VOLUME_SAMPLING_DISTANCE));
	int background_volume_interpolation = render_parameters.GetValue("background_volume_interpolation", eval_time);
	bg_shader->set_volume_interpolation_method(background_volume_interpolation == 1 ? ccl::VolumeInterpolation::VOLUME_INTERPOLATION_CUBIC : ccl::VolumeInterpolation::VOLUME_INTERPOLATION_LINEAR);
	bg_shader->set_volume_step_rate(render_parameters.GetValue("background_volume_step_rate", eval_time));
}

void set_background_light_params(ccl::Scene* scene, ccl::Light* light, ccl::Shader* bg_shader, const XSI::CParameterRefArray& render_parameters, const XSI::CTime &eval_time)
{
	int background_surface_sampling_method = render_parameters.GetValue("background_surface_sampling_method", eval_time);
	int background_surface_resolution = render_parameters.GetValue("background_surface_resolution", eval_time);
	int background_surface_max_bounces = render_parameters.GetValue("background_surface_max_bounces", eval_time);
	bool background_surface_shadow_caustics = render_parameters.GetValue("background_surface_shadow_caustics", eval_time);
	light->set_map_resolution(background_surface_sampling_method == 1 ? background_surface_resolution : 0);
	light->set_max_bounces(background_surface_max_bounces);
	light->set_use_caustics(background_surface_shadow_caustics);

	light->set_use_mis(true);
	light->set_shader(bg_shader);
	light->tag_update(scene);
}

void set_background_light(ccl::Scene* scene, ccl::Background* background, ccl::Shader* bg_shader, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	set_background_params(background, bg_shader, render_parameters, eval_time);

	// add background light to the scene
	ccl::Light* light = scene->create_node<ccl::Light>();
	light->set_is_enabled(true);
	light->set_light_type(ccl::LightType::LIGHT_BACKGROUND);

	set_background_light_params(scene, light, bg_shader, render_parameters, eval_time);

	update_context->set_background_light_index(scene->lights.size() - 1);
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

void sync_background_color(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::MATH::CColor4f ambience_color = get_xsi_ambience(eval_time);

	// setup shader
	ccl::Shader* bg_shader = scene->default_background;
	bg_shader->set_graph(build_backgound_default_graph(ambience_color));
	bg_shader->tag_update(scene);

	// add light and set it properties
	set_background_light(scene, scene->background, bg_shader, update_context, render_parameters, eval_time);
}

XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context)
{
	if (!update_context->get_use_background_light())
	{
		XSI::CTime eval_time = update_context->get_time();
		XSI::MATH::CColor4f ambience_color = get_xsi_ambience(eval_time);

		ccl::Shader* bg_shader = scene->default_background;
		bg_shader->set_graph(build_backgound_default_graph(ambience_color));
		bg_shader->tag_update(scene);
	}

	return XSI::CStatus::OK;
}

XSI::CStatus update_background_parameters(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();

	set_background_light(scene, scene->background, scene->default_background, update_context, render_parameters, eval_time);

	return XSI::CStatus::OK;
}

XSI::CStatus update_xsi_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light &xsi_light)
{
	ULONG xsi_id = xsi_light.GetObjectID();
	if (update_context->is_xsi_light_exists(xsi_id))
	{
		size_t light_index = update_context->get_xsi_light_cycles_index(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		ccl::Light* light = scene->lights[light_index];
		sync_xsi_light(scene, light, build_xsi_light_shader(scene, xsi_light, eval_time), xsi_light, eval_time);
		light->tag_update(scene);

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
		size_t light_index = update_context->get_xsi_light_cycles_index(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		ccl::Light* light = scene->lights[light_index];
		sync_xsi_light_tfm(light, xsi_light, eval_time);
		light->tag_update(scene);

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}