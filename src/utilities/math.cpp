#include <string>
#include <vector>
#include <random>
#include <map>

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_math.h>
#include <xsi_color4f.h>
#include <xsi_vector3f.h>
#include <xsi_vector2f.h>
#include <xsi_rotationf.h>

#include "util/transform.h"
#include "util/projection.h"
#include "util/color.h"
#include "scene/camera.h"

#include "../render_base/type_enums.h"
#include "../render_cycles/cyc_scene/cyc_motion.h"

const float epsilon = 0.0001f;

int get_frame(const XSI::CTime& eval_time)
{
	return (int)(eval_time.GetTime() + 0.5);
}

XSI::CString seconds_to_date(double total_seconds)
{
	int int_hours = (int)floor(total_seconds / 3600.0);
	int int_minutes = (int)floor((total_seconds - int_hours * 3600.0) / 60.0);
	float remain_seconds = total_seconds - int_hours * 3600.0 - int_minutes * 60.0;

	XSI::CString to_return;
	if (int_hours > 0)
	{
		to_return = to_return + XSI::CString(int_hours) + " h. ";
	}
	if (int_minutes > 0)
	{
		to_return = to_return + XSI::CString(int_minutes) + " m. ";
	}

	std::string remain_seconds_str = std::to_string(remain_seconds);
	if (remain_seconds_str.size() > 4)
	{
		remain_seconds_str = remain_seconds_str.substr(0, 4);
	}
	to_return = to_return + XSI::CString(remain_seconds_str.c_str()) + " s.";

	return to_return;
}

float clamp_float(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

float linear_to_srgb_float(float v)
{
	if (v <= 0.0f)
	{
		return 0.0;
	}
	if (v >= 1.0f)
	{
		return v;
	}
	if (v <= 0.0031308f)
	{
		return  12.92f * v;
	}

	return (1.055f * pow(v, 1.0f / 2.4f)) - 0.055f;
}

uint8_t linear_to_srgb_int8(float v)
{
	if (v <= 0.0f)
	{
		return 0;
	}
	if (v >= 1.0f)
	{
		return 255;
	}
	if (v <= 0.0031308f)
	{
		return  (unsigned char)((12.92f * v * 255.0f) + 0.5f);
	}
	return (unsigned char)(((1.055f * pow(v, 1.0f / 2.4f)) - 0.055f) * 255.0f + 0.5f);
}

uint16_t linear_to_srgb_int16(float v)
{
	if (v <= 0.0f)
	{
		return 0;
	}
	if (v >= 1.0f)
	{
		return 65535;
	}
	if (v <= 0.0031308f)
	{
		return  (unsigned char)((12.92f * v * 65535.0f) + 0.5f);
	}
	return (unsigned char)(((1.055f * pow(v, 1.0f / 2.4f)) - 0.055f) * 65535.0f + 0.5f);
}

float srgb_to_linear(float value)
{
	return ccl::color_srgb_to_linear(value);
}

uint8_t linear_clamp_int8(float v)
{
	if (v <= 0.0f)
	{
		return 0;
	}
	if (v >= 1.0f)
	{
		return 255;
	}
	return (uint8_t)(v * 255.0);
}

uint16_t linear_clamp_int16(float v)
{
	if (v <= 0.0f)
	{
		return 0;
	}
	if (v >= 1.0f)
	{
		return 65535;
	}
	return (uint8_t)(v * 65535.0);
}

bool equal_floats(float a, float b)
{
	float abs_value = std::abs(a - b);
	return abs_value < epsilon;
}

ccl::Transform get_transform(std::vector<float>& array)
{
	ccl::ProjectionTransform projection;
	projection.x = ccl::make_float4(array[0], array[1], array[2], array[3]);
	projection.y = ccl::make_float4(array[4], array[5], array[6], array[7]);
	projection.z = ccl::make_float4(array[8], array[9], array[10], array[11]);
	projection.w = ccl::make_float4(array[12], array[13], array[14], array[15]);
	projection = projection_transpose(projection);
	return projection_to_transform(projection);
}

ccl::Transform tweak_camera_matrix(const ccl::Transform& tfm, const ccl::CameraType type, const ccl::PanoramaType panorama_type)
{
	ccl::Transform result;

	if (type == ccl::CAMERA_PANORAMA)
	{
		if (panorama_type == ccl::PANORAMA_MIRRORBALL)
		{
			result = tfm * ccl::make_transform(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, -1.0f, 0.0f, 0.0f);
		}
		else
		{
			result = tfm * ccl::make_transform(
				0.0f, -1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				1.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	else
	{
		result = tfm * ccl::transform_scale(1.0f, 1.0f, 1.0f);
	}

	return transform_clear_scale(result);
}

void xsi_matrix_to_cycles_array(std::vector<float>& array, XSI::MATH::CMatrix4 matrix, bool flip_z)
{
	array.resize(16);
	int s = 1;
	if (flip_z)
	{
		s = -1;
	}
	array[0] = matrix.GetValue(0, 0);
	array[1] = matrix.GetValue(0, 1);
	array[2] = matrix.GetValue(0, 2);
	array[3] = matrix.GetValue(0, 3);

	array[4] = matrix.GetValue(1, 0);
	array[5] = matrix.GetValue(1, 1);
	array[6] = matrix.GetValue(1, 2);
	array[7] = matrix.GetValue(1, 3);

	array[8] = s * matrix.GetValue(2, 0);
	array[9] = s * matrix.GetValue(2, 1);
	array[10] = s * matrix.GetValue(2, 2);
	array[11] = s * matrix.GetValue(2, 3);

	array[12] = matrix.GetValue(3, 0);
	array[13] = matrix.GetValue(3, 1);
	array[14] = matrix.GetValue(3, 2);
	array[15] = matrix.GetValue(3, 3);
}

ccl::Transform xsi_matrix_to_transform(const XSI::MATH::CMatrix4 &xsi_matrix, bool flip_z)
{
	std::vector<float> tfm_array;
	xsi_matrix_to_cycles_array(tfm_array, xsi_matrix, flip_z);
	return get_transform(tfm_array);
}

std::random_device device;
std::mt19937 rng(device());

double get_random_value(double min, double max)
{
	std::uniform_real_distribution<> dist(min, max);

	return dist(rng);
}

ccl::float3 color4_to_float3(const XSI::MATH::CColor4f& color)
{
	return ccl::make_float3(color.GetR(), color.GetG(), color.GetB());
}

ccl::float4 color4_to_float4(const XSI::MATH::CColor4f& color)
{
	return ccl::make_float4(color.GetR(), color.GetG(), color.GetB(), color.GetA());
}

ccl::float3 vector3_to_float3(const XSI::MATH::CVector3& vector)
{
	return ccl::make_float3(vector.GetX(), vector.GetY(), vector.GetZ());
}

ccl::float3 vector3_to_float3(const XSI::MATH::CVector3f& vector)
{
	return ccl::make_float3(vector.GetX(), vector.GetY(), vector.GetZ());
}

ccl::float2 vector2_to_float2(const XSI::MATH::CVector2f& vector)
{
	return ccl::make_float2(vector.GetX(), vector.GetY());
}

ccl::float4 quaternion_to_float4(const XSI::MATH::CQuaternion& quaternion)
{
	return ccl::make_float4(quaternion.GetX(), quaternion.GetY(), quaternion.GetZ(), quaternion.GetW());
}

ccl::float3 rotation_to_float3(const XSI::MATH::CRotationf& rotation)
{
	float x, y, z;
	rotation.GetXYZAngles(x, y, z);
	return ccl::make_float3(x, y, z);
}

float get_minimum(float v1, float v2, float v3)
{
	float to_return = v1;
	if (v2 < to_return)
	{
		to_return = v2;
	}
	if (v3 < to_return)
	{
		to_return = v3;
	}
	return to_return;
}

float get_maximum(float v1, float v2, float v3)
{
	float to_return = v1;
	if (v2 > to_return)
	{
		to_return = v2;
	}
	if (v3 > to_return)
	{
		to_return = v3;
	}
	return to_return;
}

float interpolate_float(float a, float b, float t)
{
	return (1 - t) * a + t * b;
}

float transform_time_by_mid(float t, float mid)
{
	if (mid < 0.001)
	{
		mid = 0.001;
	}
	if (mid > 0.999)
	{
		mid = 0.999;
	}
	if (t < mid)
	{
		return 0.5 * t / mid;
	}
	else
	{
		return 0.5 * t / (1 - mid) + (0.5 - mid) / (1 - mid);
	}
}

float interpolate_float_with_middle(float a, float b, float t, float middle)
{
	return interpolate_float(a, b, transform_time_by_mid(t, middle));
}

XSI::MATH::CColor4f interpolate_color(const XSI::MATH::CColor4f& color1, const XSI::MATH::CColor4f& color2, float t, float mid)
{
	return XSI::MATH::CColor4f(interpolate_float_with_middle(color1.GetR(), color2.GetR(), t, mid),
		interpolate_float_with_middle(color1.GetG(), color2.GetG(), t, mid),
		interpolate_float_with_middle(color1.GetB(), color2.GetB(), t, mid),
		interpolate_float_with_middle(color1.GetA(), color2.GetA(), t, mid));
}

ccl::array<int> exctract_tiles(const std::map<int, XSI::CString>& tile_to_path_map)
{
	ccl::array<int> to_return;
	for (auto const& [key, val] : tile_to_path_map)
	{
		to_return.push_back_slow(key);
	}
	return to_return;
}

std::vector<float> flip_pixels(float* input, ULONG width, ULONG height, ULONG channels)
{
	size_t pixels_count = width * height;
	std::vector<float> out_pixels(pixels_count * channels);
	for (ULONG pixel = 0; pixel < pixels_count; pixel++)
	{
		ULONG row = pixel / width;
		ULONG column = pixel - row * width;
		ULONG flip_pixel = (height - row - 1) * width + column;
		for (ULONG c = 0; c < channels; c++)
		{
			float v = input[channels * pixel + c];
			out_pixels[channels * flip_pixel + c] = v;
		}
	}

	return out_pixels;
}

int powi(int base, unsigned int exp)
{
	int res = 1;
	while (exp) 
	{
		if (exp & 1)
		{
			res *= base;
		}
		exp >>= 1;
		base *= base;
	}
	return res;
}

size_t calc_time_motion_step(size_t mi, size_t motion_steps, MotionSettingsPosition motion_position) {
	size_t time_motion_step = mi;
	if (motion_position == MotionSettingsPosition::MotionPosition_Start)
	{
		time_motion_step++;
	}
	else if (motion_position == MotionSettingsPosition::MotionPosition_Center)
	{// center
		if (mi >= motion_steps / 2)
		{
			time_motion_step++;
		}
	}

	return time_motion_step;
}