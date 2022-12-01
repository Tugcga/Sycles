#include <xsi_arrayparameter.h>
#include <xsi_time.h>

#include "session/session.h"
#include "scene/film.h"
#include "../../render_cycles/update_context.h"

void sync_film(ccl::Session* session, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();
	ccl::Film* film = session->scene->film;
	
	film->set_exposure(render_parameters.GetValue("film_exposure", eval_time));
	film->set_filter_width(render_parameters.GetValue("film_filter_width", eval_time));
	int xsi_filter_type = render_parameters.GetValue("film_filter_type", eval_time);
	if (xsi_filter_type == 0)
	{
		film->set_filter_type(ccl::FilterType::FILTER_BOX);
	}
	else if (xsi_filter_type == 1)
	{
		film->set_filter_type(ccl::FilterType::FILTER_GAUSSIAN);
	}
	else
	{
		film->set_filter_type(ccl::FilterType::FILTER_BLACKMAN_HARRIS);
	}

	// mist settings
	film->set_mist_start(render_parameters.GetValue("mist_start", eval_time));
	film->set_mist_depth(render_parameters.GetValue("mist_depth", eval_time));
	film->set_mist_falloff(render_parameters.GetValue("mist_falloff", eval_time));
}