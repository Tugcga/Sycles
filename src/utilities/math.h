#pragma once
#include <vector>

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_math.h>
#include <xsi_color4f.h>

#include "util/transform.h"
#include "util/math_float3.h"

#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))

int get_frame(const XSI::CTime& eval_time);
XSI::CString seconds_to_date(double total_seconds);
float clamp_float(float value, float min, float max);
float linear_to_srgb_float(float v);
unsigned char linear_to_srgb(float v);
unsigned char linear_clamp(float v);
bool equal_floats(float a, float b);
ccl::Transform get_transform(std::vector<float>& array);
double get_random_value(double min, double max);
void xsi_matrix_to_cycles_array(std::vector<float>& array, XSI::MATH::CMatrix4 matrix, bool flip_z);

ccl::float3 color4_to_float3(const XSI::MATH::CColor4f &color);
ccl::float3 vector3_to_float3(const XSI::MATH::CVector3 &vector);