#pragma once
#include <xsi_application.h>

#include "scene/pass.h"

// implemented in cyc_pass
XSI::CString add_prefix_to_aov_name(const XSI::CString& name, bool is_color);
XSI::CString remove_prefix_from_aov_name(const XSI::CString& name);
// convert pass string (from xsi) to actual Cycles PassType
ccl::PassType channel_to_pass_type(const XSI::CString& channel_name);
// return default name of the pass
XSI::CString pass_to_name(ccl::PassType pass_type);
// return the number of components in the output for given pass type
int get_pass_components(ccl::PassType pass_type);