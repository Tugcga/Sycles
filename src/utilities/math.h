#pragma once
#include <vector>
#include <map>

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_math.h>
#include <xsi_color4f.h>
#include <xsi_vector3f.h>
#include <xsi_vector2f.h>

#include "util/transform.h"
#include "util/math_float3.h"
#include "scene/camera.h"
#include "util/color.h"
#include "util/array.h"

#include "../render_base/type_enums.h"

#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))

int get_frame(const XSI::CTime& eval_time);
XSI::CString seconds_to_date(double total_seconds);
float clamp_float(float value, float min, float max);
float linear_to_srgb_float(float v);
float srgb_to_linear(float value);
uint8_t linear_to_srgb_int8(float v);
uint16_t linear_to_srgb_int16(float v);
uint8_t linear_clamp_int8(float v);
uint16_t linear_clamp_int16(float v);
bool equal_floats(float a, float b);
ccl::Transform get_transform(std::vector<float>& array);
ccl::Transform tweak_camera_matrix(const ccl::Transform& tfm, const ccl::CameraType type, const ccl::PanoramaType panorama_type);
void xsi_matrix_to_cycles_array(std::vector<float>& array, XSI::MATH::CMatrix4 matrix, bool flip_z);
ccl::Transform xsi_matrix_to_transform(const XSI::MATH::CMatrix4& xsi_matrix, bool flip_z = false);
double get_random_value(double min, double max);

ccl::float3 color4_to_float3(const XSI::MATH::CColor4f &color);
ccl::float4 color4_to_float4(const XSI::MATH::CColor4f& color);
ccl::float3 vector3_to_float3(const XSI::MATH::CVector3 &vector);
ccl::float3 vector3_to_float3(const XSI::MATH::CVector3f& vector);
ccl::float2 vector2_to_float2(const XSI::MATH::CVector2f& vector);

float get_minimum(float v1, float v2, float v3);
float get_maximum(float v1, float v2, float v3);

float interpolate_float_with_middle(float a, float b, float t, float middle);
XSI::MATH::CColor4f interpolate_color(const XSI::MATH::CColor4f& color1, const XSI::MATH::CColor4f& color2, float t, float mid = 0.5);

ccl::array<int> exctract_tiles(const std::map<int, XSI::CString>& tile_to_path_map);
std::vector<float> flip_pixels(float* input, ULONG width, ULONG height, ULONG channels);
int powi(int base, unsigned int exp);