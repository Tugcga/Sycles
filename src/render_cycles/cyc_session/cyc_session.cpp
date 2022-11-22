#include "session/session.h"
#include "scene/pass.h"

#include <xsi_application.h>

#include "../../input/input_devices.h"
#include "../cyc_scene/cyc_scene.h"
#include "cyc_session.h"

ccl::SessionParams get_session_params()
{
	ccl::SessionParams session_params;

	session_params.background = true;
	session_params.samples = 10;

	ccl::vector<ccl::DeviceInfo> available_devices = get_available_devices();
	session_params.device = available_devices[0];

	return session_params;
}

ccl::SceneParams get_scene_params()
{
	ccl::SceneParams scene_params;
	scene_params.shadingsystem = ccl::SHADINGSYSTEM_SVM;

	return scene_params;
}

ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height)
{
	ccl::BufferParams buffer_params;

	buffer_params.full_width = full_width;
	buffer_params.full_height = full_height;
	buffer_params.full_x = offset_x;
	buffer_params.full_y = offset_y;
	buffer_params.width = width;
	buffer_params.height = height;

	buffer_params.window_width = buffer_params.width;
	buffer_params.window_height = buffer_params.height;

	return buffer_params;
}

ccl::Session* create_session()
{
	ccl::SessionParams session_params = get_session_params();
	ccl::SceneParams scene_params = get_scene_params();
	ccl::Session* session = new ccl::Session(session_params, scene_params);
	
	return session;
}

void sync_session(ccl::Session* session, XSI::RendererContext& xsi_render_context, OutputContext *output_context)
{
	sync_scene(session->scene, xsi_render_context);

	output_context->set_output_passes(XSI::CStringArray(), XSI::CStringArray(), XSI::CStringArray());
	sync_passes(session->scene, output_context);
}