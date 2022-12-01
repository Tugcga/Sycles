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

bool UpdateContext::is_changed_render_parameters_only_cm(const std::unordered_set<std::string> &parameters)
{
	// this array should be the same as color management parameters in the ui
	std::vector<std::string> cm_parameters = { "cm_apply_to_ldr", "cm_mode", "cm_display_index", "cm_view_index", "cm_look_index", "cm_exposure", "cm_gamma"};
	// check are changed parameters in this list or not
	for (const auto& value : parameters)
	{
		if (!is_contains(cm_parameters, value))
		{
			return false;
		}
	}
	return true;
}

void UpdateContext::set_prev_display_pass_name(const XSI::CString& value)
{
	prev_dispaly_pass_name = value;
}

XSI::CString UpdateContext::get_prev_display_pass_name()
{
	return prev_dispaly_pass_name;
}