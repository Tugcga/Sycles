#pragma once
#include <xsi_application.h>
#include <xsi_arrayparameter.h>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class UpdateContext
{
public:
	UpdateContext();
	~UpdateContext();

	void setup_prev_render_parameters(const XSI::CParameterRefArray &render_parameters);

	// return the list of all render parameter names, changed from the last render session (or full list of parameters, if there are no any previous renders)
	std::unordered_set<std::string> get_changed_parameters(const XSI::CParameterRefArray& render_parameters);

	void reset();

	void set_is_update_scene(bool value);

	bool get_is_update_scene();

	// return true if only color management parameters changed in render settings
	bool is_changed_render_parameters_only_cm(const std::unordered_set<std::string>& parameters);

	void set_prev_display_pass_name(const XSI::CString &value);

	XSI::CString get_prev_display_pass_name();

private:
	// after each render prepare session we store here used render parameter values
	// this map allows to check what parameter is changed from the previous rendere session
	std::unordered_map<std::string, XSI::CValue> prev_render_parameters;

	// set true if we update scene in this render session
	// at the very start it set to false, but if something should be changed, then is set to true
	// if it false, then we should not render, because the visual buffer already contains rendered image
	bool is_update_scene;

	// the name of the display channel from previous render call
	XSI::CString prev_dispaly_pass_name;
};