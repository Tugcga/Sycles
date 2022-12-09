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

	scene_xsi_lights.resize(0);
	scene_custom_lights.resize(0);
	scene_polymeshes.resize(0);

	// does not reset full_width and full_height, because these values used as in update scene and create scene
}

void UpdateContext::set_is_update_scene(bool value)
{
	is_update_scene = value;
}

bool UpdateContext::get_is_update_scene()
{
	return is_update_scene;
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
	// TODO: if we switch motion or change motion blur params, then we should recreate the session, because all objects from the scene should be updated
	std::vector<std::string> integrator_parameters{
		"sampling_render_samples",
		"film_motion_use", 
		"paths_max_bounces", "paths_max_diffuse_bounces", "paths_max_glossy_bounces", "paths_max_transmission_bounces", "paths_max_volume_bounces", "paths_max_transparent_bounces",
		"sampling_advanced_min_light_bounces", "sampling_advanced_min_transparent_bounces",
		"performance_volume_step_rate", "performance_volume_max_steps",
		"paths_caustics_filter_glossy", "paths_caustics_reflective", "paths_caustics_refractive",
		"sampling_advanced_seed", "sampling_advanced_animate_seed",
		"paths_clamp_direct", "paths_clamp_indirect",
		"sampling_advanced_light_threshold",
		"sampling_render_use_adaptive", "sampling_render_adaptive_threshold", "sampling_render_adaptive_min_samples",
		"sampling_advanced_pattern",
		"sampling_advanced_scrambling_distance", "sampling_advanced_scrambling_multiplier"
		"paths_fastgi_use", "paths_fastgi_ao_factor", "paths_fastgi_ao_distance", "paths_fastgi_method", "paths_fastgi_ao_bounces",
		"sampling_path_guiding_use", "sampling_path_guiding_surface", "sampling_path_guiding_volume", "sampling_path_guiding_training_samples"
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
		"background_surface_sampling_method", "background_surface_max_bounces", "background_surface_resolution", "background_surface_shadow_caustics"
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
	eval_time = time;
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

void UpdateContext::set_motion(const XSI::CParameterRefArray& render_parameters, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel, MotionType known_type)
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
		motion_shutter_time = 2.0f;
	}
	motion_rolling = film_motion_rolling_type == 1;
	motion_rolling_duration = render_parameters.GetValue("film_motion_rolling_duration", eval_time);

	int motion_steps = (2 << (film_motion_steps - 1)) + 1;
	motion_position = film_motion_position == 0 ? MotionPosition_Start : (film_motion_position == 1 ? MotionPosition_Center : MotionPosition_End);
	
	motion_times.resize(motion_steps, 0.0f);
	// calculate actual frames for motion steps
	double center_time = eval_time.GetTime();
	float center_delta = 0.0;
	// use motion position parameter only for motion blur mode (for motion pass always use center)
	if (motion_type != MotionType_Pass)
	{
		if (motion_position == MotionPosition_End)
		{
			center_delta = -motion_shutter_time * 0.5f;
		}
		else if (motion_position == MotionPosition_Start)
		{
			center_delta = motion_shutter_time * 0.5f;
		}
	}
	float frame_time = center_time + center_delta;
	float step_time = motion_shutter_time / (float)(motion_steps - 1);
	for (size_t i = 0; i < motion_steps; i++)
	{
		float moment_time = frame_time - 0.5 * motion_shutter_time + i * step_time;
		motion_times[i] = moment_time;
	}
}

void UpdateContext::set_motion_type(MotionType value)
{
	motion_type = value;
}

bool UpdateContext::get_need_motion()
{
	return !(motion_type == MotionType_None);
}

MotionType UpdateContext::get_motion_type()
{
	return motion_type;
}

size_t UpdateContext::get_motion_steps()
{
	return motion_times.size();
}

MotionPosition UpdateContext::get_motion_position()
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

float UpdateContext::get_motion_fisrt_time()
{
	return motion_times[0];
}

float UpdateContext::get_motion_last_time()
{
	return motion_times[motion_times.size() - 1];
}

const std::vector<float>& UpdateContext::get_motion_times()
{
	return motion_times;
}

float UpdateContext::get_motion_time(size_t step)
{
	return motion_times[step];
}

void UpdateContext::set_render_type(RenderType value)
{
	render_type = value;
}

RenderType UpdateContext::get_render_type()
{
	return render_type;
}

void UpdateContext::setup_scene_objects(const XSI::CRefArray& isolation_list, const XSI::CRefArray& lights_list, const XSI::CRefArray& scene_list, const XSI::CRefArray& all_objects_list)
{
	use_background_light = false;
	// in this method we should parse input scene objects and sort it by different arrays
	if (render_type == RenderType_Shaderball)
	{
		// for renderball we use scene_list
	}
	else
	{
		// for other render modes we use other input arrays
		if (isolation_list.GetCount() > 0)
		{// render isolation view
			// we should use all objects from isolation list and all light objects (build-in and custom) from all objects list

		}
		else
		{// render general scene view
			// in this case we should enumerate objects from complete list
			size_t objects_count = all_objects_list.GetCount();
			for (size_t i = 0; i < objects_count; i++)
			{
				XSI::CRef object_ref = all_objects_list[i];
				XSI::siClassID object_class = object_ref.GetClassID();
				if (object_class == XSI::siLightID)
				{// built-in light
					XSI::X3DObject xsi_object(object_ref);
					if (is_render_visible(xsi_object, eval_time))
					{
						scene_xsi_lights.push_back((XSI::Light)object_ref);
					}
				}
				else if (object_class == XSI::siX3DObjectID)
				{
					XSI::X3DObject xsi_object(object_ref);
					XSI::CString object_type = xsi_object.GetType();
					if (object_type == "polymsh")
					{
						scene_polymeshes.push_back(xsi_object);
					}
					else if (object_type == "cyclesPoint")  // types of custom lights, TODO: addd another supported types (or change their names)
					{
						scene_custom_lights.push_back(xsi_object);
					}
				}
				else
				{
					
				}
			}
		}
	}
}

const std::vector<XSI::Light>& UpdateContext::get_xsi_lights()
{
	return scene_xsi_lights;
}

void UpdateContext::add_light_index(ULONG xsi_light_id, size_t cyc_light_index)
{
	lights_xsi_to_cyc[xsi_light_id] = cyc_light_index;
}

bool UpdateContext::is_xsi_light_exists(ULONG xsi_id)
{
	return lights_xsi_to_cyc.contains(xsi_id);
}

size_t UpdateContext::get_xsi_light_cycles_index(ULONG xsi_id)
{
	return lights_xsi_to_cyc[xsi_id];
}

bool UpdateContext::get_use_background_light()
{
	return use_background_light;
}

void UpdateContext::set_background_light_index(size_t value)
{
	background_light_index = value;
}