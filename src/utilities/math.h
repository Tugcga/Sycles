#pragma once
#include <vector>

#include <xsi_application.h>
#include <xsi_time.h>

#include "util/transform.h"

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