#include "update_context.h"
#include "../utilities/logs.h"
#include "../utilities/arrays.h"

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
	for (const auto& value : parameters)
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