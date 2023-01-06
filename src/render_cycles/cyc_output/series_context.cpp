#include "series_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

SeriesContext::SeriesContext()
{
	save_intermediate = false;
	reset();
}

SeriesContext::~SeriesContext()
{
	reset();
}

void SeriesContext::setup(const ConfigSeries& config_series)
{
	save_intermediate = config_series.save_intermediate;
	save_albedo = config_series.save_albedo;
	save_normal = config_series.save_normal;
	save_beauty = config_series.save_beauty;

	sampling_size = config_series.sampling_size;

	albedo_prefix = config_series.albedo_prefix;
	normal_prefix = config_series.normal_prefix;
	beauty_prefix = config_series.beauty_prefix;
	sampling_start_separator = config_series.sampling_start_separator;
	sampling_middle_separator = config_series.sampling_middle_separator;
	sampling_postfix = config_series.sampling_postfix;
	sampling_step = config_series.sampling_step;
}

void SeriesContext::reset()
{
	last_sample = 0;
	is_active = false;
}

void SeriesContext::activate()
{
	if (save_intermediate)
	{
		is_active = true;
	}
	else
	{
		is_active = false;
	}
}

size_t SeriesContext::get_last_sample()
{
	return last_sample;
}

bool SeriesContext::get_is_active()
{
	return is_active;
}

int SeriesContext::get_sampling_step()
{
	return sampling_step;
}

void SeriesContext::set_last_sample(size_t value)
{
	last_sample = value;
}

XSI::CString samples_to_string(size_t samples, int size)
{
	XSI::CString raw_string = XSI::CString(samples);
	int delta_size = size - raw_string.Length();
	if (delta_size > 0)
	{
		XSI::CString to_return = "";
		for (int i = 0; i < delta_size; i++)
		{
			to_return += "0";
		}
		to_return += raw_string;
		return to_return;
	}
	else
	{
		return raw_string;
	}
}

XSI::CString SeriesContext::build_output_path(ccl::PassType pass_type, const XSI::CTime& eval_time)
{
	return file_common_path +
		XSI::CString(get_frame(eval_time)) +
		sampling_start_separator +
		samples_to_string(last_sample, sampling_size) +
		sampling_postfix +
		sampling_middle_separator +
		(pass_type == ccl::PASS_COMBINED ? beauty_prefix : (
			pass_type == ccl::PASS_DIFFUSE_COLOR ? albedo_prefix : (
				pass_type == ccl::PASS_NORMAL ? normal_prefix : "unknown"))) +
		".exr";
}

void SeriesContext::set_common_path(OutputContext* output_context)
{
	if (is_active && save_intermediate)
	{
		XSI::CString first_path = output_context->get_first_nonempty_path();
		XSI::CString common_name = output_context->get_common_path();

		if (first_path.Length() > 0 && common_name.Length() > 0)
		{
			ULONG slash_point = first_path.ReverseFindString("\\");
			if (slash_point != UINT_MAX)
			{
				file_common_path = first_path.GetSubString(0, slash_point) + "\\" + common_name;
			}
			else
			{
				is_active = false;
			}
		}
		else
		{
			is_active = false;
		}
	}
}

bool SeriesContext::need_albedo()
{
	return is_active && save_albedo;
}

bool SeriesContext::need_normal()
{
	return is_active && save_normal;
}

bool SeriesContext::need_beauty()
{
	return is_active && save_beauty;
}