#pragma once

#include "../render_base/render_engine_base.h"

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
};