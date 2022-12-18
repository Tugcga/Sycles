#include <string>
#include <vector>
#include <random>

#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_math.h>
#include <xsi_color4f.h>

#include "util/transform.h"
#include "util/projection.h"
#include "scene/camera.h"

#include "../render_base/type_enums.h"

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

unsigned char linear_to_srgb(float v)
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

unsigned char linear_clamp(float v)
{
	if (v <= 0.0f)
	{
		return 0;
	}
	if (v >= 1.0f)
	{
		return 255;
	}
	return (unsigned char)(v * 255.0);
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

ccl::float3 vector3_to_float3(const XSI::MATH::CVector3& vector)
{
	return ccl::make_float3(vector.GetX(), vector.GetY(), vector.GetZ());
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
