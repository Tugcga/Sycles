#pragma once
#include "session/session.h"
#include "scene/scene.h"

#include <xsi_renderercontext.h>
#include <xsi_arrayparameter.h>

#include "../cyc_output/output_context.h"
#include "../../render_base/render_visual_buffer.h"
#include "../../render_cycles/update_context.h"
#include "../../input/input.h"
#include "cyc_baking.h"

ccl::Session* create_session(ccl::SessionParams session_params, ccl::SceneParams scene_params);
ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height);

// cyc_session
void set_session_samples(ccl::SessionParams& session_params, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);
ccl::SessionParams get_session_params(RenderType render_type, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);
ccl::SceneParams get_scene_params(RenderType render_type, const ccl::SessionParams& session_params, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);

// cyc pass
void sync_passes(ccl::Scene* scene, OutputContext* output_context, BakingContext* baking_context, RenderVisualBuffer* visual_buffer, MotionSettingsType motion_type, const XSI::CStringArray& lightgroups, const XSI::CStringArray& aov_color_names, const XSI::CStringArray& aov_value_names);

// cyc film
void sync_film(ccl::Session* session, UpdateContext* update_context, const XSI::CParameterRefArray &render_parameters);

// cyc integrator
void sync_integrator(ccl::Session* session, UpdateContext* update_context, BakingContext* baking_context, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const InputConfig& input_config);