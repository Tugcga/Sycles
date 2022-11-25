#include <string>

#include <xsi_application.h>
#include <xsi_time.h>

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