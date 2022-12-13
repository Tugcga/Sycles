#pragma once
#include <xsi_application.h>
#include <xsi_arrayparameter.h>
#include <xsi_camera.h>
#include <xsi_time.h>
#include <xsi_light.h>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "../render_cycles/cyc_session/cyc_pass_utils.h"
#include "../render_cycles/cyc_scene/cyc_motion.h"
#include "../render_base/type_enums.h"

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

	bool is_changed_render_paramters_integrator(const std::unordered_set<std::string>& parameters);
	bool is_changed_render_paramters_film(const std::unordered_set<std::string>& parameters);

	// return true if we change motion parameters and should recreate the scene
	bool is_change_render_parameters_motion(const std::unordered_set<std::string>& parameters);
	bool is_change_render_parameters_camera(const std::unordered_set<std::string>& parameters);
	bool is_change_render_parameters_background(const std::unordered_set<std::string>& parameters);

	void set_prev_display_pass_name(const XSI::CString &value);

	XSI::CString get_prev_display_pass_name();

	void set_image_size(size_t width, size_t height);
	size_t get_full_width();
	size_t get_full_height();

	void set_camera(const XSI::Camera &camera);
	XSI::Camera get_camera();

	void set_time(const XSI::CTime &time);
	XSI::CTime get_time();

	void set_logging(bool is_rendertime, bool is_details);
	bool get_is_log_rendertime();
	bool get_is_log_details();
	
	void set_motion(const XSI::CParameterRefArray& render_parameters, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel, MotionType known_type = MotionType_Unknown);
	void set_motion_type(MotionType value);
	bool get_need_motion();
	MotionType get_motion_type();
	size_t get_motion_steps();
	MotionPosition get_motion_position();
	float get_motion_shutter_time();
	bool get_motion_rolling();
	void set_motion_rolling(bool value);
	float get_motion_rolling_duration();
	void set_motion_rolling_duration(float value);
	float get_motion_fisrt_time();
	float get_motion_last_time();
	const std::vector<float>& get_motion_times();
	float get_motion_time(size_t step);

	void set_render_type(RenderType value);
	RenderType get_render_type();

	void setup_scene_objects(const XSI::CRefArray &isolation_list, const XSI::CRefArray &lights_list, const XSI::CRefArray& scene_list, const XSI::CRefArray &all_objects_list);
	const std::vector<XSI::Light>& get_xsi_lights();
	void add_light_index(ULONG xsi_light_id, size_t cyc_light_index);
	bool is_xsi_light_exists(ULONG xsi_id);
	size_t get_xsi_light_cycles_index(ULONG xsi_id);
	const XSI::X3DObject& get_shaderball();
	const std::vector<XSI::X3DObject>& get_scene_polymeshes();

	bool get_use_background_light();
	void set_background_light_index(size_t value);

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

	// width and height of the total screen
	// these values used for camera sync
	size_t full_width;
	size_t full_height;

	XSI::Camera xsi_camera;

	XSI::CTime eval_time;

	// set these values every time after scene is created (or updated)
	bool log_rendertime;
	bool log_details;

	MotionType motion_type;
	float motion_shutter_time;
	std::vector<float> motion_times;
	MotionPosition motion_position;
	bool motion_rolling;  // false means None
	float motion_rolling_duration;  // only for rolling true

	RenderType render_type;

	// prepare scene objects and form these arrays with input objects for rendering
	std::vector<XSI::Light> scene_xsi_lights;  // this array contains xsi lights
	std::vector<XSI::X3DObject> scene_custom_lights;  // this array contains custom lights
	std::vector<XSI::X3DObject> scene_polymeshes;

	bool use_background_light;  // set true when we use background light source from the scene, if false, then use only ambient color
	size_t background_light_index;  // store here background light index in the Cycles array (we always create background light, from scene on manual)
	std::unordered_map<ULONG, size_t> lights_xsi_to_cyc; // map from Softimage object id for Ligth (not for x3dobject) to index in the Cycles array of lights

	XSI::X3DObject shaderball;  // hero shaderball object
};