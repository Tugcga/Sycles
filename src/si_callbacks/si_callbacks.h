#pragma once

#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_renderer.h>
#include <xsi_status.h>
#include <xsi_string.h>
#include <xsi_renderercontext.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

XSI::CStatus begin_render_event(XSI::RendererContext& ctxt, XSI::CStringArray& output_paths);
XSI::CStatus end_render_event(XSI::RendererContext& ctxt, XSI::CStringArray& output_paths, bool in_skipped);