#pragma once
#include <xsi_application.h>
#include <xsi_time.h>

int get_frame(const XSI::CTime& eval_time);
XSI::CString seconds_to_date(double total_seconds);