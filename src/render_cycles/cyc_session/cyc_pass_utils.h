#pragma once
#include <xsi_application.h>

#include "scene/pass.h"

#include "cyc_baking.h"

// implemented in cyc_pass
XSI::CString get_name_for_motion_display_channel();
XSI::CString get_name_for_motion_output_channel();
XSI::CString channel_name_to_pass_name(const XSI::CParameterRefArray& render_parameters, const XSI::CString& channel_name, const XSI::CTime& eval_time);
XSI::CString add_prefix_to_aov_name(const XSI::CString& name, bool is_color);
XSI::CString add_prefix_to_lightgroup_name(const XSI::CString& name);
XSI::CString remove_prefix_from_aov_name(const XSI::CString& name);
XSI::CString remove_prefix_from_lightgroup_name(const XSI::CString& name);
// convert pass string (from xsi) to actual Cycles PassType
ccl::PassType channel_to_pass_type(const XSI::CString& channel_name);
// return default name of the pass
XSI::CString pass_to_name(ccl::PassType pass_type);
// return the number of components in the output for given pass type
int get_pass_components(ccl::PassType pass_type, bool is_lightgroup);
ccl::PassType convert_baking_pass(ccl::PassType input_pass, BakingContext* baking_context);