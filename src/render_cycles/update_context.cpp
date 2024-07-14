#include <xsi_model.h>
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_projectitem.h>
#include <xsi_texture.h>
#include <xsi_application.h>

#include "update_context.h"
#include "../utilities/logs.h"
#include "../utilities/arrays.h"
#include "../utilities/xsi_properties.h"

UpdateContext::UpdateContext()
{
	reset();
}

UpdateContext::~UpdateContext()
{
	reset();
}

void UpdateContext::reset()
{
	prev_render_parameters.clear();

	// set to true, because reset called when we create the scene
	// in this case we say that the scene is new and render should be done
	is_update_scene = true;
	is_update_light_linking = true;

	prev_dispaly_pass_name = "";

	log_rendertime = false;
	log_details = false;

	motion_type = MotionType_None;
	motion_times.resize(0);
	motion_rolling = false;
	motion_shutter_time = 0.5f;
	motion_rolling_duration = 0.1f;
	motion_position = MotionPosition_Center;

	render_type = RenderType_Unknown;

	// does not reset full_width and full_height, because these values used as in update scene and create scene

	lights_xsi_to_cyc.clear();
	material_xsi_to_cyc.clear();
	shaderball_material_to_node.clear();
	background_light_index = -1;
	need_update_background = false;
	use_background_light = false;

	use_denoising = false;

	lightgroups.clear();
	color_aovs.clear();
	value_aovs.clear();

	xsi_light_from_instance_map.clear();
	xsi_nested_to_hosts_instances_map.clear();
	xsi_light_id_to_instance_map.clear();
	xsi_geometry_from_instance_map.clear();
	xsi_geometry_id_to_instance_map.clear();
	geometry_xsi_to_cyc.clear();
	object_xsi_to_cyc.clear();

	abort_update_transforms_ids.clear();

	displacement_mode = -1;
	primitive_shape_map.clear();
	xsi_displacement_materials.clear();
}

void UpdateContext::set_is_update_light_linking(bool value)
{
	is_update_light_linking = value;
}

void UpdateContext::set_is_update_scene(bool value)
{
	is_update_scene = value;
}

bool UpdateContext::get_is_update_light_linking()
{
	return is_update_light_linking;
}

bool UpdateContext::get_is_update_scene()
{
	return is_update_scene;
}

void UpdateContext::set_use_denoising(bool value)
{
	use_denoising = value;
}

bool UpdateContext::get_use_denoising()
{
	return use_denoising;
}

void UpdateContext::set_current_render_parameters(const XSI::CParameterRefArray& render_parameters)
{
	current_render_parameters = render_parameters;
}

XSI::CParameterRefArray UpdateContext::get_current_render_parameters()
{
	return current_render_parameters;
}

void UpdateContext::setup_prev_render_parameters(const XSI::CParameterRefArray& render_parameters)
{
	prev_render_parameters.clear();
	size_t params_count = render_parameters.GetCount();
	for (size_t i = 0; i < params_count; i++)
	{
		XSI::Parameter param = render_parameters[i];
		XSI::CString param_name = param.GetName();
		XSI::CValue param_value = param.GetValue();

		prev_render_parameters[std::string(param_name.GetAsciiString())] = param_value;
	}
}

std::unordered_set<std::string> UpdateContext::get_changed_parameters(const XSI::CParameterRefArray& render_parameters)
{
	std::unordered_set<std::string> to_return;
	size_t params_count = render_parameters.GetCount();
	for (size_t i = 0; i < params_count; i++)
	{
		XSI::Parameter param = render_parameters[i];
		std::string param_name = param.GetName().GetAsciiString();
		XSI::CValue param_value = param.GetValue();

		if (prev_render_parameters.contains(param_name))
		{
			XSI::CValue prev_value = prev_render_parameters[param_name];
			if (prev_value != param_value)
			{
				to_return.insert(param_name);
			}
		}
		else
		{
			to_return.insert(param_name);
		}
	}

	return to_return;
}

// return true if at lest one value from the set is in array
bool is_set_contains_from_array(const std::unordered_set<std::string>& parameters, const std::vector<std::string> &array)
{
	for (const std::string& value : parameters)
	{
		if (is_contains(array, value))
		{
			return true;
		}
	}
	return false;
}

bool UpdateContext::is_changed_render_parameters_only_cm(const std::unordered_set<std::string> &parameters)
{
	// this array should be the same as color management parameters in the ui
	std::vector<std::string> cm_parameters = { "cm_apply_to_ldr", "cm_mode", "cm_display_index", "cm_view_index", "cm_look_index", "cm_exposure", "cm_gamma"};
	// check are changed parameters in this list or not
	for (const auto& value : parameters)
	{
		if (!is_contains(cm_parameters, value))
		{
			// there is changed parameter not from the list, so, something other is changed
			return false;
		}
	}
	return true;
}

bool UpdateContext::is_changed_render_paramters_integrator(const std::unordered_set<std::string>& parameters)
{
	std::vector<std::string> integrator_parameters{
		"sampling_render_samples",
		"film_motion_use", 
		"paths_max_bounces", "paths_max_diffuse_bounces", "paths_max_glossy_bounces", "paths_max_transmission_bounces", "paths_max_volume_bounces", "paths_max_transparent_bounces",
		"sampling_advanced_min_light_bounces", "sampling_advanced_min_transparent_bounces",
		"performance_volume_step_rate", "performance_volume_max_steps",
		"paths_caustics_filter_glossy", "paths_caustics_reflective", "paths_caustics_refractive",
		"sampling_advanced_seed", "sampling_advanced_animate_seed",
		"paths_clamp_direct", "paths_clamp_indirect",
		"sampling_advanced_light_tree", "sampling_advanced_light_threshold",
		"sampling_render_use_adaptive", "sampling_render_adaptive_threshold", "sampling_render_adaptive_min_samples",
		"sampling_advanced_pattern",
		"sampling_advanced_scrambling_distance", "sampling_advanced_scrambling_multiplier"
		"paths_fastgi_use", "paths_fastgi_ao_factor", "paths_fastgi_ao_distance", "paths_fastgi_method", "paths_fastgi_ao_bounces",
		"sampling_path_guiding_use", "sampling_path_guiding_surface", "sampling_path_guiding_volume", "sampling_path_guiding_training_samples",
		"denoise_mode", "denoise_channels", "denoise_prefilter"
	};

	return is_set_contains_from_array(parameters, integrator_parameters);
}

bool UpdateContext::is_changed_render_paramters_film(const std::unordered_set<std::string>& parameters)
{
	std::vector<std::string> film_parameters = { "film_exposure", "film_filter_type", "film_filter_width", "mist_start", "mist_depth", "mist_falloff", "output_pass_alpha_threshold"};
	return is_set_contains_from_array(parameters, film_parameters);
}

bool UpdateContext::is_change_render_parameters_motion(const std::unordered_set<std::string>& parameters)
{
	// "film_motion_use" checked separately
	// "film_motion_rolling_type" and "film_motion_rolling_duration" requires only camera update
	std::vector<std::string> motion_parameters = { "film_motion_steps", "film_motion_position", "film_motion_shutter" };
	return is_set_contains_from_array(parameters, motion_parameters);
}

bool UpdateContext::is_change_render_parameters_camera(const std::unordered_set<std::string>& parameters)
{
	std::vector<std::string> camera_parameters = { "film_motion_rolling_type", "film_motion_rolling_duration" };
	return is_set_contains_from_array(parameters, camera_parameters);
}

bool UpdateContext::is_change_render_parameters_background(const std::unordered_set<std::string>& parameters)
{
	std::vector<std::string> background_parameters = { 
		"film_transparent", "film_transparent_glass", "film_transparent_roughness",
		"background_ray_visibility_camera", "background_ray_visibility_diffuse", "background_ray_visibility_glossy", "background_ray_visibility_transmission", "background_ray_visibility_scatter",
		"background_volume_sampling", "background_volume_interpolation", "background_volume_homogeneous", "background_volume_step_rate",
		"background_surface_sampling_method", "background_surface_max_bounces", "background_surface_resolution", "background_surface_shadow_caustics",
		"background_lightgroup"
	};
	return is_set_contains_from_array(parameters, background_parameters);
}

bool UpdateContext::is_change_render_parameters_shaders(const std::unordered_set<std::string>& parameters)
{
	std::vector<std::string> background_parameters = {
		"options_shaders_emission_sampling", "options_shaders_transparent_shadows", "options_displacement_method"
	};
	return is_set_contains_from_array(parameters, background_parameters);
}

void UpdateContext::set_prev_display_pass_name(const XSI::CString& value)
{
	prev_dispaly_pass_name = value;
}

XSI::CString UpdateContext::get_prev_display_pass_name()
{
	return prev_dispaly_pass_name;
}

void UpdateContext::set_image_size(size_t width, size_t height)
{
	full_width = width;
	full_height = height;
}

size_t UpdateContext::get_full_width()
{
	return full_width;
}

size_t UpdateContext::get_full_height()
{
	return full_height;
}

void UpdateContext::set_camera(const XSI::Camera& camera)
{
	xsi_camera = camera;
}

XSI::Camera UpdateContext::get_camera()
{
	return xsi_camera;
}

void UpdateContext::set_time(const XSI::CTime& time)
{
	eval_time.PutFormat(time.GetFormat(), time.GetFrameRate());
	eval_time.PutTime(time.GetTime());
}

XSI::CTime UpdateContext::get_time()
{
	return eval_time;
}

void UpdateContext::set_logging(bool is_rendertime, bool is_details)
{
	log_rendertime = is_rendertime;
	log_details = is_details;
}

bool UpdateContext::get_is_log_rendertime()
{
	return log_rendertime;
}

bool UpdateContext::get_is_log_details()
{
	return log_details;
}

void UpdateContext::set_motion(const XSI::CParameterRefArray& render_parameters, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel, MotionSettingsType known_type)
{
	if (known_type == MotionType_Unknown)
	{
		motion_type = get_motion_type_from_parameters(render_parameters, eval_time, output_channels, visual_channel);
	}
	else
	{
		motion_type = known_type;
	}
	
	int film_motion_steps = render_parameters.GetValue("film_motion_steps", eval_time);
	int film_motion_position = render_parameters.GetValue("film_motion_position", eval_time);
	int film_motion_rolling_type = render_parameters.GetValue("film_motion_rolling_type", eval_time);

	motion_shutter_time = render_parameters.GetValue("film_motion_shutter", eval_time);
	// as in Cycles, for pass we use constant shutter time
	if (motion_type == MotionType_Pass)
	{
		motion_shutter_time = 2.0;
	}
	motion_rolling = film_motion_rolling_type == 1;
	motion_rolling_duration = render_parameters.GetValue("film_motion_rolling_duration", eval_time);

	int motion_steps = (2 << (film_motion_steps - 1)) + 1;
	motion_position = film_motion_position == 0 ? MotionPosition_Start : (film_motion_position == 1 ? MotionPosition_Center : MotionPosition_End);
	
	motion_times.resize(motion_steps, 0.0);
	// calculate actual frames for motion steps
	double center_time = eval_time.GetTime();
	double center_delta = 0.0;
	// use motion position parameter only for motion blur mode (for motion pass always use center)
	if (motion_type != MotionType_Pass)
	{
		if (motion_position == MotionPosition_End)
		{
			center_delta = -motion_shutter_time * 0.5;
		}
		else if (motion_position == MotionPosition_Start)
		{
			center_delta = motion_shutter_time * 0.5;
		}
	}
	double frame_time = center_time + center_delta;
	double step_time = motion_shutter_time / (double)(motion_steps - 1);
	for (size_t i = 0; i < motion_steps; i++)
	{
		double moment_time = frame_time - 0.5 * motion_shutter_time + (double)i * step_time;
		motion_times[i] = moment_time;
	}
}

void UpdateContext::set_motion_type(MotionSettingsType value)
{
	motion_type = value;
}

bool UpdateContext::get_need_motion()
{
	return !(motion_type == MotionType_None);
}

MotionSettingsType UpdateContext::get_motion_type()
{
	return motion_type;
}

size_t UpdateContext::get_motion_steps()
{
	return motion_times.size();
}

MotionSettingsPosition UpdateContext::get_motion_position()
{
	return motion_position;
}

float UpdateContext::get_motion_shutter_time()
{
	return motion_shutter_time;
}

bool UpdateContext::get_motion_rolling()
{
	return motion_rolling;
}

void UpdateContext::set_motion_rolling(bool value)
{
	motion_rolling = value;
}

float UpdateContext::get_motion_rolling_duration()
{
	return motion_rolling_duration;
}

void UpdateContext::set_motion_rolling_duration(float value)
{
	motion_rolling_duration = value;
}

double UpdateContext::get_motion_fisrt_time()
{
	return motion_times[0];
}

double UpdateContext::get_motion_last_time()
{
	return motion_times[motion_times.size() - 1];
}

std::vector<double> UpdateContext::get_motion_times()
{
	if (get_need_motion())
	{
		return motion_times;
	}
	else
	{
		return  { (float)eval_time.GetTime() };
	}
}

double UpdateContext::get_motion_time(size_t step)
{
	return motion_times[step];
}

size_t UpdateContext::get_main_motion_step()
{
	if (get_need_motion())
	{
		if (motion_position == MotionSettingsPosition::MotionPosition_Start)
		{
			return 0;
		}
		else if (motion_position == MotionSettingsPosition::MotionPosition_End)
		{
			return motion_times.size() - 1;
		}
		else
		{// center
			return motion_times.size() / 2;
		}
	}
	else
	{
		return 0;
	}
}

void UpdateContext::set_render_type(RenderType value)
{
	render_type = value;
}

RenderType UpdateContext::get_render_type()
{
	return render_type;
}

void UpdateContext::add_light_index(ULONG xsi_light_id, size_t cyc_light_index)
{
	if (lights_xsi_to_cyc.contains(xsi_light_id))
	{
		lights_xsi_to_cyc[xsi_light_id].push_back(cyc_light_index);
	}
	else
	{
		std::vector<size_t> array(1, cyc_light_index);
		lights_xsi_to_cyc[xsi_light_id] = array;
	}
}

bool UpdateContext::is_xsi_light_exists(ULONG xsi_id)
{
	return lights_xsi_to_cyc.contains(xsi_id);
}

std::vector<size_t> UpdateContext::get_xsi_light_cycles_indexes(ULONG xsi_id)
{
	return lights_xsi_to_cyc[xsi_id];
}

std::vector<ULONG> UpdateContext::get_xsi_light_ids()
{
	std::vector<ULONG> xsi_ids;
	xsi_ids.reserve(lights_xsi_to_cyc.size());

	for (auto kv : lights_xsi_to_cyc)
	{
		xsi_ids.push_back(kv.first);
	}
	return xsi_ids;
}

void UpdateContext::add_geometry_index(ULONG xsi_id, size_t cyc_geo_index)
{
	geometry_xsi_to_cyc[xsi_id] = cyc_geo_index;
}

bool UpdateContext::is_geometry_exists(ULONG xsi_id)
{
	return geometry_xsi_to_cyc.contains(xsi_id);
}

size_t UpdateContext::get_geometry_index(ULONG xsi_id)
{
	return geometry_xsi_to_cyc[xsi_id];
}

void UpdateContext::add_object_index(ULONG xsi_id, size_t cyc_index)
{
	if (object_xsi_to_cyc.contains(xsi_id))
	{
		object_xsi_to_cyc[xsi_id].push_back(cyc_index);
	}
	else
	{
		object_xsi_to_cyc[xsi_id] = { cyc_index };
	}
}

bool UpdateContext::is_object_exists(ULONG xsi_id)
{
	return object_xsi_to_cyc.contains(xsi_id);
}

std::vector<size_t> UpdateContext::get_object_cycles_indexes(ULONG xsi_id)
{
	return object_xsi_to_cyc[xsi_id];
}

std::vector<ULONG> UpdateContext::get_xsi_object_ids()
{
	std::vector<ULONG> xsi_ids;
	xsi_ids.reserve(object_xsi_to_cyc.size());

	for (auto kv : object_xsi_to_cyc)
	{
		xsi_ids.push_back(kv.first);
	}
	return xsi_ids;
}

bool UpdateContext::get_use_background_light()
{
	return use_background_light;
}

void UpdateContext::set_use_background_light(size_t shader_index, ULONG material_id)
{
	use_background_light = true;
	background_shader_index = shader_index;
	background_xsi_material_id = material_id;
}

void UpdateContext::reset_need_update_background()
{
	need_update_background = false;
}

void UpdateContext::activate_need_update_background()
{
	need_update_background = true;
}

bool UpdateContext::is_need_update_background()
{
	return need_update_background;
}

void UpdateContext::set_background_light_index(int value)
{
	background_light_index = value;
}

int UpdateContext::get_background_light_index()
{
	return background_light_index;
}

void UpdateContext::add_material_index(ULONG xsi_id, size_t cyc_shader_index, bool has_displacement, ShaderballType shaderball_type)
{
	material_xsi_to_cyc[xsi_id] = cyc_shader_index;

	if (shaderball_type != ShaderballType_Material && shaderball_type != ShaderballType_Unknown)
	{
		// write also id of the host object (material, camera or light)
		// in the update process we obtain only this host object
		// so, if it non-empty, then we will update not updated object, but current shader (or texture) node
		XSI::ProjectItem item = XSI::Application().GetObjectFromID(xsi_id);
		XSI::CRef parent;
		if (shaderball_type == ShaderballType_SurfaceShader || shaderball_type == ShaderballType_VolumeShader)
		{
			XSI::Shader xsi_shader(item);
			parent = xsi_shader.GetRoot();
		}
		else if (shaderball_type == ShaderballType_Texture)
		{
			XSI::Texture xsi_texture(item);
			parent = xsi_texture.GetRoot();
		}

		ULONG parent_id = 0;
		XSI::siClassID parent_class = parent.GetClassID();
		if (parent_class == XSI::siMaterialID)
		{
			XSI::Material parent_material(parent);
			parent_id = parent_material.GetObjectID();
		}
		if (parent_id > 0)
		{
			shaderball_material_to_node[parent_id] = xsi_id;
		}
	}

	if (has_displacement && displacement_mode != 0)
	{
		xsi_displacement_materials.insert(xsi_id);
	}
}

bool UpdateContext::is_material_exists(ULONG xsi_id)
{
	return material_xsi_to_cyc.contains(xsi_id);
}

size_t UpdateContext::get_xsi_material_cycles_index(ULONG xsi_id)
{
	return material_xsi_to_cyc[xsi_id];
}

ULONG UpdateContext::get_shaderball_material_node(ULONG material_id)
{
	if (shaderball_material_to_node.contains(material_id))
	{
		return shaderball_material_to_node[material_id];
	}
	else
	{
		return 0;
	}
}

size_t UpdateContext::get_background_shader_index()
{
	return background_shader_index;
}

ULONG UpdateContext::get_background_xsi_material_id()
{
	return background_xsi_material_id;
}

void UpdateContext::add_lightgroup(const XSI::CString& name)
{
	if (name.Length() > 0)
	{
		lightgroups.insert(std::string(name.GetAsciiString()));
	}
}

XSI::CStringArray UpdateContext::get_lightgropus()
{
	XSI::CStringArray to_return;
	for (std::string v : lightgroups)
	{
		to_return.Add(XSI::CString(v.c_str()));
	}

	return to_return;
}

void UpdateContext::add_aov_names(const XSI::CStringArray& in_color_aovs, const XSI::CStringArray& in_value_aovs)
{
	for (ULONG i = 0; i < in_color_aovs.GetCount(); i++)
	{
		std::string new_color = std::string(in_color_aovs[i].GetAsciiString());
		if (new_color.length() > 0)
		{
			color_aovs.insert(new_color);
		}
	}

	for (ULONG i = 0; i < in_value_aovs.GetCount(); i++)
	{
		std::string new_value = std::string(in_value_aovs[i].GetAsciiString());
		if (new_value.length() > 0)
		{
			value_aovs.insert(new_value);
		}
	}
}

XSI::CStringArray UpdateContext::get_color_aovs()
{
	XSI::CStringArray to_return;
	for (std::string v : color_aovs)
	{
		to_return.Add(XSI::CString(v.c_str()));
	}

	return to_return;
}

XSI::CStringArray UpdateContext::get_value_aovs()
{
	XSI::CStringArray to_return;
	for (std::string v : value_aovs)
	{
		to_return.Add(XSI::CString(v.c_str()));
	}

	return to_return;
}

// master_ids is array [m1, o1, m2, o2, ...]
// where mi and oi are master root and master object
// this array has length > 2 if there are nested prefabs
// in this case m1, o1 defines top level, m2, o2 next nested level and so on
// at the end tow last values are the final master root id and master object (which is actualy exported)
void UpdateContext::add_light_instance_data(ULONG xsi_instance_root_id, size_t cycles_light_index, std::vector<ULONG> master_ids)
{
	if (xsi_light_from_instance_map.contains(xsi_instance_root_id))
	{
		xsi_light_from_instance_map[xsi_instance_root_id][cycles_light_index].insert(xsi_light_from_instance_map[xsi_instance_root_id][cycles_light_index].end(), master_ids.begin(), master_ids.end());
	}
	else
	{
		std::unordered_map<size_t, std::vector<ULONG>> new_map;
		new_map[cycles_light_index].insert(new_map[cycles_light_index].end(), master_ids.begin(), master_ids.end());

		xsi_light_from_instance_map[xsi_instance_root_id] = new_map;
	}

	ULONG master_object_id = master_ids[master_ids.size() - 1];
	if (xsi_light_id_to_instance_map.contains(master_object_id))
	{
		xsi_light_id_to_instance_map[master_object_id].push_back(xsi_instance_root_id);
	}
	else
	{
		xsi_light_id_to_instance_map[master_object_id] = { xsi_instance_root_id };
	}
}

bool UpdateContext::is_light_from_instance_data_contains_id(ULONG xsi_id)
{
	return xsi_light_from_instance_map.contains(xsi_id);
}

std::unordered_map<size_t, std::vector<ULONG>> UpdateContext::get_light_from_instance_data(ULONG xsi_id)
{
	return xsi_light_from_instance_map[xsi_id];
}

void UpdateContext::add_nested_instance_data(ULONG nested_id, ULONG host_id)
{
	if (xsi_nested_to_hosts_instances_map.contains(nested_id))
	{
		xsi_nested_to_hosts_instances_map[nested_id].push_back(host_id);
	}
	else
	{
		xsi_nested_to_hosts_instances_map[nested_id] = { host_id };
	}
}

bool UpdateContext::is_nested_to_host_instances_contains_id(ULONG id)
{
	return xsi_nested_to_hosts_instances_map.contains(id);
}

std::vector<ULONG> UpdateContext::get_nested_to_host_instances_ids(ULONG id)
{
	return xsi_nested_to_hosts_instances_map[id];
}

bool UpdateContext::is_light_id_to_instance_contains_id(ULONG id)
{
	return xsi_light_id_to_instance_map.contains(id);
}

std::vector<ULONG> UpdateContext::get_light_id_to_instance_ids(ULONG id)
{
	return xsi_light_id_to_instance_map[id];
}

void UpdateContext::add_geometry_instance_data(ULONG xsi_instance_root_id, size_t cycles_geometry_index, std::vector<ULONG> master_ids)
{
	if (xsi_geometry_from_instance_map.contains(xsi_instance_root_id))
	{
		xsi_geometry_from_instance_map[xsi_instance_root_id][cycles_geometry_index].insert(xsi_geometry_from_instance_map[xsi_instance_root_id][cycles_geometry_index].end(), master_ids.begin(), master_ids.end());
	}
	else
	{
		std::unordered_map<size_t, std::vector<ULONG>> new_map;
		new_map[cycles_geometry_index].insert(new_map[cycles_geometry_index].end(), master_ids.begin(), master_ids.end());

		xsi_geometry_from_instance_map[xsi_instance_root_id] = new_map;
	}

	ULONG master_object_id = master_ids[master_ids.size() - 1];
	if (xsi_geometry_id_to_instance_map.contains(master_object_id))
	{
		xsi_geometry_id_to_instance_map[master_object_id].push_back(xsi_instance_root_id);
	}
	else
	{
		xsi_geometry_id_to_instance_map[master_object_id] = { xsi_instance_root_id };
	}
}

bool UpdateContext::is_geometry_from_instance_data_contains_id(ULONG xsi_id)
{
	return xsi_geometry_from_instance_map.contains(xsi_id);
}

std::unordered_map<size_t, std::vector<ULONG>> UpdateContext::get_geometry_from_instance_data(ULONG xsi_id)
{
	return xsi_geometry_from_instance_map[xsi_id];
}

bool UpdateContext::is_geometry_id_to_instance_contains_id(ULONG id)
{
	return xsi_geometry_id_to_instance_map.contains(id);
}

std::vector<ULONG> UpdateContext::get_geometry_id_to_instance_ids(ULONG id)
{
	return xsi_geometry_id_to_instance_map[id];
}

void UpdateContext::add_abort_update_transform_id(ULONG id)
{
	abort_update_transforms_ids.insert(id);
}

void UpdateContext::add_abort_update_transform_id(const XSI::CRefArray& ref_array)
{
	ULONG objects_count = ref_array.GetCount();
	for (ULONG i = 0; i < objects_count; i++)
	{
		XSI::X3DObject object(ref_array[i]);
		if (object.IsValid())
		{
			add_abort_update_transform_id(object.GetObjectID());
		}
	}
}

bool UpdateContext::is_abort_update_transform_id_exist(ULONG id)
{
	return abort_update_transforms_ids.contains(id);
}

void UpdateContext::add_primitive_shape(XSI::siICEShapeType shape_type, size_t shader_index, size_t mesh_index)
{
	primitive_shape_map[std::make_pair(shape_type, shader_index)] = mesh_index;
}

bool UpdateContext::is_primitive_shape_exists(XSI::siICEShapeType shape_type, size_t shader_index)
{
	return primitive_shape_map.contains(std::make_pair(shape_type, shader_index));
}

size_t UpdateContext::get_primitive_shape(XSI::siICEShapeType shape_type, size_t shader_index)
{
	return primitive_shape_map[std::make_pair(shape_type, shader_index)];
}

bool UpdateContext::is_displacement_material(ULONG xsi_id)
{
	return xsi_displacement_materials.contains(xsi_id);
}

void UpdateContext::set_displacement_mode(int in_mode)
{
	displacement_mode = in_mode;
}

int UpdateContext::get_displacement_mode()
{
	return displacement_mode;
}