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
#include "cyc_scene/cyc_loaders/cyc_loaders.h"

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
	bool is_change_render_parameters_shaders(const std::unordered_set<std::string>& parameters);

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

	void reset_need_update_background();
	void activate_need_update_background();
	bool is_need_update_background();

	void add_light_index(ULONG xsi_light_id, size_t cyc_light_index);
	bool is_xsi_light_exists(ULONG xsi_id);
	std::vector<size_t> get_xsi_light_cycles_indexes(ULONG xsi_id);

	bool get_use_background_light();
	void set_use_background_light(size_t shader_index, ULONG material_id);  // activate use backgound light
	void set_background_light_index(int value);
	int get_background_light_index();
	size_t get_background_shader_index();
	ULONG get_background_xsi_material_id();

	void add_material_index(ULONG xsi_id, size_t cyc_shader_index, ShaderballType shaderball_type);
	bool is_material_exists(ULONG xsi_id);
	size_t get_xsi_material_cycles_index(ULONG xsi_id);
	ULONG get_shaderball_material_node(ULONG material_id);

	void add_lightgroup(const XSI::CString& name);
	XSI::CStringArray get_lightgropus();
	void add_aov_names(const XSI::CStringArray &in_color_aovs, const XSI::CStringArray &in_value_aovs);
	XSI::CStringArray get_color_aovs();
	XSI::CStringArray get_value_aovs();

	void add_light_instance_data(ULONG xsi_instance_root_id, size_t cycles_light_index, std::vector<ULONG> master_ids);
	bool is_light_from_instance_data_contains_id(ULONG xsi_id);
	std::unordered_map<size_t, std::vector<ULONG>> get_light_from_instance_data(ULONG xsi_id);
	void add_light_nested_instance_data(ULONG nested_id, ULONG host_id);
	bool is_light_nested_to_host_instances_contains_id(ULONG id);
	std::vector<ULONG> get_light_nested_to_host_instances_ids(ULONG id);
	bool is_light_id_to_instance_contains_id(ULONG id);
	std::vector<ULONG> get_light_id_to_instance_ids(ULONG id);

private:
	// after each render prepare session we store here used render parameter values
	// this map allows to check what parameter is changed from the previous rendere session
	std::unordered_map<std::string, XSI::CValue> prev_render_parameters;

	// set true if we update scene in this render session
	// at the very start it set to false, but if something should be changed, then is set to true
	// if it false, then we should not render, because the visual buffer already contains rendered image
	bool is_update_scene;

	// set true if we change something in the scene and this may effect to the backgound
	// set false in every pre scene process
	bool need_update_background;

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

	std::unordered_set<std::string> lightgroups;
	std::unordered_set<std::string> color_aovs;
	std::unordered_set<std::string> value_aovs;

	bool use_background_light;  // set true when we use background light source from the scene, if false, then use only ambient color
	size_t background_shader_index;  // index in the Cycles array of shaders
	ULONG background_xsi_material_id;  // material id from library, used for custom backround light source
	int background_light_index;  // store here background light index in the Cycles array (we always create background light, from scene on manual), when no light is assigned, then -1

	// map from Softimage object id for Light (not for x3dobject) to index in the Cycles array of lights
	// for custm light from x3dobject id to cycles index
	// value is array, it contains indexes in Cycles lights array of one master light in scene and several instances
	// when we change parameterse of the master light, then we should resync all lights in Cycles array with thiese indexes
	// this is a map from master light id to different cycles light indexes
	std::unordered_map<ULONG, std::vector<size_t>> lights_xsi_to_cyc;

	// this map from instance root id to a map, which contains data about cycles light indexes and corresponded xsi master root ids and xsi master object ids (in this order)
	// this map should be used when we change transform of the instance root object (instance objects can not moved)
	// then we obtain all key of the corresponding map - these will be cycles light indices
	// and then for each cycles light index obtain master root id and master object id
	// obtain corresponding xsi objects and extract valid transforms
	std::unordered_map<ULONG, std::unordered_map<size_t, std::vector<ULONG>>> xsi_light_from_instance_map;

	// key - id of the instance,
	// value - ids of all instances, where the key is object, store only the top level of the instance
	std::unordered_map<ULONG, std::vector<ULONG>> xsi_light_nested_to_hosts_instances_map;

	// key - id light object in the scene
	// value - array of all instances, whcich contains this light
	std::unordered_map<ULONG, std::vector<ULONG>> xsi_light_id_to_instance_map;

	// key - object id, value - index of the shader
	// for shaderball material we use object id of the Node for shaderball (material, shader or texture)
	std::unordered_map<ULONG, size_t> material_xsi_to_cyc;
	
	std::unordered_map<ULONG, ULONG> shaderball_material_to_node;  // store map from parent material id to shader node id inside this material (only for shaderball session)
};