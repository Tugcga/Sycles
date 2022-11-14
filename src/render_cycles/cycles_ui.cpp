#include <xsi_parameter.h>
#include <xsi_customproperty.h>
#include <xsi_ppglayout.h>
#include <xsi_ppgitem.h>
#include <xsi_context.h>
#include <xsi_ppgeventcontext.h>

#include "scene/integrator.h"

#include "render_engine_cyc.h"
#include "../input/input.h"

const XSI::siCapabilities block_mode = XSI::siReadOnly;  // siReadOnly or siNotInspectable

void build_layout(XSI::PPGLayout& layout)
{
	layout.Clear();
}

XSI::CStatus RenderEngineCyc::render_options_update(XSI::PPGEventContext& event_context) 
{
	XSI::PPGEventContext::PPGEvent event_id = event_context.GetEventID();
	bool is_refresh = false;
	if (event_id == XSI::PPGEventContext::siOnInit) 
	{
		XSI::CustomProperty cp_source = event_context.GetSource();
	}
	else if (event_id == XSI::PPGEventContext::siParameterChange) 
	{
		XSI::Parameter changed = event_context.GetSource();
		XSI::CustomProperty prop = changed.GetParent();
		XSI::CString param_name = changed.GetScriptName();
	}

	if (is_refresh)
	{
		event_context.PutAttribute("Refresh", true);
	}

	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineCyc::render_option_define_layout(XSI::Context& context)
{
	XSI::PPGLayout layout = context.GetSource();
	XSI::Parameter parameter = context.GetSource();
	XSI::CustomProperty property = parameter.GetParent();

	build_layout(layout);

	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineCyc::render_option_define(XSI::CustomProperty& property)
{
	const int caps = XSI::siPersistable | XSI::siAnimatable;
	XSI::Parameter param;

	// sampling tab
	// render
	property.AddParameter("sampling_render_samples", XSI::CValue::siInt4, caps, "", "", 4, 1, ccl::Integrator::MAX_SAMPLES, 1, 4096 * 2, param);
	property.AddParameter("sampling_render_use_adaptive", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("sampling_render_adaptive_threshold", XSI::CValue::siFloat, caps, "", "", 0.001, 0.0, 1.0, 0.0, 0.1, param);
	property.AddParameter("sampling_render_adaptive_min_samples", XSI::CValue::siInt4, caps, "", "", 0, 0, 4096, 0, 512, param);
	property.AddParameter("sampling_render_time_limit", XSI::CValue::siInt4, caps, "", "", 0, 0, INT_MAX, 0, 120, param);

	// path guiding
	property.AddParameter("sampling_path_guiding_use", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("sampling_path_guiding_surface", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("sampling_path_guiding_volume", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("sampling_path_guiding_training_samples", XSI::CValue::siInt4, caps, "", "", 128, 1, ccl::Integrator::MAX_SAMPLES, 1, 256, param);

	// advanced
	property.AddParameter("sampling_advanced_seed", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("sampling_advanced_animate_seed", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("sampling_advanced_pattern", XSI::CValue::siInt4, caps, "", "", 1, 0, 1, 0, 1, param);
	property.AddParameter("sampling_advanced_offset", XSI::CValue::siInt4, caps, "", "", 0, 0, ccl::Integrator::MAX_SAMPLES, 0, 128, param);
	property.AddParameter("sampling_advanced_scrambling_distance", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("sampling_advanced_scrambling_multiplier", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 1.0, 0.0, 1.0, param);
	property.AddParameter("sampling_advanced_min_light_bounces", XSI::CValue::siInt4, caps, "", "", 0, 0, 1024, 1, 32, param);
	property.AddParameter("sampling_advanced_min_transparent_bounces", XSI::CValue::siInt4, caps, "", "", 0, 0, 1024, 1, 32, param);
	property.AddParameter("sampling_advanced_light_threshold", XSI::CValue::siFloat, caps, "", "", 0.01, 0.0, 1.0, 0.0, 0.05, param);
	
	// light paths tab
	// max bounces
	property.AddParameter("paths_max_bounces", XSI::CValue::siInt4, caps, "", "", 12, 0, INT_MAX, 1, 32, param);
	property.AddParameter("paths_max_diffuse_bounces", XSI::CValue::siInt4, caps, "", "", 4, 0, INT_MAX, 1, 32, param);
	property.AddParameter("paths_max_glossy_bounces", XSI::CValue::siInt4, caps, "", "", 4, 0, INT_MAX, 1, 32, param);
	property.AddParameter("paths_max_transmission_bounces", XSI::CValue::siInt4, caps, "", "", 12, 0, INT_MAX, 1, 32, param);
	property.AddParameter("paths_max_volume_bounces", XSI::CValue::siInt4, caps, "", "", 1, 0, INT_MAX, 1, 32, param);
	property.AddParameter("paths_max_transparent_bounces", XSI::CValue::siInt4, caps, "", "", 8, 0, INT_MAX, 1, 32, param);

	// clamping
	property.AddParameter("paths_clamp_direct", XSI::CValue::siFloat, caps, "", "", 0.0, 0.0, FLT_MAX, 0.0, 64.0, param);
	property.AddParameter("paths_clamp_indirect", XSI::CValue::siFloat, caps, "", "", 10.0, 0.0, FLT_MAX, 0.0, 64.0, param);

	// caustics
	property.AddParameter("paths_caustics_filter_glossy", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 10.0, 0.0, 10.0, param);
	property.AddParameter("paths_caustics_reflective", XSI::CValue::siBool, caps, "", "", 1, param);
	property.AddParameter("paths_caustics_refractive", XSI::CValue::siBool, caps, "", "", 1, param);
	
	// fast gi
	property.AddParameter("paths_fastgi_use", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("paths_fastgi_ao_factor", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, FLT_MAX, 0.0, 1.0, param);
	property.AddParameter("paths_fastgi_ao_distance", XSI::CValue::siFloat, caps, "", "", 10.0, 0.0, INT_MAX, 0.0, 16.0, param);
	property.AddParameter("paths_fastgi_method", XSI::CValue::siInt4, caps, "", "", 0, 0, 1, 0, 1, param);
	property.AddParameter("paths_fastgi_ao_bouncess", XSI::CValue::siInt4, caps, "", "", 1, 0, 1024, 0, 16, param);
	
	// volumes tab
	property.AddParameter("volume_step_rate", XSI::CValue::siFloat, caps, "", "", 1.0, 0.1, 10.0, 0.1, 10.0, param);
	property.AddParameter("volume_max_steps", XSI::CValue::siInt4, caps, "", "", 1024, 0, INT_MAX, 0, 2048, param);
	
	// curves tab
	property.AddParameter("curves_type", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("curves_subdivs", XSI::CValue::siInt4, caps, "", "", 2, 0, 24, 0, 6, param);

	// simplify tab
	// culling
	property.AddParameter("simplify_cull_mode", XSI::CValue::siInt4, caps, "", "", 0, 0, 1, 0, 1, param);
	property.AddParameter("simplify_cull_camera", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("simplify_cull_distance", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("simplify_cull_camera_margin", XSI::CValue::siFloat, caps, "", "", 0.1, 0.0, 5.0, 0.0, 5.0, param);
	property.AddParameter("simplify_cull_distance_margin", XSI::CValue::siFloat, caps, "", "", 50.0, 0.0, FLT_MAX, 0.0, 100.0, param);

	// motion tab
	property.AddParameter("motion_use", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("motion_position", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("motion_shutter", XSI::CValue::siFloat, caps, "", "", 0.5, 0.01, 1.0, 0.01, 1.0, param);
	property.AddParameter("motion_rolling_type", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("motion_rolling_duration", XSI::CValue::siFloat, caps, "", "", 0.1, 0.0, 1.0, 0.0, 1.0, param);

	// film tab
	property.AddParameter("film_exposure", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 10.0, 0.0, 2.0, param);
	// pixel filter
	property.AddParameter("film_filter_type", XSI::CValue::siInt4, caps, "", "", 1, 0, 2, 0, 2, param);
	property.AddParameter("film_filter_width", XSI::CValue::siFloat, caps, "", "", 1.5, param);
	// transparent
	property.AddParameter("film_transparent", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("film_transparent_glass", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("film_transparent_roughness", XSI::CValue::siDouble, caps, "", "", 0.1, 0.0, 1.0, 0.0, 1.0, param);

	// performance tab
	// threads
	property.AddParameter("performance_threads_mode", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("performance_threads_count", XSI::CValue::siInt4, caps, "", "", 0, param);

	// memory
	property.AddParameter("performance_memory_use_auto_tile", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_memory_tile_size", XSI::CValue::siInt4, caps, "", "", 2048, 8, INT_MAX, 8, 4096, param);

	// acceleration structure
	property.AddParameter("performance_acceleration_use_spatial_split", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_acceleration_use_compact_bvh", XSI::CValue::siBool, caps, "", "", false, param);

	// color management tab
	property.AddParameter("cm_apply_to_ldr", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("cm_mode", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("cm_device_index", XSI::CValue::siInt4, caps, "", "", 0, param);  // <-- default device index
	property.AddParameter("cm_view_index", XSI::CValue::siInt4, caps, "", "", 1, 0, 4, 0, 4, param);
	property.AddParameter("cm_look_index", XSI::CValue::siInt4, caps, "", "", 0, 0, 14, 0, 14, param);
	property.AddParameter("cm_exposure", XSI::CValue::siFloat, caps, "", "", 0.0, -1024.0, 1024.0, -10.0, 10.0, param);
	property.AddParameter("cm_gamma", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 1024.0, 0.0, 5.0, param);

	// labels tab
	property.AddParameter("label_render_time", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_time", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_frame", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_scene", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_camera", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_samples", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_objects_count", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_lights_count", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_triangles_count", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("label_curves_count", XSI::CValue::siBool, caps, "", "", 0, param);

	// output tab
	// passes
	property.AddParameter("output_pass_alpha_threshold", XSI::CValue::siFloat, caps, "", "", 0.5, 0.0, 1.0, 0.0, 1.0, param);
	property.AddParameter("output_pass_fill_alpha", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("output_pass_assign_unique_pass_id", XSI::CValue::siBool, caps, "", "", false, param);

	// multilayer exr
	property.AddParameter("output_exr_combine_passes", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_exr_render_separate_passes", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("output_exr_noisy_passes", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_exr_denoising_data", XSI::CValue::siBool, caps, "", "", false, param);

	// cryptomatte
	property.AddParameter("output_crypto_object", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_material", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_asset", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_levels", XSI::CValue::siInt4, caps, "", "", 6, 2, 16, 2, 16, param);

	// world tab
	// mist
	property.AddParameter("mist_start", XSI::CValue::siFloat, caps, "", "", 5.0, 0.0, FLT_MAX, 0.0, 16.0, param);
	property.AddParameter("mist_depth", XSI::CValue::siFloat, caps, "", "", 25.0, 0.0, FLT_MAX, 0.0, 64.0, param);
	property.AddParameter("mist_falloff", XSI::CValue::siFloat, caps, "", "", 2.0, 0.0, FLT_MAX, 0.0, 2.0, param);

	// background
	// color
	property.AddParameter("background_colorR", XSI::CValue::siDouble, caps, "Color", "", 0.2, param);
	property.AddParameter("background_colorG", XSI::CValue::siDouble, caps, "", "", 0.2, param);
	property.AddParameter("background_colorB", XSI::CValue::siDouble, caps, "", "", 0.2, param);
	property.AddParameter("background_strength", XSI::CValue::siFloat, caps, "", "", 0.0, 0.0, INT_MAX, 0.0, 2.0, param);
	// visibility
	property.AddParameter("background_ray_visibility_camera", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_diffuse", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_glossy", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_transmission", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_scatter", XSI::CValue::siBool, caps, "", "", true, param);
	// volume
	property.AddParameter("background_volume_sampling", XSI::CValue::siInt4, caps, "", "", 1, param);
	property.AddParameter("background_volume_interpolation", XSI::CValue::siInt4, caps, "", "", 1, param);
	property.AddParameter("background_volume_homogeneous", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("background_volume_step_rate", XSI::CValue::siFloat, caps, "", "", 1.0, 0.01, 100.0, 0.1, 10.0, param);
	// surface
	property.AddParameter("background_surface_sampling_method", XSI::CValue::siUInt1, caps, "", "", 0, param);
	property.AddParameter("background_surface_max_bounces", XSI::CValue::siUInt2, caps, "", "", 1024, 0, 1024, 0, 1024, param);
	property.AddParameter("background_surface_resolution", XSI::CValue::siInt4, caps, "", "", 1024, 4, 8191, 4, 2048, param);
	property.AddParameter("background_surface_shadow_caustics", XSI::CValue::siBool, caps, "", "", false, param);

	// denoising tab
	property.AddParameter("denoise_mode", XSI::CValue::siInt4, caps, "", "", 0, 0, 2, 0, 2, param);
	property.AddParameter("denoise_channels", XSI::CValue::siInt4, caps, "", "", 2, 0, 2, 0, 2, param);  // for Optix and OIDenoise modes
	property.AddParameter("denoise_prefilter", XSI::CValue::siInt4, caps, "", "", 2, 0, 2, 0, 2, param);  // for OIDenoise

	// options tab
	// shaders
	property.AddParameter("options_shaders_use_mis", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("options_shaders_transparent_shadows", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("options_shaders_system", XSI::CValue::siInt4, caps, "", "", 0, param);

	// logging
	property.AddParameter("options_logging_log_rendertime", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("options_logging_log_details", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("options_logging_log_profiling", XSI::CValue::siBool, caps, "", "", false, param);

	// displacement
	property.AddParameter("options_displacement_method", XSI::CValue::siInt4, caps, "", "", 2, param);

	// devices
	ULONG device_count = 16;
	InputConfig input_config = get_input_config();
	if (input_config.is_init)
	{
		device_count = input_config.render.devices;
	}

	for (ULONG i = 0; i < device_count; i++)
	{
		property.AddParameter("device_" + XSI::CString(i), XSI::CValue::siBool, caps, "", "", i == 0, param);
	}

	return XSI::CStatus::OK;
}