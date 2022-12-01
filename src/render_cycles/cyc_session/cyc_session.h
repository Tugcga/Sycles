#pragma once
#include "session/session.h"
#include "scene/scene.h"

#include <xsi_renderercontext.h>

#include "../cyc_output/output_context.h"
#include "../../render_base/render_visual_buffer.h"
#include "../../render_cycles/update_context.h"

ccl::Session* create_session();
ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height);

// cyc_session
ccl::SessionParams get_session_params();
ccl::SceneParams get_scene_params();
void sync_session(ccl::Session *session, UpdateContext* update_context, OutputContext* output_context, RenderVisualBuffer* visual_buffer);

// cyc pass
void sync_passes(ccl::Scene* scene, OutputContext* output_context, RenderVisualBuffer* visual_buffer);