#pragma once
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_time.h>
#include <xsi_property.h>
#include <xsi_shader.h>
#include <xsi_vector3.h>

#include <string>

// ignore hide master only for instances
// for all other objects this ignore should be false
bool is_render_visible(XSI::X3DObject& xsi_object, bool ignore_hide_master, const XSI::CTime& eval_time);
XSI::Property get_xsi_object_property(XSI::X3DObject& xsi_object, const XSI::CString& property_name);
bool obtain_subsub_directions(const XSI::Shader& xsi_shader, float& sun_x, float& sun_y, float& sun_z, const XSI::CTime& eval_time);
std::string get_asset_name(const XSI::X3DObject& xsi_object);
XSI::MATH::CVector3 get_object_color(XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
bool is_light(const XSI::CRef& object_ref);