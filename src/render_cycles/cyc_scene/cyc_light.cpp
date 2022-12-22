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

ccl::Shader* build_xsi_light_shader(ccl::Scene* scene, const XSI::Light& xsi_light, const XSI::CTime& eval_time)
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

	ccl::Shader* light_shader = new ccl::Shader();
	light_shader->name = "Light " + std::to_string(xsi_light.GetObjectID()) + " shader";
	ccl::ShaderGraph* graph = new ccl::ShaderGraph();
	ccl::EmissionNode* node = new ccl::EmissionNode();

	std::unordered_map<ULONG, ccl::ShaderNode*> nodes_map;

	// emission node has input Color and Strength ports
	// so, we should try to connect it to nodes, connected to original color and intensity ports
	// we should get shader node, connected to the color shader parameter (if it valid)
	std::vector<XSI::CStringArray> aovs(2);
	aovs[0].Clear();
	aovs[1].Clear();
	if (color_parameter.IsValid())
	{
		sync_float3_parameter(scene, graph, node, color_parameter, nodes_map, aovs, "Color", eval_time);
	}
	else
	{
		node->set_color(ccl::make_float3(0.0, 0.0, 0.0));
	}

	if (intensity_parameter.IsValid())
	{
		sync_float_parameter(scene, graph, node, intensity_parameter, nodes_map, aovs, "Strength", eval_time);
	}
	else
	{
		node->set_strength(0.0f);
	}
	
	graph->add(node);
	ccl::ShaderNode* out = graph->output();
	graph->connect(node->output("Emission"), out->input("Surface"));
	light_shader->set_graph(graph);
	light_shader->tag_update(scene);

	scene->shaders.push_back(light_shader);

	return light_shader;
}

// common method as for xsi lights and custom lights
void sync_light_tfm(ccl::Light* light, const XSI::MATH::CMatrix4 &xsi_tfm_matrix)
{
	//XSI::MATH::CVector3 xsi_tfm_position = xsi_tfm.GetTranslation();  // translation does not changed by previous rotation, so, we can use it
	XSI::MATH::CVector3 xsi_tfm_position = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(3, 0), xsi_tfm_matrix.GetValue(3, 1), xsi_tfm_matrix.GetValue(3, 2));
	XSI::MATH::CVector3 xsi_tfm_direction = XSI::MATH::CVector3(-1 * xsi_tfm_matrix.GetValue(2, 0), -1 * xsi_tfm_matrix.GetValue(2, 1), -1 * xsi_tfm_matrix.GetValue(2, 2));

	light->set_dir(vector3_to_float3(xsi_tfm_direction));
	light->set_co(vector3_to_float3(xsi_tfm_position));

	// also here set axis u and v
	XSI::MATH::CVector3 xsi_axis_u = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(0, 0), xsi_tfm_matrix.GetValue(0, 1), xsi_tfm_matrix.GetValue(0, 2));
	XSI::MATH::CVector3 xsi_axis_v = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(1, 0), xsi_tfm_matrix.GetValue(1, 1), xsi_tfm_matrix.GetValue(1, 2));
	light->set_axisu(vector3_to_float3(xsi_axis_u));
	light->set_axisv(vector3_to_float3(xsi_axis_v));
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
	sync_light_tfm(light, xsi_tfm_matrix);
}

void sync_xsi_light(ccl::Scene* scene, ccl::Light* light, ccl::Shader* light_shader, const XSI::Light &xsi_light, const XSI::CTime &eval_time)
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
		sync_xsi_light_tfm(light, xsi_light, eval_time);

		// save connection between light object in the Softimage scene and in the Cycles scene
		update_context->add_light_index(xsi_light.GetObjectID(), scene->lights.size() - 1);
	}
}

void sync_custom_light(ccl::Light* light, const XSI::CString &xsi_name, CustomLightType light_type, const XSI::CParameterRefArray &xsi_parameters, const XSI::CTime &eval_time, XSI::CString &out_lightgroup)
{
	light->set_is_enabled(true);

	if (light_type == CustomLightType_Point)
	{
		light->set_light_type(ccl::LightType::LIGHT_POINT);
		light->set_size(xsi_parameters.GetValue("size", eval_time));
	}
	else if (light_type == CustomLightType_Sun)
	{
		light->set_light_type(ccl::LightType::LIGHT_DISTANT);
		light->set_angle(xsi_parameters.GetValue("angle", eval_time));
	}
	else if (light_type == CustomLightType_Spot)
	{
		light->set_light_type(ccl::LightType::LIGHT_SPOT);
		light->set_size(xsi_parameters.GetValue("size", eval_time));
		light->set_spot_angle(DEG2RADF((float)xsi_parameters.GetValue("spot_angle", eval_time)));
		light->set_spot_smooth(xsi_parameters.GetValue("spot_smooth", eval_time));
	}
	else if (light_type == CustomLightType_Area)
	{
		light->set_light_type(ccl::LightType::LIGHT_AREA);
		light->set_size(1.0f);
		light->set_sizeu(xsi_parameters.GetValue("sizeU", eval_time));
		light->set_sizev(xsi_parameters.GetValue("sizeV", eval_time));
		light->set_is_portal(xsi_parameters.GetValue("is_portal", eval_time));
		light->set_spread(DEG2RADF((float)xsi_parameters.GetValue("spread", eval_time)));
		XSI::CValue shape_value = xsi_parameters.GetValue("shape", eval_time);
		bool is_round = false;
		if (!shape_value.IsEmpty())
		{
			is_round = static_cast<int>(shape_value) != 0;
		}
		light->set_round(is_round);
	}

	light->set_strength(ccl::one_float3() * (float)xsi_parameters.GetValue("power", eval_time));
	light->set_cast_shadow(xsi_parameters.GetValue("cast_shadow", eval_time));
	light->set_use_mis(xsi_parameters.GetValue("multiple_importance", eval_time));
	light->set_max_bounces(xsi_parameters.GetValue("max_bounces", eval_time));
	light->set_use_camera(xsi_parameters.GetValue("use_camera", eval_time));
	light->set_use_diffuse(xsi_parameters.GetValue("use_diffuse", eval_time));
	light->set_use_glossy(xsi_parameters.GetValue("use_glossy", eval_time));
	light->set_use_transmission(xsi_parameters.GetValue("use_transmission", eval_time));
	light->set_use_scatter(xsi_parameters.GetValue("use_scatter", eval_time));
	light->set_is_shadow_catcher(xsi_parameters.GetValue("is_shadow_catcher", eval_time));
	light->set_use_caustics(xsi_parameters.GetValue("shadow_caustics", eval_time));

	light->set_random_id(ccl::hash_uint2(ccl::hash_string(xsi_name.GetAsciiString()), 0));
	out_lightgroup = xsi_parameters.GetValue("lightgroup", eval_time);
	light->set_lightgroup(ccl::ustring(out_lightgroup.GetAsciiString()));
}

void sync_custom_light_tfm(ccl::Light* light, const XSI::KinematicState &xsi_kine, const XSI::CTime& eval_time)
{
	XSI::MATH::CMatrix4 xsi_matrix = xsi_kine.GetTransform(eval_time).GetMatrix4();
	sync_light_tfm(light, xsi_matrix);
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
	// set ray visibility flags
	ccl::uint visibility = 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_camera", eval_time)) ? ccl::PATH_RAY_CAMERA : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_diffuse", eval_time)) ? ccl::PATH_RAY_DIFFUSE : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_glossy", eval_time)) ? ccl::PATH_RAY_GLOSSY : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_transmission", eval_time)) ? ccl::PATH_RAY_TRANSMIT : 0;
	visibility |= bool(render_parameters.GetValue("background_ray_visibility_scatter", eval_time)) ? ccl::PATH_RAY_VOLUME_SCATTER : 0;
	background->set_visibility(visibility);
	background->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));

	bg_shader->set_heterogeneous_volume(render_parameters.GetValue("background_volume_homogeneous", eval_time));
	int background_volume_sampling = render_parameters.GetValue("background_volume_sampling", eval_time);
	bg_shader->set_volume_sampling_method(background_volume_sampling == 2 ? ccl::VolumeSampling::VOLUME_SAMPLING_MULTIPLE_IMPORTANCE : (background_volume_sampling == 1 ? ccl::VolumeSampling::VOLUME_SAMPLING_EQUIANGULAR : ccl::VolumeSampling::VOLUME_SAMPLING_DISTANCE));
	int background_volume_interpolation = render_parameters.GetValue("background_volume_interpolation", eval_time);
	bg_shader->set_volume_interpolation_method(background_volume_interpolation == 1 ? ccl::VolumeInterpolation::VOLUME_INTERPOLATION_CUBIC : ccl::VolumeInterpolation::VOLUME_INTERPOLATION_LINEAR);
	bg_shader->set_volume_step_rate(render_parameters.GetValue("background_volume_step_rate", eval_time));
}

void set_background_light_params(ccl::Scene* scene, ccl::Light* light, ccl::Shader* bg_shader, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
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
	XSI::CString lightgroup = render_parameters.GetValue("background_lightgroup", eval_time);
	update_context->add_lightgroup(lightgroup);
	set_background_params(background, bg_shader, render_parameters, lightgroup, eval_time);

	// add background light to the scene
	int bg_index = update_context->get_background_light_index();
	ccl::Light* light;
	if (bg_index >= 0)
	{
		light = scene->lights[bg_index];
	}
	else
	{
		light = scene->create_node<ccl::Light>();
		update_context->set_background_light_index(scene->lights.size() - 1);
	}

	light->set_is_enabled(true);
	light->set_light_type(ccl::LightType::LIGHT_BACKGROUND);

	set_background_light_params(scene, light, bg_shader, render_parameters, eval_time);
}

void sync_custom_background(ccl::Scene* scene, const XSI::X3DObject &xsi_object, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
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
}

void sync_custom_lights(ccl::Scene* scene, const std::vector<XSI::X3DObject>& custom_lights, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();
	for (size_t i = 0; i < custom_lights.size(); i++)
	{
		XSI::X3DObject xsi_object = custom_lights[i];
		CustomLightType light_type = get_custom_light_type(xsi_object.GetType());
		if (light_type != CustomLightType_Unknown)
		{
			if (light_type == CustomLightType_Background)
			{
				sync_custom_background(scene, xsi_object, update_context, render_parameters, eval_time);
			}
			else
			{
				// get light shader index
				XSI::Material xsi_material = xsi_object.GetMaterial();
				ULONG xsi_material_id = xsi_material.GetObjectID();
				if (update_context->is_material_exists(xsi_material_id))
				{
					size_t shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);

					ccl::Light* light = scene->create_node<ccl::Light>();
					ccl::Shader* shader = scene->shaders[shader_index];
					light->set_shader(shader);

					XSI::CString lightgroup;
					sync_custom_light(light, xsi_object.GetUniqueName(), light_type, xsi_object.GetParameters(), eval_time, lightgroup);
					update_context->add_lightgroup(lightgroup);
					sync_custom_light_tfm(light, xsi_object.GetKinematics().GetGlobal(), eval_time);

					update_context->add_light_index(xsi_object.GetObjectID(), scene->lights.size() - 1);
				}
			}
		}
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

// called when we change ambience color of the scene
XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context)
{
	if (!update_context->get_use_background_light())
	{
		XSI::CTime eval_time = update_context->get_time();
		XSI::MATH::CColor4f ambience_color = get_xsi_ambience(eval_time);

		ccl::Shader* bg_shader = scene->default_background;
		bg_shader->set_graph(build_backgound_default_graph(ambience_color));
		bg_shader->tag_update(scene);

		// update the shader, no need to do this again
		update_context->reset_need_update_background();
	}

	return XSI::CStatus::OK;
}

// called in post scene if we change background parameters from render settings
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

		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}

XSI::CStatus update_custom_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object)
{
	ULONG xsi_id = xsi_object.GetObjectID();
	CustomLightType light_type = get_custom_light_type(xsi_object.GetType());
	if (update_context->is_xsi_light_exists(xsi_id) && light_type != CustomLightType_Unknown && light_type != CustomLightType_Background)
	{
		size_t light_index = update_context->get_xsi_light_cycles_index(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		ccl::Light* light = scene->lights[light_index];

		XSI::CString lightgroup;
		sync_custom_light(light, xsi_object.GetUniqueName(), light_type, xsi_object.GetParameters(), eval_time, lightgroup);
		update_context->add_lightgroup(lightgroup);
		light->tag_update(scene);

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
		size_t light_index = update_context->get_xsi_light_cycles_index(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		ccl::Light* light = scene->lights[light_index];
		sync_xsi_light_tfm(light, xsi_light, eval_time);
		light->tag_update(scene);

		// we may change position of the light for sun in background sky
		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}

XSI::CStatus update_custom_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object)
{
	ULONG xsi_id = xsi_object.GetObjectID();
	if (update_context->is_xsi_light_exists(xsi_id))
	{
		size_t light_index = update_context->get_xsi_light_cycles_index(xsi_id);
		XSI::CTime eval_time = update_context->get_time();

		ccl::Light* light = scene->lights[light_index];
		sync_custom_light_tfm(light, xsi_object.GetKinematics().GetGlobal(), eval_time);
		light->tag_update(scene);

		update_context->activate_need_update_background();

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}

void update_background(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	if (update_context->get_use_background_light())
	{
		size_t shader_index = update_context->get_background_shader_index();
		ULONG material_id = update_context->get_background_xsi_material_id();
		XSI::ProjectItem item = XSI::Application().GetObjectFromID(material_id);
		XSI::Material xsi_material(item);

		std::vector<XSI::CStringArray> aovs(2);
		aovs[0].Clear();
		aovs[1].Clear();
		XSI::CStatus is_update = update_material(scene, xsi_material, shader_index, update_context->get_time(), aovs);
		set_background_light(scene, scene->background, scene->default_background, update_context, render_parameters, update_context->get_time());
	}
}