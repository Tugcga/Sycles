#pragma once
#include "session/session.h"

#include <xsi_renderercontext.h>

ccl::Session* create_session();
ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height);

void sync_session(ccl::Session *session, XSI::RendererContext& xsi_render_context);