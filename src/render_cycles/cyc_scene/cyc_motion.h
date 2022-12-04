#pragma once
#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>

// the same as in Cycles, but we will control it also here
enum MotionType
{
	MotionType_None,
	MotionType_Blur,
	MotionType_Pass,
	MotionType_Unknown
};

enum MotionPosition
{
	MotionPosition_Start,
	MotionPosition_Center,
	MotionPosition_End
};

MotionType get_motion_type_from_parameters(const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel);