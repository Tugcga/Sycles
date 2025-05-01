#include <xsi_parameter.h>
#include <xsi_customproperty.h>
#include <xsi_ppglayout.h>
#include <xsi_ppgitem.h>
#include <xsi_context.h>
#include <xsi_ppgeventcontext.h>

#include "scene/integrator.h"

#include "render_engine_cyc.h"
#include "../input/input.h"
#include "../utilities/logs.h"

const XSI::siCapabilities block_mode = XSI::siReadOnly;  // siReadOnly or siNotInspectable

void build_layout(XSI::PPGLayout& layout, const XSI::CParameterRefArray& parameters)
{
	layout.Clear();

	layout.AddTab("Sampling");
	layout.AddGroup("Render");
	layout.AddItem("sampling_render_use_adaptive", "Adaptive Sampling");
	layout.AddItem("sampling_render_adaptive_threshold", "Noise Threshold");
	layout.AddItem("sampling_render_samples", "Samples");
	layout.AddItem("sampling_render_adaptive_min_samples", "Min Samples");
	layout.AddItem("sampling_render_time_limit", "Time Limit");
	layout.EndGroup();

	layout.AddGroup("Path Guiding");
	layout.AddItem("sampling_path_guiding_use", "Active Path Guiding");
	layout.AddItem("sampling_path_guiding_surface", "Surface Guiding");
	layout.AddItem("sampling_path_guiding_volume", "Volume Guiding");
	layout.AddItem("sampling_path_guiding_training_samples", "Training Samples");
	layout.EndGroup();

	layout.AddGroup("Lights");
	layout.AddItem("sampling_advanced_light_tree", "Light Tree");
	layout.AddItem("sampling_advanced_light_threshold", "Light Threshold");
	layout.EndGroup();

	layout.AddGroup("Advanced");
	layout.AddItem("sampling_advanced_seed", "Seed");
	layout.AddItem("sampling_advanced_animate_seed", "Animate Seed");
	layout.AddItem("sampling_advanced_offset", "Sample Offset");
	layout.AddItem("sampling_advanced_scrambling_distance", "Auto Scrambling Distance");
	layout.AddItem("sampling_advanced_scrambling_multiplier", "Scrambling Distance Multiplier");
	layout.AddItem("sampling_advanced_min_light_bounces", "Min Light Bounces");
	layout.AddItem("sampling_advanced_min_transparent_bounces", "Min Transparent Bounces");
	layout.EndGroup();

	layout.AddTab("Light Paths");
	layout.AddGroup("Max Bounces");
	layout.AddItem("paths_max_bounces", "Total");
	layout.AddItem("paths_max_diffuse_bounces", "Diffuse");
	layout.AddItem("paths_max_glossy_bounces", "Glossy");
	layout.AddItem("paths_max_transmission_bounces", "Transmission");
	layout.AddItem("paths_max_volume_bounces", "Volume");
	layout.AddItem("paths_max_transparent_bounces", "Transparent");
	layout.EndGroup();

	layout.AddGroup("Clamping");
	layout.AddItem("paths_clamp_direct", "Direct Light");
	layout.AddItem("paths_clamp_indirect", "Indirect Light");
	layout.EndGroup();

	layout.AddGroup("Caustics");
	layout.AddItem("paths_caustics_filter_glossy", "Filter Glossy");
	layout.AddItem("paths_caustics_reflective", "Reflective");
	layout.AddItem("paths_caustics_refractive", "Refractive");
	layout.EndGroup();

	layout.AddGroup("Fast GI Approximation");
	layout.AddItem("paths_fastgi_use", "Use AO");
	XSI::CValueArray fastgi_method_combo(4);
	fastgi_method_combo[0] = "Replace"; fastgi_method_combo[1] = LONG(0);
	fastgi_method_combo[2] = "Add"; fastgi_method_combo[3] = LONG(1);
	layout.AddEnumControl("paths_fastgi_method", fastgi_method_combo, "Method", XSI::siControlCombo);
	layout.AddItem("paths_fastgi_ao_factor", "AO Factor");
	layout.AddItem("paths_fastgi_ao_distance", "AO Distance");
	layout.AddItem("paths_fastgi_ao_bounces", "Bounces");
	layout.EndGroup();

	layout.AddTab("Film");
	layout.AddGroup("Film");
	layout.AddItem("film_exposure", "Exposure");
	layout.EndGroup();

	layout.AddGroup("Pixel Filter");
	XSI::CValueArray film_type_combo(6);
	film_type_combo[0] = "Box"; film_type_combo[1] = LONG(0);
	film_type_combo[2] = "Gaussian"; film_type_combo[3] = LONG(1);
	film_type_combo[4] = "Blackman-Harris"; film_type_combo[5] = LONG(2);
	layout.AddEnumControl("film_filter_type", film_type_combo, "Filter Type", XSI::siControlCombo);
	layout.AddItem("film_filter_width", "Width");
	layout.EndGroup();

	layout.AddGroup("Transparent");
	layout.AddItem("film_transparent", "Transparent");
	layout.AddItem("film_transparent_glass", "Transparent Glass");
	layout.AddItem("film_transparent_roughness", "Roughness Threshold");
	layout.EndGroup();

	layout.AddGroup("Motion");
	layout.AddItem("film_motion_use", "Motion Blur");
	layout.AddItem("film_motion_steps", "Motion Steps");
	XSI::CValueArray motion_position_combo(6);
	motion_position_combo[0] = "Start on Frame"; motion_position_combo[1] = 0;
	motion_position_combo[2] = "Center on Frame"; motion_position_combo[3] = 1;
	motion_position_combo[4] = "End on Frame"; motion_position_combo[5] = 2;
	layout.AddEnumControl("film_motion_position", motion_position_combo, "Position", XSI::siControlCombo);
	layout.AddItem("film_motion_shutter", "Shutter");
	XSI::CValueArray motion_rolling_combo(4);
	motion_rolling_combo[0] = "None"; motion_rolling_combo[1] = 0;
	motion_rolling_combo[2] = "Top-Bottom"; motion_rolling_combo[3] = 1;
	layout.AddEnumControl("film_motion_rolling_type", motion_rolling_combo, "Rolling Shutter", XSI::siControlCombo);
	layout.AddItem("film_motion_rolling_duration", "Duration");
	layout.EndGroup();

	layout.AddTab("Performance");
	layout.AddGroup("Threads");
	XSI::CValueArray threads_mode_combo(4);
	threads_mode_combo[0] = "Auto-detect"; threads_mode_combo[1] = 0;
	threads_mode_combo[2] = "Fixed"; threads_mode_combo[3] = 1;
	layout.AddEnumControl("performance_threads_mode", threads_mode_combo, "Threads Mode", XSI::siControlCombo);
	layout.AddItem("performance_threads_count", "Threads");
	layout.EndGroup();

	// TODO: with activated tile rendering there are some problems to obtain pixels from the tile
	// it adds some padding to the tile segment, and pixels of the tile have empty spaces
	// so, pass pixels array to the tile.get_pass_pixels does not work, because it requires more pixels than size of the tile
	// NOTE: in test console app all works fine, the tile does not contains any padding in pixels
	// why it happens here?
	// so, for now hode option for tile rendering from the UI
	// by default it's false
	// layout.AddGroup("Memory");
	// layout.AddItem("performance_memory_use_auto_tile", "Use Tiling");
	// layout.AddItem("performance_memory_tile_size", "Tile Size");
	// layout.EndGroup();

	layout.AddGroup("Acceleration Structure");
	layout.AddItem("performance_acceleration_use_spatial_split", "Use Spatial Splits");
	layout.AddItem("performance_acceleration_use_compact_bvh", "Use Compact BVH");
	layout.EndGroup();

	layout.AddGroup("Volumes");
	layout.AddItem("performance_volume_step_rate", "Step Rate");
	layout.AddItem("performance_volume_max_steps", "Max Steps");
	layout.EndGroup();

	layout.AddGroup("Curves");
	XSI::CValueArray curves_type_combo(4);
	curves_type_combo[0] = "Rounded Ribbons"; curves_type_combo[1] = LONG(0);
	curves_type_combo[2] = "3D Curves"; curves_type_combo[3] = LONG(1);
	layout.AddEnumControl("performance_curves_type", curves_type_combo, "Shape", XSI::siControlCombo);
	layout.AddItem("performance_curves_subdivs", "Curve Subdivisions");
	layout.EndGroup();

	layout.AddTab("Color Managements");
	layout.AddGroup("Color Management");
	layout.AddItem("cm_apply_to_ldr", "Apply Color Profile to LDR Combined Pass");
	OCIOConfig ocio_config = get_ocio_config();
	if (ocio_config.is_file_exist && ocio_config.is_init)
	{
		XSI::CValueArray cm_mode_combo(4);
		cm_mode_combo[0] = "Simple sRGB"; cm_mode_combo[1] = 0;
		cm_mode_combo[2] = "OpenColorIO Config"; cm_mode_combo[3] = 1;
		layout.AddEnumControl("cm_mode", cm_mode_combo, "Mode", XSI::siControlCombo);
		layout.EndGroup();

		layout.AddGroup("Parameters");
		// displays
		XSI::CValueArray cm_displays_combo(ocio_config.displays_count * 2);
		for (size_t display_index = 0; display_index < ocio_config.displays_count; display_index++)
		{
			cm_displays_combo[display_index * 2] = ocio_config.displays[display_index].name;
			cm_displays_combo[display_index * 2 + 1] = LONG(display_index);
		}
		layout.AddEnumControl("cm_display_index", cm_displays_combo, "Display", XSI::siControlCombo);
		XSI::Parameter cm_display_index_parameter = parameters.GetItem("cm_display_index");
		int cm_display_index = cm_display_index_parameter.GetValue();
		if (cm_display_index < 0 || cm_display_index >= ocio_config.displays_count)
		{
			cm_display_index = ocio_config.default_display;
		}
		cm_display_index = std::min(cm_display_index, (int)ocio_config.displays_count - 1);
		cm_display_index_parameter.PutValue(cm_display_index);

		// views
		size_t display_views_count = ocio_config.displays[cm_display_index].views_count;
		XSI::CValueArray cm_views_combo(display_views_count * 2);
		for (size_t view_index = 0; view_index < display_views_count; view_index++)
		{
			cm_views_combo[view_index * 2] = ocio_config.displays[cm_display_index].views[view_index];
			cm_views_combo[view_index * 2 + 1] = LONG(view_index);
		}
		layout.AddEnumControl("cm_view_index", cm_views_combo, "View", XSI::siControlCombo);
		XSI::Parameter cm_view_index_parameter = parameters.GetItem("cm_view_index");
		int cm_view_index = cm_view_index_parameter.GetValue();
		if (cm_view_index < 0)
		{
			cm_view_index = 0;
		}

		if (cm_view_index >= display_views_count)
		{
			cm_view_index = display_views_count - 1;
		}
		cm_view_index_parameter.PutValue(cm_view_index);

		// looks
		XSI::CValueArray cm_looks_combo(2 + ocio_config.looks_count * 2);
		cm_looks_combo[0] = "None"; cm_looks_combo[1] = LONG(0);  // set None at start, actual look index will be-1 with this value
		for (size_t look_index = 0; look_index < ocio_config.looks_count; look_index++)
		{
			cm_looks_combo[2 + 2 * look_index] = ocio_config.looks[look_index];
			cm_looks_combo[2 + 2 * look_index + 1] = LONG(look_index + 1);
		}
		XSI::Parameter cm_look_index_parameter = parameters.GetItem("cm_look_index");
		int cm_look_index = cm_look_index_parameter.GetValue();
		if (cm_look_index < 0 || cm_look_index > ocio_config.looks_count)
		{
			cm_look_index = 0;
			cm_look_index_parameter.PutValue(cm_look_index);
		}
		layout.AddEnumControl("cm_look_index", cm_looks_combo, "Look", XSI::siControlCombo);
		layout.AddItem("cm_exposure", "Exposure");
		layout.AddItem("cm_gamma", "Gamma");
		layout.EndGroup();
	}
	else
	{
		XSI::CValueArray cm_mode_combo(2);
		cm_mode_combo[0] = "Simple sRGB"; cm_mode_combo[1] = 0;
		layout.AddEnumControl("cm_mode", cm_mode_combo, "Mode", XSI::siControlCombo);
		layout.EndGroup();
	}
	layout.EndGroup();

	layout.AddTab("Output");
	layout.AddGroup("Passes");
	layout.AddItem("output_pass_alpha_threshold", "Alpha Threshold");
	layout.AddItem("output_pass_assign_unique_pass_id", "Assign Unique Object Pass Id");
	layout.AddGroup("Preview AOV/Lightgroup Name");
	XSI::PPGItem item = layout.AddItem("output_pass_preview_name", "");
	item.PutAttribute("NoLabel", true);
	layout.EndGroup();
	layout.EndGroup();

	layout.AddGroup("Multilayer EXR");
	layout.AddItem("output_exr_combine_passes", "Combine Render Passes To Single EXR");
	layout.AddItem("output_exr_denoising_data", "Include Denoising Passes");
	layout.AddItem("output_exr_render_separate_passes", "Save Separate Passes");
	layout.EndGroup();

	layout.AddGroup("Cryptomatte");
	layout.AddItem("output_crypto_object", "Object");
	layout.AddItem("output_crypto_material", "Material");
	layout.AddItem("output_crypto_asset", "Asset");
	layout.AddItem("output_crypto_levels", "Levels");
	layout.EndGroup();

	layout.AddGroup("Labels");
	layout.AddItem("output_label_render_time", "Render Time");
	layout.AddItem("output_label_samples", "Samples");
	layout.AddItem("output_label_time", "Date");
	layout.AddItem("output_label_frame", "Frame");
	layout.AddItem("output_label_scene", "Scene");
	layout.AddItem("output_label_camera", "Camera");
	layout.AddItem("output_label_triangles_count", "Triangles");
	layout.AddItem("output_label_curves_count", "Curves");
	layout.AddItem("output_label_objects_count", "Objects");
	layout.EndGroup();

	layout.AddTab("World");
	layout.AddGroup("Mist");
	layout.AddItem("mist_start", "Start");
	layout.AddItem("mist_depth", "Depth");
	layout.AddItem("mist_falloff", "Falloff");
	layout.EndGroup();

	layout.AddGroup("Surface");
	XSI::CValueArray back_sampling_method_combo(4);
	back_sampling_method_combo[0] = "Auto"; back_sampling_method_combo[1] = 0;
	back_sampling_method_combo[2] = "Manual"; back_sampling_method_combo[3] = 1;
	layout.AddEnumControl("background_surface_sampling_method", back_sampling_method_combo, "Sampling", XSI::siControlCombo);
	layout.AddItem("background_surface_resolution", "Map Resolution");
	layout.AddItem("background_surface_max_bounces", "Max Bounces");
	layout.AddItem("background_surface_shadow_caustics", "Shadow Caustics");
	layout.EndGroup();

	layout.AddGroup("Volume");
	XSI::CValueArray back_sampling_combo(6);
	back_sampling_combo[0] = "Distance"; back_sampling_combo[1] = 0;
	back_sampling_combo[2] = "Equiangular"; back_sampling_combo[3] = 1;
	back_sampling_combo[4] = "Multiple Importance"; back_sampling_combo[5] = 2;
	layout.AddEnumControl("background_volume_sampling", back_sampling_combo, "Sampling", XSI::siControlCombo);
	XSI::CValueArray back_interpolation_combo(4);
	back_interpolation_combo[0] = "Linear"; back_interpolation_combo[1] = 0;
	back_interpolation_combo[2] = "Cubic"; back_interpolation_combo[3] = 1;
	layout.AddEnumControl("background_volume_interpolation", back_interpolation_combo, "Interpolation", XSI::siControlCombo);
	layout.AddItem("background_volume_homogeneous", "Homogeneous");
	layout.AddItem("background_volume_step_rate", "Step Size");
	layout.EndGroup();

	layout.AddGroup("Light Group");
	layout.AddItem("background_lightgroup", "Light Group");
	layout.EndGroup();

	layout.AddGroup("Ray Visibility");
	layout.AddItem("background_ray_visibility_diffuse", "Diffuse");
	layout.AddItem("background_ray_visibility_glossy", "Glossy");
	layout.AddItem("background_ray_visibility_transmission", "Transmission");
	layout.AddItem("background_ray_visibility_scatter", "Volume Scatter");
	layout.EndGroup();

	bool is_optix = is_optix_available();
	bool is_oidn = false;
#ifdef WITH_OPENIMAGEDENOISE
	is_oidn = true;
#endif // WITH_OPENIMAGEDENOISE

	// always use custom implementation of the oidn
	is_oidn = true;

	if (is_optix || is_oidn)
	{
		// if both optix and oidn are not allowed, then nothing to show
		layout.AddTab("Denoise");
		layout.AddGroup("Denoise");
		XSI::CValueArray denoise_mode_enum(is_optix ? (is_oidn ? 6 : 4) : (is_oidn ? 4 : 2));
		denoise_mode_enum[0] = "Disable"; denoise_mode_enum[1] = 0;
		size_t mode_index = 1;
		if (is_oidn)
		{
			denoise_mode_enum[2 * mode_index] = "OpenImageDenoise"; denoise_mode_enum[2 * mode_index + 1] = 1;
			mode_index++;
		}
		if (is_optix)
		{
			denoise_mode_enum[2 * mode_index] = "OptiX"; denoise_mode_enum[2 * mode_index + 1] = 2;
		}

		// in any case show denoise channels
		layout.AddEnumControl("denoise_mode", denoise_mode_enum, "Denoise Mode", XSI::siControlCombo);
		XSI::CValueArray denoise_channels_enum(6);
		denoise_channels_enum[0] = "None"; denoise_channels_enum[1] = 0;
		denoise_channels_enum[2] = "Albedo"; denoise_channels_enum[3] = 1;
		denoise_channels_enum[4] = "Albedo and Normal"; denoise_channels_enum[5] = 2;
		layout.AddEnumControl("denoise_channels", denoise_channels_enum, "Passes", XSI::siControlCombo);
		layout.EndGroup();
	}
	
	layout.AddTab("Options");
	layout.AddGroup("Shaders");
	XSI::CValueArray emission_sampling_combo(10);
	emission_sampling_combo[0] = "None"; emission_sampling_combo[1] = 0;
	emission_sampling_combo[2] = "Auto"; emission_sampling_combo[3] = 1;
	emission_sampling_combo[4] = "Front"; emission_sampling_combo[5] = 2;
	emission_sampling_combo[6] = "Back"; emission_sampling_combo[7] = 3;
	emission_sampling_combo[8] = "Front and Back"; emission_sampling_combo[9] = 4;
	layout.AddEnumControl("options_shaders_emission_sampling", emission_sampling_combo, "Emission Sampling", XSI::siControlCombo);
	layout.AddItem("options_shaders_transparent_shadows", "Transparent Shadows");
#ifdef WITH_OSL
	XSI::CValueArray shader_system_combo(4);
	shader_system_combo[0] = "SVM"; shader_system_combo[1] = 0;
	shader_system_combo[2] = "OSL"; shader_system_combo[3] = 1;
	layout.AddEnumControl("options_shaders_system", shader_system_combo, "Shading System", XSI::siControlCombo);
#endif
	layout.EndGroup();

	layout.AddGroup("Update");
	XSI::CValueArray update_combo(4);
	update_combo[0] = "Always Abort"; update_combo[1] = 0;
	update_combo[2] = "Update and Abort"; update_combo[3] = 1;
	layout.AddEnumControl("options_update_method", update_combo, "Mode", XSI::siControlCombo);
	layout.EndGroup();

	layout.AddGroup("Logging");
	layout.AddItem("options_logging_log_rendertime", "Log Rendertime");
	layout.AddItem("options_logging_log_details", "Log Statistics");
	layout.AddItem("options_logging_log_profiling", "Profiling");
	layout.EndGroup();

	layout.AddGroup("Displacement");
	XSI::CValueArray displacement_combo(6);
	displacement_combo[0] = "Bump"; displacement_combo[1] = 0;
	displacement_combo[2] = "True Displacement"; displacement_combo[3] = 1;
	displacement_combo[4] = "Both Displacement"; displacement_combo[5] = 2;
	layout.AddEnumControl("options_displacement_method", displacement_combo, "Displacement Method", XSI::siControlCombo);
	layout.EndGroup();

	layout.AddGroup("Devices");
	XSI::CStringArray device_names = get_available_devices_names();
	for (LONG device_index = 0; device_index < device_names.GetCount(); device_index++)
	{
		layout.AddItem("device_" + XSI::CString(device_index), device_names[device_index]);
	}
	layout.EndGroup();
}

void set_sampling(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter sampling_render_use_adaptive = prop_array.GetItem("sampling_render_use_adaptive");
	bool use_adaptive = sampling_render_use_adaptive.GetValue();

	XSI::Parameter sampling_render_adaptive_threshold = prop_array.GetItem("sampling_render_adaptive_threshold");
	sampling_render_adaptive_threshold.PutCapabilityFlag(block_mode, !use_adaptive);

	XSI::Parameter sampling_render_adaptive_min_samples = prop_array.GetItem("sampling_render_adaptive_min_samples");
	sampling_render_adaptive_min_samples.PutCapabilityFlag(block_mode, !use_adaptive);
}

void set_path_guiding(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter sampling_path_guiding_use = prop_array.GetItem("sampling_path_guiding_use");
	bool use_path_guiding = sampling_path_guiding_use.GetValue();

	XSI::Parameter sampling_path_guiding_surface = prop_array.GetItem("sampling_path_guiding_surface");
	sampling_path_guiding_surface.PutCapabilityFlag(block_mode, !use_path_guiding);

	XSI::Parameter sampling_path_guiding_volume = prop_array.GetItem("sampling_path_guiding_volume");
	sampling_path_guiding_volume.PutCapabilityFlag(block_mode, !use_path_guiding);

	XSI::Parameter sampling_path_guiding_training_samples = prop_array.GetItem("sampling_path_guiding_training_samples");
	sampling_path_guiding_training_samples.PutCapabilityFlag(block_mode, !use_path_guiding);
}

void set_advances_sampling(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter sampling_advanced_pattern = prop_array.GetItem("sampling_advanced_pattern");
	int pattern = sampling_advanced_pattern.GetValue();

	XSI::Parameter sampling_advanced_scrambling_distance = prop_array.GetItem("sampling_advanced_scrambling_distance");
	sampling_advanced_scrambling_distance.PutCapabilityFlag(block_mode, pattern == 0);

	XSI::Parameter sampling_advanced_scrambling_multiplier = prop_array.GetItem("sampling_advanced_scrambling_multiplier");
	sampling_advanced_scrambling_multiplier.PutCapabilityFlag(block_mode, pattern == 0);
}

void set_lights(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter sampling_advanced_light_tree = prop_array.GetItem("sampling_advanced_light_tree");
	bool use_tree = sampling_advanced_light_tree.GetValue();

	XSI::Parameter sampling_advanced_light_threshold = prop_array.GetItem("sampling_advanced_light_threshold");
	sampling_advanced_light_threshold.PutCapabilityFlag(block_mode, !use_tree);
}

void set_fastgi(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter paths_fastgi_use = prop_array.GetItem("paths_fastgi_use");
	bool use_fastgi = paths_fastgi_use.GetValue();

	XSI::Parameter paths_fastgi_ao_factor = prop_array.GetItem("paths_fastgi_ao_factor");
	paths_fastgi_ao_factor.PutCapabilityFlag(block_mode, !use_fastgi);

	XSI::Parameter paths_fastgi_ao_distance = prop_array.GetItem("paths_fastgi_ao_distance");
	paths_fastgi_ao_distance.PutCapabilityFlag(block_mode, !use_fastgi);

	XSI::Parameter paths_fastgi_method = prop_array.GetItem("paths_fastgi_method");
	paths_fastgi_method.PutCapabilityFlag(block_mode, !use_fastgi);

	int fastgi_mode = paths_fastgi_method.GetValue();

	XSI::Parameter paths_fastgi_ao_bounces = prop_array.GetItem("paths_fastgi_ao_bounces");
	paths_fastgi_ao_bounces.PutCapabilityFlag(block_mode, !use_fastgi || fastgi_mode == 1);
}

void set_curves(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter performance_curves_type = prop_array.GetItem("performance_curves_type");
	int curve_mode = performance_curves_type.GetValue();

	XSI::Parameter performance_curves_subdivs = prop_array.GetItem("performance_curves_subdivs");
	performance_curves_subdivs.PutCapabilityFlag(block_mode, curve_mode == 1);
}

void set_film_filter(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter film_filter_type = prop_array.GetItem("film_filter_type");
	int filter_type = film_filter_type.GetValue();

	XSI::Parameter film_filter_width = prop_array.GetItem("film_filter_width");
	film_filter_width.PutCapabilityFlag(block_mode, filter_type == 0);
}

void set_film_transparent(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter film_transparent = prop_array.GetItem("film_transparent");
	bool is_transparent = film_transparent.GetValue();

	XSI::Parameter film_transparent_glass = prop_array.GetItem("film_transparent_glass");
	film_transparent_glass.PutCapabilityFlag(block_mode, !is_transparent);

	bool glass_transparent = film_transparent_glass.GetValue();

	XSI::Parameter film_transparent_roughness = prop_array.GetItem("film_transparent_roughness");
	film_transparent_roughness.PutCapabilityFlag(block_mode, !is_transparent || !glass_transparent);
}

void set_film_motion(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter film_motion_use = prop_array.GetItem("film_motion_use");
	bool use_motion = film_motion_use.GetValue();

	XSI::Parameter film_motion_position = prop_array.GetItem("film_motion_position");
	film_motion_position.PutCapabilityFlag(block_mode, !use_motion);

	XSI::Parameter film_motion_steps = prop_array.GetItem("film_motion_steps");
	film_motion_steps.PutCapabilityFlag(block_mode, !use_motion);

	XSI::Parameter film_motion_shutter = prop_array.GetItem("film_motion_shutter");
	film_motion_shutter.PutCapabilityFlag(block_mode, !use_motion);

	XSI::Parameter film_motion_rolling_type = prop_array.GetItem("film_motion_rolling_type");
	film_motion_rolling_type.PutCapabilityFlag(block_mode, !use_motion);

	int rolling_type = film_motion_rolling_type.GetValue();

	XSI::Parameter film_motion_rolling_duration = prop_array.GetItem("film_motion_rolling_duration");
	film_motion_rolling_duration.PutCapabilityFlag(block_mode, !use_motion || rolling_type == 0);
}

void set_threads(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter performance_threads_mode = prop_array.GetItem("performance_threads_mode");
	int threads_mode = performance_threads_mode.GetValue();

	XSI::Parameter performance_threads_count = prop_array.GetItem("performance_threads_count");
	performance_threads_count.PutCapabilityFlag(block_mode, threads_mode == 0);
}

void set_memory(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter performance_memory_use_auto_tile = prop_array.GetItem("performance_memory_use_auto_tile");
	bool auto_tile = performance_memory_use_auto_tile.GetValue();

	XSI::Parameter performance_memory_tile_size = prop_array.GetItem("performance_memory_tile_size");
	performance_memory_tile_size.PutCapabilityFlag(block_mode, !auto_tile);
}

void set_culling(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter performance_simplify_cull_camera = prop_array.GetItem("performance_simplify_cull_camera");
	bool is_camera = performance_simplify_cull_camera.GetValue();

	XSI::Parameter performance_simplify_cull_distance = prop_array.GetItem("performance_simplify_cull_distance");
	bool is_distance = performance_simplify_cull_distance.GetValue();

	XSI::Parameter performance_simplify_cull_camera_margin = prop_array.GetItem("performance_simplify_cull_camera_margin");
	performance_simplify_cull_camera_margin.PutCapabilityFlag(block_mode, !is_camera);

	XSI::Parameter performance_simplify_cull_distance_margin = prop_array.GetItem("performance_simplify_cull_distance_margin");
	performance_simplify_cull_distance_margin.PutCapabilityFlag(block_mode, !is_distance);
}

void set_multilayer_exr(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter output_exr_combine_passes = prop_array.GetItem("output_exr_combine_passes");
	bool is_multilayer = output_exr_combine_passes.GetValue();

	XSI::Parameter output_exr_render_separate_passes = prop_array.GetItem("output_exr_render_separate_passes");
	output_exr_render_separate_passes.PutCapabilityFlag(block_mode, !is_multilayer);

	XSI::Parameter output_exr_denoising_data = prop_array.GetItem("output_exr_denoising_data");
	output_exr_denoising_data.PutCapabilityFlag(block_mode, !is_multilayer);
}

void set_cryptomatte(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter output_crypto_object = prop_array.GetItem("output_crypto_object");
	bool is_object = output_crypto_object.GetValue();

	XSI::Parameter output_crypto_material = prop_array.GetItem("output_crypto_material");
	bool is_material = output_crypto_material.GetValue();

	XSI::Parameter output_crypto_asset = prop_array.GetItem("output_crypto_asset");
	bool is_asset = output_crypto_asset.GetValue();

	XSI::Parameter output_crypto_levels = prop_array.GetItem("output_crypto_levels");
	output_crypto_levels.PutCapabilityFlag(block_mode, !is_object && !is_material && !is_asset);
}

void set_background_volume(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter background_volume_homogeneous = prop_array.GetItem("background_volume_homogeneous");
	bool is_homogeneous = background_volume_homogeneous.GetValue();

	XSI::Parameter background_volume_step_rate = prop_array.GetItem("background_volume_step_rate");
	background_volume_step_rate.PutCapabilityFlag(block_mode, is_homogeneous);
}

void set_background_surface(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter background_surface_sampling_method = prop_array.GetItem("background_surface_sampling_method");
	int sampling = background_surface_sampling_method.GetValue();

	XSI::Parameter background_surface_max_bounces = prop_array.GetItem("background_surface_max_bounces");
	background_surface_max_bounces.PutCapabilityFlag(block_mode, sampling == 0);

	XSI::Parameter background_surface_resolution = prop_array.GetItem("background_surface_resolution");
	background_surface_resolution.PutCapabilityFlag(block_mode, sampling != 2);
}

void set_denoising(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter denoise_mode = prop_array.GetItem("denoise_mode");
	int mode = denoise_mode.GetValue();

	XSI::Parameter denoise_channels = prop_array.GetItem("denoise_channels");
	denoise_channels.PutCapabilityFlag(block_mode, mode == 0);
}

void set_colormanagement(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter cm_mode = prop_array.GetItem("cm_mode");
	int mode = cm_mode.GetValue();
	XSI::Parameter cm_apply_to_ldr = prop_array.GetItem("cm_apply_to_ldr");
	bool cm_apply = cm_apply_to_ldr.GetValue();

	cm_mode.PutCapabilityFlag(block_mode, !cm_apply);

	XSI::Parameter cm_display_index = prop_array.GetItem("cm_display_index");
	cm_display_index.PutCapabilityFlag(block_mode, mode == 0 || !cm_apply);

	XSI::Parameter cm_view_index = prop_array.GetItem("cm_view_index");
	cm_view_index.PutCapabilityFlag(block_mode, mode == 0 || !cm_apply);

	XSI::Parameter cm_look_index = prop_array.GetItem("cm_look_index");
	cm_look_index.PutCapabilityFlag(block_mode, mode == 0 || !cm_apply);

	XSI::Parameter cm_exposure = prop_array.GetItem("cm_exposure");
	cm_exposure.PutCapabilityFlag(block_mode, mode == 0 || !cm_apply);

	XSI::Parameter cm_gamma = prop_array.GetItem("cm_gamma");
	cm_gamma.PutCapabilityFlag(block_mode, mode == 0 || !cm_apply);

	OCIOConfig ocio_config = get_ocio_config();
	int display_index = cm_display_index.GetValue();
	OCIODisplay ocio_display = ocio_config.displays[display_index];
	int views_count = ocio_display.views_count;
	if ((int)cm_view_index.GetValue() >= views_count)
	{
		cm_view_index.PutValue(views_count - 1);
	}
}

void set_logging(XSI::CustomProperty& prop)
{
	XSI::CParameterRefArray prop_array = prop.GetParameters();

	XSI::Parameter options_logging_log_details = prop_array.GetItem("options_logging_log_details");
	bool is_log = options_logging_log_details.GetValue();

	XSI::Parameter options_logging_log_profiling = prop_array.GetItem("options_logging_log_profiling");
	options_logging_log_profiling.PutCapabilityFlag(block_mode, !is_log);
}

XSI::CStatus RenderEngineCyc::render_options_update(XSI::PPGEventContext& event_context) 
{
	XSI::PPGEventContext::PPGEvent event_id = event_context.GetEventID();
	bool is_refresh = false;
	if (event_id == XSI::PPGEventContext::siOnInit) 
	{
		XSI::CustomProperty cp_source = event_context.GetSource();

		set_sampling(cp_source);
		set_path_guiding(cp_source);
		set_advances_sampling(cp_source);
		set_lights(cp_source);
		set_fastgi(cp_source);
		set_curves(cp_source);
		set_film_filter(cp_source);
		set_film_transparent(cp_source);
		set_film_motion(cp_source);
		set_threads(cp_source);
		set_memory(cp_source);
		set_culling(cp_source);
		set_multilayer_exr(cp_source);
		set_cryptomatte(cp_source);
		set_background_volume(cp_source);
		set_background_surface(cp_source);
		set_denoising(cp_source);
		set_colormanagement(cp_source);
		set_logging(cp_source);
	}
	else if (event_id == XSI::PPGEventContext::siParameterChange) 
	{
		XSI::Parameter changed = event_context.GetSource();
		XSI::CustomProperty prop = changed.GetParent();
		XSI::CString param_name = changed.GetScriptName();

		if (param_name == "sampling_render_use_adaptive")
		{
			set_sampling(prop);
		}
		else if (param_name == "sampling_path_guiding_use")
		{
			set_path_guiding(prop);
		}
		else if (param_name == "sampling_advanced_pattern")
		{
			set_advances_sampling(prop);
		}
		else if (param_name == "sampling_advanced_light_tree")
		{
			set_lights(prop);
		}
		else if (param_name == "paths_fastgi_use" || param_name == "paths_fastgi_method")
		{
			set_fastgi(prop);
		}
		else if (param_name == "performance_curves_type")
		{
			set_curves(prop);
		}
		else if (param_name == "film_filter_type")
		{
			set_film_filter(prop);
		}
		else if (param_name == "film_transparent" || param_name == "film_transparent_glass")
		{
			set_film_transparent(prop);
		}
		else if (param_name == "film_motion_use" || param_name == "film_motion_rolling_type")
		{
			set_film_motion(prop);
		}
		else if (param_name == "performance_threads_mode")
		{
			set_threads(prop);
		}
		else if (param_name == "performance_memory_use_auto_tile")
		{
			set_memory(prop);
		}
		else if (param_name == "performance_simplify_cull_camera" || param_name == "performance_simplify_cull_distance")
		{
			set_culling(prop);
		}
		else if (param_name == "output_exr_combine_passes")
		{
			set_multilayer_exr(prop);
		}
		else if (param_name == "output_crypto_object" || param_name == "output_crypto_material" || param_name == "output_crypto_asset")
		{
			set_cryptomatte(prop);
		}
		else if (param_name == "background_volume_homogeneous")
		{
			set_background_volume(prop);
		}
		else if (param_name == "background_surface_sampling_method")
		{
			set_background_surface(prop);
		}
		else if (param_name == "denoise_mode")
		{
			set_denoising(prop);
		}
		else if (param_name == "cm_mode" || param_name == "cm_display_index" || param_name == "cm_apply_to_ldr")
		{
			XSI::PPGLayout ppg_layout = prop.GetPPGLayout();
			build_layout(ppg_layout, prop.GetParameters());
			set_colormanagement(prop);

			is_refresh = true;
		}
		else if (param_name == "options_logging_log_details")
		{
			set_logging(prop);
		}
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

	build_layout(layout, property.GetParameters());

	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineCyc::render_option_define(XSI::CustomProperty& property)
{
	const int caps = XSI::siPersistable | XSI::siAnimatable;
	XSI::Parameter param;

	// sampling tab
	// render
	property.AddParameter("sampling_render_samples", XSI::CValue::siInt4, caps, "", "", 4, 1, ccl::Integrator::MAX_SAMPLES, 1, 256, param);
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
	property.AddParameter("sampling_advanced_light_tree", XSI::CValue::siBool, caps, "", "", true, param);
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
	property.AddParameter("paths_fastgi_ao_bounces", XSI::CValue::siInt4, caps, "", "", 1, 0, 1024, 0, 16, param);

	// film tab
	property.AddParameter("film_exposure", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 10.0, 0.0, 2.0, param);

	// pixel filter
	property.AddParameter("film_filter_type", XSI::CValue::siInt4, caps, "", "", 2, 0, 2, 0, 2, param);
	property.AddParameter("film_filter_width", XSI::CValue::siFloat, caps, "", "", 1.5, 0.01, 10.0, 0.5, 2.5, param);

	// transparent
	property.AddParameter("film_transparent", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("film_transparent_glass", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("film_transparent_roughness", XSI::CValue::siDouble, caps, "", "", 0.1, 0.0, 1.0, 0.0, 1.0, param);

	// motion
	property.AddParameter("film_motion_use", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("film_motion_steps", XSI::CValue::siInt4, caps, "", "", 1, 1, 7, 1, 3, param);
	property.AddParameter("film_motion_position", XSI::CValue::siInt4, caps, "", "", 1, param);
	property.AddParameter("film_motion_shutter", XSI::CValue::siFloat, caps, "", "", 0.5, 0.01, FLT_MAX, 0.01, 1.0, param);
	property.AddParameter("film_motion_rolling_type", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("film_motion_rolling_duration", XSI::CValue::siFloat, caps, "", "", 0.1, 0.0, 1.0, 0.0, 1.0, param);

	// performance tab
	// threads
	property.AddParameter("performance_threads_mode", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("performance_threads_count", XSI::CValue::siInt4, caps, "", "", 0, 0, INT_MAX, 0, 12, param);

	// memory
	property.AddParameter("performance_memory_use_auto_tile", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_memory_tile_size", XSI::CValue::siInt4, caps, "", "", 2048, 8, INT_MAX, 8, 4096, param);

	// acceleration structure
	property.AddParameter("performance_acceleration_use_spatial_split", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_acceleration_use_compact_bvh", XSI::CValue::siBool, caps, "", "", false, param);

	// volumes
	property.AddParameter("performance_volume_step_rate", XSI::CValue::siFloat, caps, "", "", 1.0, 0.1, 10.0, 0.1, 10.0, param);
	property.AddParameter("performance_volume_max_steps", XSI::CValue::siInt4, caps, "", "", 1024, 0, INT_MAX, 0, 2048, param);

	// curves
	property.AddParameter("performance_curves_type", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("performance_curves_subdivs", XSI::CValue::siInt4, caps, "", "", 2, 0, 24, 0, 6, param);

	// culling
	property.AddParameter("performance_simplify_cull_mode", XSI::CValue::siInt4, caps, "", "", 0, 0, 1, 0, 1, param);
	property.AddParameter("performance_simplify_cull_camera", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_simplify_cull_distance", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("performance_simplify_cull_camera_margin", XSI::CValue::siFloat, caps, "", "", 0.1, 0.0, 5.0, 0.0, 5.0, param);
	property.AddParameter("performance_simplify_cull_distance_margin", XSI::CValue::siFloat, caps, "", "", 50.0, 0.0, FLT_MAX, 0.0, 100.0, param);

	// color management tab
	OCIOConfig ocio_config = get_ocio_config();
	property.AddParameter("cm_apply_to_ldr", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("cm_mode", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("cm_display_index", XSI::CValue::siInt4, caps, "", "", 0, param);  // <-- default device index
	property.AddParameter("cm_view_index", XSI::CValue::siInt4, caps, "", "", 2, param);  // by deafult set AgX, the maximum nuber of view transforms are different for different devices
	property.AddParameter("cm_look_index", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("cm_exposure", XSI::CValue::siFloat, caps, "", "", 0.0, -1024.0, 1024.0, -10.0, 10.0, param);
	property.AddParameter("cm_gamma", XSI::CValue::siFloat, caps, "", "", 1.0, 0.0, 1024.0, 0.0, 5.0, param);

	// output tab
	// passes
	property.AddParameter("output_pass_alpha_threshold", XSI::CValue::siFloat, caps, "", "", 0.5, 0.0, 1.0, 0.0, 1.0, param);
	property.AddParameter("output_pass_assign_unique_pass_id", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_pass_preview_name", XSI::CValue::siString, caps, "", "", "", param);  // used for preview aov passes

	// multilayer exr
	property.AddParameter("output_exr_combine_passes", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_exr_render_separate_passes", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("output_exr_denoising_data", XSI::CValue::siBool, caps, "", "", false, param);

	// cryptomatte
	property.AddParameter("output_crypto_object", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_material", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_asset", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("output_crypto_levels", XSI::CValue::siInt4, caps, "", "", 6, 2, 16, 2, 16, param);

	// labels
	property.AddParameter("output_label_render_time", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_time", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_frame", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_scene", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_camera", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_samples", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_objects_count", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_triangles_count", XSI::CValue::siBool, caps, "", "", 0, param);
	property.AddParameter("output_label_curves_count", XSI::CValue::siBool, caps, "", "", 0, param);

	// world tab
	// mist
	property.AddParameter("mist_start", XSI::CValue::siFloat, caps, "", "", 5.0, 0.0, FLT_MAX, 0.0, 16.0, param);
	property.AddParameter("mist_depth", XSI::CValue::siFloat, caps, "", "", 25.0, 0.0, FLT_MAX, 0.0, 64.0, param);
	property.AddParameter("mist_falloff", XSI::CValue::siFloat, caps, "", "", 2.0, 0.0, FLT_MAX, 0.0, 2.0, param);

	// background
	// visibility
	property.AddParameter("background_ray_visibility_camera", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_diffuse", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_glossy", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_transmission", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("background_ray_visibility_scatter", XSI::CValue::siBool, caps, "", "", true, param);
	// volume
	property.AddParameter("background_volume_sampling", XSI::CValue::siInt4, caps, "", "", 1, param);
	property.AddParameter("background_volume_interpolation", XSI::CValue::siInt4, caps, "", "", 0, param);
	property.AddParameter("background_volume_homogeneous", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("background_volume_step_rate", XSI::CValue::siFloat, caps, "", "", 1.0, 0.01, 100.0, 0.1, 10.0, param);
	// surface
	property.AddParameter("background_surface_sampling_method", XSI::CValue::siUInt1, caps, "", "", 0, param);
	property.AddParameter("background_surface_max_bounces", XSI::CValue::siUInt2, caps, "", "", 1024, 0, 1024, 0, 1024, param);
	property.AddParameter("background_surface_resolution", XSI::CValue::siInt4, caps, "", "", 1024, 4, 8191, 4, 2048, param);
	property.AddParameter("background_surface_shadow_caustics", XSI::CValue::siBool, caps, "", "", false, param);
	// lightgroup
	property.AddParameter("background_lightgroup", XSI::CValue::siString, caps, "", "", "", param);

	// denoising tab
	property.AddParameter("denoise_mode", XSI::CValue::siInt4, caps, "", "", 0, 0, 2, 0, 2, param);
	property.AddParameter("denoise_channels", XSI::CValue::siInt4, caps, "", "", 2, 0, 2, 0, 2, param);  // for Optix and OIDenoise modes

	// options tab
	// shaders
	property.AddParameter("options_shaders_emission_sampling", XSI::CValue::siInt4, caps, "", "", 1, param);
	property.AddParameter("options_shaders_transparent_shadows", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("options_shaders_system", XSI::CValue::siInt4, caps, "", "", 0, param);

	// logging
	property.AddParameter("options_logging_log_rendertime", XSI::CValue::siBool, caps, "", "", true, param);
	property.AddParameter("options_logging_log_details", XSI::CValue::siBool, caps, "", "", false, param);
	property.AddParameter("options_logging_log_profiling", XSI::CValue::siBool, caps, "", "", false, param);

	// displacement
	property.AddParameter("options_displacement_method", XSI::CValue::siInt4, caps, "", "", 2, param);

	// updates
	property.AddParameter("options_update_method", XSI::CValue::siInt4, caps, "", "", 1, param);  // 0 - only abort, 1 - update and abort, 2 - only update (what it is mean?)

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