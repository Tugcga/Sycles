#pragma once
#include <vector>

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_math.h>
#include <xsi_color4f.h>

#include "util/transform.h"
#include "util/math_float3.h"
#include "scene/camera.h"
#include "util/color.h"

#include "../render_base/type_enums.h"

#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))

int get_frame(const XSI::CTime& eval_time);
XSI::CString seconds_to_date(double total_seconds);
float clamp_float(float value, float min, float max);
float linear_to_srgb_float(float v);
float srgb_to_linear(float value);
unsigned char linear_to_srgb(float v);
unsigned char linear_clamp(float v);
bool equal_floats(float a, float b);
ccl::Transform get_transform(std::vector<float>& array);
ccl::Transform tweak_camera_matrix(const ccl::Transform& tfm, const ccl::CameraType type, const ccl::PanoramaType panorama_type);
void xsi_matrix_to_cycles_array(std::vector<float>& array, XSI::MATH::CMatrix4 matrix, bool flip_z);
ccl::Transform xsi_matrix_to_transform(const XSI::MATH::CMatrix4& xsi_matrix, bool flip_z = false);
double get_random_value(double min, double max);

ccl::float3 color4_to_float3(const XSI::MATH::CColor4f &color);
ccl::float3 vector3_to_float3(const XSI::MATH::CVector3 &vector);

float get_minimum(float v1, float v2, float v3);
float get_maximum(float v1, float v2, float v3);

float interpolate_float_with_middle(float a, float b, float t, float middle);
XSI::MATH::CColor4f interpolate_color(const XSI::MATH::CColor4f& color1, const XSI::MATH::CColor4f& color2, float t, float mid = 0.5);