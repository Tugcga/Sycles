#include <chrono>
#include <thread>

#include "xsi_primitive.h"
#include "xsi_application.h"
#include "xsi_project.h"
#include "xsi_library.h"
#include "xsi_renderchannel.h"
#include "xsi_framebuffer.h"
#include "xsi_projectitem.h"

#include "render_engine_cyc.h"
#include "../utilities/logs.h"

using namespace std::chrono_literals;

RenderEngineCyc::RenderEngineCyc()
{
	
}

//when we delete the engine, then at first this method is called, and then the method from base class
RenderEngineCyc::~RenderEngineCyc()
{

}

//nothing to do here
XSI::CStatus RenderEngineCyc::pre_render_engine()
{
	return XSI::CStatus::OK;
}

//called every time before scene update
//here we should setup parameters, which should be done for both situations: create scene from scratch or update the scene
XSI::CStatus RenderEngineCyc::pre_scene_process()
{
	return XSI::CStatus::OK;
}

//return OK, if object successfully updates, Abort in other case
XSI::CStatus RenderEngineCyc::update_scene(XSI::X3DObject& xsi_object, const UpdateType update_type)
{
	return XSI::CStatus::Abort;

}

XSI::CStatus RenderEngineCyc::update_scene(XSI::SIObject& si_object, const UpdateType update_type)
{
	return XSI::CStatus::Abort;
}

XSI::CStatus RenderEngineCyc::update_scene(XSI::Material& xsi_material, bool material_assigning)
{
	return XSI::CStatus::Abort;
	
}

XSI::CStatus RenderEngineCyc::update_scene_render()
{
	return XSI::CStatus::OK;
}

//here we create the scene for rendering from scratch
XSI::CStatus RenderEngineCyc::create_scene()
{
	return XSI::CStatus::OK;
}

//call this method after scene created or updated but before unlock
XSI::CStatus RenderEngineCyc::post_scene()
{
	return XSI::CStatus::OK;
}

void RenderEngineCyc::render()
{
	
}

XSI::CStatus RenderEngineCyc::post_render_engine()
{
	//log render time
	double time = (finish_render_time - start_prepare_render_time) / CLOCKS_PER_SEC;
	if (render_type != RenderType_Shaderball)
	{
		log_message("Render statistics: " + XSI::CString(time) + " seconds");
	}

	return XSI::CStatus::OK;
}

void RenderEngineCyc::abort()
{

}

void RenderEngineCyc::clear_engine()
{

}