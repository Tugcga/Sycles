#pragma once
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_time.h>
#include <xsi_property.h>
#include <xsi_shader.h>

bool is_render_visible(XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
bool get_xsi_object_property(XSI::X3DObject& xsi_object, const XSI::CString& property_name, XSI::Property& out_property);
bool obtain_subsub_directions(const XSI::Shader& xsi_shader, float& sun_x, float& sun_y, float& sun_z);