#pragma once
#include <xsi_application.h>
#include <xsi_time.h>

int get_frame(const XSI::CTime& eval_time);
XSI::CString seconds_to_date(double total_seconds);
float clamp_float(float value, float min, float max);
float linear_to_srgb_float(float v);
unsigned char linear_to_srgb(float v);
unsigned char linear_clamp(float v);
bool equal_floats(float a, float b);