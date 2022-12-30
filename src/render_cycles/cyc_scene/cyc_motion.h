#pragma once
#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>

// the same as in Cycles, but we will control it also here
enum MotionSettingsType
{
	MotionType_None,
	MotionType_Blur,
	MotionType_Pass,
	MotionType_Unknown
};

enum MotionSettingsPosition
{
	MotionPosition_Start,
	MotionPosition_Center,
	MotionPosition_End
};

MotionSettingsType get_motion_type_from_parameters(const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel);