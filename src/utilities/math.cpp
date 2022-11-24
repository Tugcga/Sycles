#include <xsi_application.h>
#include <xsi_time.h>

int get_frame(const XSI::CTime& eval_time)
{
	return (int)(eval_time.GetTime() + 0.5);
}