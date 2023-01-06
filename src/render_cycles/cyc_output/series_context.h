#pragma once
#include "scene/pass.h"

#include <xsi_application.h>
#include <xsi_time.h>

#include "../../input/config_ini.h"
#include "output_context.h"

class SeriesContext
{
public:
	SeriesContext();
	~SeriesContext();

	void setup(const ConfigSeries& config_series);
	void reset();
	void activate();

	bool get_is_active();
	size_t get_last_sample();
	void set_last_sample(size_t value);
	void set_common_path(OutputContext* output_context);
	bool need_beauty();
	bool need_albedo();
	bool need_normal();
	int get_sampling_step();

	XSI::CString build_output_path(ccl::PassType pass_type, const XSI::CTime &eval_time);

private:
	bool is_active;  // make true if we run pass rendering with active save intermidiate

	bool save_intermediate;
	bool save_albedo;
	bool save_normal;
	bool save_beauty;
	int sampling_size;
	XSI::CString albedo_prefix;
	XSI::CString normal_prefix;
	XSI::CString beauty_prefix;
	XSI::CString sampling_start_separator;
	XSI::CString sampling_middle_separator;
	XSI::CString sampling_postfix;
	int sampling_step;

	XSI::CString file_common_path;  // full path but without end of the file name
	size_t last_sample = 0;
};