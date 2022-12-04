#include <xsi_arrayparameter.h>

#include "session/session.h"
#include "scene/integrator.h"
#include "util/hash.h"

#include "../../render_cycles/update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

void sync_integrator(ccl::Session* session, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters)
{
	ccl::Integrator* integrator = session->scene->integrator;
	XSI::CTime eval_time = update_context->get_time();

	integrator->set_motion_blur(update_context->get_motion_type() == MotionType_Blur);

	integrator->set_max_bounce(render_parameters.GetValue("paths_max_bounces", eval_time));
	integrator->set_max_diffuse_bounce(render_parameters.GetValue("paths_max_diffuse_bounces", eval_time));
	integrator->set_max_glossy_bounce(render_parameters.GetValue("paths_max_glossy_bounces", eval_time));
	integrator->set_max_transmission_bounce(render_parameters.GetValue("paths_max_transmission_bounces", eval_time));
	integrator->set_max_volume_bounce(render_parameters.GetValue("paths_max_volume_bounces", eval_time));
	integrator->set_max_transmission_bounce(render_parameters.GetValue("paths_max_transparent_bounces", eval_time));

	integrator->set_min_bounce(render_parameters.GetValue("sampling_advanced_min_light_bounces", eval_time));
	integrator->set_transparent_min_bounce(render_parameters.GetValue("sampling_advanced_min_transparent_bounces", eval_time));

	integrator->set_volume_step_rate(render_parameters.GetValue("performance_volume_step_rate", eval_time));
	integrator->set_volume_max_steps(render_parameters.GetValue("performance_volume_max_steps", eval_time));

	integrator->set_filter_glossy(render_parameters.GetValue("paths_caustics_filter_glossy", eval_time));
	integrator->set_caustics_reflective(render_parameters.GetValue("paths_caustics_reflective", eval_time));
	integrator->set_caustics_refractive(render_parameters.GetValue("paths_caustics_refractive", eval_time));

	int seed = render_parameters.GetValue("sampling_advanced_seed", eval_time);
	bool is_anim_seed = render_parameters.GetValue("sampling_advanced_animate_seed", eval_time);
	if (is_anim_seed)
	{
		integrator->set_seed(ccl::hash_uint2(get_frame(eval_time), seed));
	}
	else
	{
		integrator->set_seed(seed);
	}

	integrator->set_sample_clamp_direct(render_parameters.GetValue("paths_clamp_direct", eval_time));
	integrator->set_sample_clamp_indirect(render_parameters.GetValue("paths_clamp_indirect", eval_time));

	integrator->set_light_sampling_threshold(render_parameters.GetValue("sampling_advanced_light_threshold", eval_time));

	bool use_adaptive = render_parameters.GetValue("sampling_render_use_adaptive", eval_time);
	integrator->set_use_adaptive_sampling(use_adaptive);
	integrator->set_adaptive_threshold(render_parameters.GetValue("sampling_render_adaptive_threshold", eval_time));
	integrator->set_adaptive_min_samples(render_parameters.GetValue("sampling_render_adaptive_min_samples", eval_time));

	int patttern = render_parameters.GetValue("sampling_advanced_pattern", eval_time);
	integrator->set_sampling_pattern(patttern == 0 ? ccl::SamplingPattern::SAMPLING_PATTERN_SOBOL_BURLEY : ccl::SamplingPattern::SAMPLING_PATTERN_PMJ);

	// TODO: make denoising support
	integrator->set_use_denoise(false);

	float scrambling_distance = render_parameters.GetValue("sampling_advanced_scrambling_multiplier", eval_time);
	bool auto_scrambling_distance = render_parameters.GetValue("sampling_advanced_scrambling_distance", eval_time);
	int samples = render_parameters.GetValue("sampling_render_samples", eval_time);
	if (auto_scrambling_distance)
	{
		if (use_adaptive)
		{
			const ccl::AdaptiveSampling adaptive_sampling = integrator->get_adaptive_sampling();
			samples = std::min(samples, adaptive_sampling.min_samples);
		}
		scrambling_distance *= 4.0f / sqrtf(samples);
	}
	integrator->set_scrambling_distance(scrambling_distance);

	bool use_fast_ao = render_parameters.GetValue("paths_fastgi_use", eval_time);
	int fast_method = render_parameters.GetValue("paths_fastgi_method", eval_time);
	if (use_fast_ao)
	{
		integrator->set_ao_bounces(render_parameters.GetValue("paths_fastgi_ao_bouncess", eval_time));
		if (fast_method == 0)
		{// replace 
			integrator->set_ao_factor(render_parameters.GetValue("paths_fastgi_ao_factor", eval_time));
			integrator->set_ao_additive_factor(0.0f);
		}
		else
		{// add
			integrator->set_ao_factor(0.0f);
			integrator->set_ao_additive_factor(render_parameters.GetValue("paths_fastgi_ao_factor", eval_time));
		}
	}
	else
	{
		integrator->set_ao_factor(0.0f);
		integrator->set_ao_additive_factor(0.0f);
		integrator->set_ao_bounces(0);
	}
	integrator->set_ao_distance(render_parameters.GetValue("paths_fastgi_ao_distance", eval_time));

	integrator->set_use_guiding(render_parameters.GetValue("sampling_path_guiding_use", eval_time));
	integrator->set_use_surface_guiding(render_parameters.GetValue("sampling_path_guiding_surface", eval_time));
	integrator->set_use_volume_guiding(render_parameters.GetValue("sampling_path_guiding_volume", eval_time));
	integrator->set_guiding_training_samples(render_parameters.GetValue("sampling_path_guiding_training_samples", eval_time));

	integrator->tag_update(session->scene, ccl::Integrator::UPDATE_ALL);
}