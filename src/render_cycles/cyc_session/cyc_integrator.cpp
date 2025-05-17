#include <xsi_arrayparameter.h>

#include "session/session.h"
#include "scene/integrator.h"
#include "util/hash.h"
#include "scene/light.h"

#include "../../render_cycles/update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"
#include "../../input/input.h"
#include "cyc_baking.h"

ccl::DenoiseParams get_denoise_params(const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
{
	ccl::DenoiseParams denoising;

	int denoise_mode = render_parameters.GetValue("denoise_mode", eval_time);
	denoising.use = denoise_mode != 0;
	denoising.use = false;  // always desable built-in denoising
	denoising.type = denoise_mode == 2 ? ccl::DenoiserType::DENOISER_OPTIX : ccl::DenoiserType::DENOISER_OPENIMAGEDENOISE;
	denoising.use_gpu = false;  // it looks like this parameter important only for OIDN

	denoising.prefilter = ccl::DENOISER_PREFILTER_NONE;

	denoising.quality = ccl::DENOISER_QUALITY_BALANCED;

	denoising.use_pass_albedo = false;
	denoising.use_pass_normal = false;

	int denoise_channels = render_parameters.GetValue("denoise_channels", eval_time);
	if (denoise_channels == 1)
	{
		denoising.use_pass_albedo = true;
	}
	else if (denoise_channels == 2)
	{
		denoising.use_pass_albedo = true;
		denoising.use_pass_normal = true;
	}

	return denoising;
}

void sync_integrator(ccl::Session* session, UpdateContext* update_context, BakingContext* baking_context, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const InputConfig &input_config)
{
	ccl::Integrator* integrator = session->scene->integrator;
	XSI::CTime eval_time = update_context->get_time();

	integrator->set_motion_blur(update_context->get_motion_type() == MotionType_Blur);

	if (render_type == RenderType_Shaderball && input_config.is_init)
	{
		ConfigShaderball config_shaderball = input_config.shaderball;
		integrator->set_max_bounce(config_shaderball.max_bounces);
		integrator->set_max_diffuse_bounce(config_shaderball.diffuse_bounces);
		integrator->set_max_glossy_bounce(config_shaderball.glossy_bounces);
		integrator->set_max_transmission_bounce(config_shaderball.transmission_bounces);
		integrator->set_max_volume_bounce(config_shaderball.volume_bounces);
		integrator->set_transparent_max_bounce(config_shaderball.transparent_bounces);

		integrator->set_sample_clamp_direct(config_shaderball.clamp_direct);
		integrator->set_sample_clamp_indirect(config_shaderball.clamp_indirect);
	}
	else
	{
		integrator->set_max_bounce(render_parameters.GetValue("paths_max_bounces", eval_time));
		integrator->set_max_diffuse_bounce(render_parameters.GetValue("paths_max_diffuse_bounces", eval_time));
		integrator->set_max_glossy_bounce(render_parameters.GetValue("paths_max_glossy_bounces", eval_time));
		integrator->set_max_transmission_bounce(render_parameters.GetValue("paths_max_transmission_bounces", eval_time));
		integrator->set_max_volume_bounce(render_parameters.GetValue("paths_max_volume_bounces", eval_time));
		integrator->set_transparent_max_bounce(render_parameters.GetValue("paths_max_transparent_bounces", eval_time));

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
		integrator->set_sampling_pattern(patttern == 1 ? ccl::SamplingPattern::SAMPLING_PATTERN_TABULATED_SOBOL: ccl::SamplingPattern::SAMPLING_PATTERN_SOBOL_BURLEY);

		ccl::DenoiseParams denoise_params = get_denoise_params(render_parameters, eval_time);
		integrator->set_use_denoise(denoise_params.use);
		if (denoise_params.use)
		{
			integrator->set_denoiser_type(denoise_params.type);
			integrator->set_denoise_use_gpu(denoise_params.use_gpu);
			integrator->set_denoise_start_sample(denoise_params.start_sample);
			integrator->set_use_denoise_pass_albedo(denoise_params.use_pass_albedo);
			integrator->set_use_denoise_pass_normal(denoise_params.use_pass_normal);
			integrator->set_denoiser_prefilter(denoise_params.prefilter);
			integrator->set_denoiser_quality(denoise_params.quality);
		}

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
			float ao_factor = render_parameters.GetValue("paths_fastgi_ao_factor", eval_time);
			int ao_bounces = render_parameters.GetValue("paths_fastgi_ao_bouncess", eval_time);
			integrator->set_ao_bounces(ao_bounces);
			
			if (fast_method == 0)
			{// replace 
				integrator->set_ao_factor(ao_factor);
				integrator->set_ao_additive_factor(0.0f);
			}
			else
			{// add
				integrator->set_ao_factor(0.0f);
				integrator->set_ao_additive_factor(ao_factor);
			}
		}
		else
		{
			integrator->set_ao_factor(0.0f);
			integrator->set_ao_additive_factor(0.0f);
			integrator->set_ao_bounces(0);
		}
		float ao_distance = render_parameters.GetValue("paths_fastgi_ao_distance", eval_time);
		integrator->set_ao_distance(ao_distance);

		ccl::DeviceInfo render_device = session->params.device;
		if (render_device.type == ccl::DeviceType::DEVICE_CPU)
		{
			integrator->set_use_guiding(render_parameters.GetValue("sampling_path_guiding_use", eval_time));
			integrator->set_use_surface_guiding(render_parameters.GetValue("sampling_path_guiding_surface", eval_time));
			integrator->set_use_volume_guiding(render_parameters.GetValue("sampling_path_guiding_volume", eval_time));
			integrator->set_guiding_training_samples(render_parameters.GetValue("sampling_path_guiding_training_samples", eval_time));
		}
		else
		{
			bool sampling_path_guiding_use = render_parameters.GetValue("sampling_path_guiding_use", eval_time);
			if (sampling_path_guiding_use)
			{
				log_warning("Path guiding available only with CPU render device. Disable it.");
			}
			integrator->set_use_guiding(false);
		}
	}

	if (render_type == RenderType::RenderType_Rendermap && baking_context->get_is_valid())
	{
		if (baking_context->get_pass_type() == ccl::PASS_COMBINED)
		{
			integrator->set_use_direct_light(baking_context->get_key_is_direct());
			integrator->set_use_indirect_light(baking_context->get_key_is_indirect());
			integrator->set_use_diffuse(baking_context->get_key_is_diffuse());
			integrator->set_use_glossy(baking_context->get_key_is_glossy());
			integrator->set_use_transmission(baking_context->get_key_is_transmission());
			integrator->set_use_emission(baking_context->get_key_is_emit());
		}
	}

	integrator->tag_update(session->scene.get(), ccl::Integrator::UPDATE_ALL);
}