#pragma once
#include <unordered_map>

#include "scene/scene.h"
#include "session/session.h"
#include "session/output_driver.h"

#include "../render_base/render_engine_base.h"
#include "cyc_output/output_context.h"
#include "cyc_scene/cyc_labels.h"
#include "cyc_output/color_transform_context.h"
#include "update_context.h"

class RenderEngineCyc : public RenderEngineBase 
{
public:
	RenderEngineCyc();
	~RenderEngineCyc();

	//ui events
	XSI::CStatus render_option_define(XSI::CustomProperty& property);
	XSI::CStatus render_option_define_layout(XSI::Context& context);
	XSI::CStatus render_options_update(XSI::PPGEventContext& event_context);

	//render events
	XSI::CStatus pre_render_engine();  // this called before the xsi scene is locked, after skiping existed images
	XSI::CStatus pre_scene_process();  // this method is called after xsi scene is locked
	XSI::CStatus create_scene();
	XSI::CStatus post_scene();
	void render();
	XSI::CStatus post_render_engine();

	//update scene events
	XSI::CStatus update_scene(XSI::X3DObject& xsi_object, const UpdateType update_type);
	XSI::CStatus update_scene(XSI::SIObject& si_object, const UpdateType update_type);
	XSI::CStatus update_scene(XSI::Material& xsi_material, bool material_assigning);
	XSI::CStatus update_scene_render();
	
	void abort();
	void clear_engine();

	void update_render_tile(const ccl::OutputDriver::Tile& tile);

	void path_init(const XSI::CString &plugin_path);

private:
	// internal variables
	ccl::Session* session;
	bool is_session;
	bool call_abort_render;  // activate when we obtain command from XSI to abort the render, use it in the cancel callback
	bool create_new_scene;  // set true if in the current session we create new scene from scratch
	bool is_update_camera;  // at start it false, but if we update camera, then turn it true
	MotionSettingsType in_update_motion_type;
	OutputContext* output_context;
	LabelsContext* labels_context;
	ColorTransformContext* color_transform_context;
	UpdateContext* update_context;
	bool make_render;
	bool is_recreate_session;  // if true, then we should create new session (and scene, even if the scene is not changed or properly updated)
	XSI::CString display_pass_name;
	ccl::SessionParams session_params;  // we create these parameters every render call at the start and check, should we recreate session or not. If yes, use these parameters
	ccl::SceneParams scene_params;
	XSI::CString temp_path;  // create it at render start and delete after the render is finish (this folder used by session to store internal render tiles)

	// internal methods
	void clear_session();
	void progress_update_callback();
	void progress_cancel_callback();  // called from Cycles to check is it should stop render or not
	void postrender_visual_output();
};