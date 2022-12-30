#include "cyc_motion.h"
#include "../cyc_session/cyc_pass_utils.h"

MotionSettingsType get_motion_type_from_parameters(const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time, const XSI::CStringArray& output_channels, const XSI::CString& visual_channel)
{
	// if we have activated motion blur, then set parameter to blur and later disable output motion pass (if it exists)
	// and also change visual pass to combined

	bool is_motion = render_parameters.GetValue("film_motion_use", eval_time);
	if (is_motion)
	{
		return MotionType_Blur;
	}

	// motion blur is off, check is there is a pass or visual
	if (visual_channel == get_name_for_motion_display_channel())
	{
		return MotionType_Pass;
	}

	XSI::CString output_motion_name = get_name_for_motion_output_channel();
	for (size_t i = 0; i < output_channels.GetCount(); i++)
	{
		if (output_channels[i] == output_motion_name)
		{
			return MotionType_Pass;
		}
	}

	return MotionType_None;
}