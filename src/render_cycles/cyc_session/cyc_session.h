#pragma once
#include "session/session.h"

#include <xsi_renderercontext.h>

#include "../cyc_output/output_context.h"

ccl::Session* create_session();
ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height);

// cyc_session
void sync_session(ccl::Session *session, XSI::RendererContext& xsi_render_context, OutputContext* output_context);

// cyc_pass
// convert pass string (from xsi) to actual Cycles PassType
ccl::PassType channel_to_pass_type(const XSI::CString& channel_name);
// return default name of the pass
XSI::CString pass_to_name(ccl::PassType pass_type);
// return the number of components in the output for given pass type
int get_pass_components(ccl::PassType pass_type);
void sync_passes(ccl::Scene* scene, OutputContext* output_context);