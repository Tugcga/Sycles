#include <chrono>
#include <thread>

#include "xsi_primitive.h"
#include "xsi_application.h"
#include "xsi_project.h"
#include "xsi_library.h"
#include "xsi_renderchannel.h"
#include "xsi_framebuffer.h"
#include "xsi_projectitem.h"
#include "xsi_utils.h"

#include "util/path.h"

#include "render_engine_cyc.h"
#include "cyc_session/cyc_session.h"
#include "cyc_scene/cyc_scene.h"
#include "cyc_output/output_drivers.h"
#include "cyc_scene/cyc_geometry/cyc_geometry.h"
#include "../utilities/logs.h"
#include "../output/write_image.h"
#include "../utilities/arrays.h"
#include "../utilities/files_io.h"
#include "../utilities/io_exr.h"
#include "../utilities/math.h"
#include "../utilities/xsi_properties.h"

using namespace std::chrono_literals;

RenderEngineCyc::RenderEngineCyc()
{
	is_session = false;
	call_abort_render = false;
	output_context = new OutputContext();
	labels_context = new LabelsContext();
	color_transform_context = new ColorTransformContext();
	update_context = new UpdateContext();
	baking_context = new BakingContext();
	series_context = new SeriesContext();

	make_render = true;
	is_recreate_session = true;
	display_pass_name = "";
	create_new_scene = true;
	is_update_camera = false;
}

// when we delete the engine, then at first this method is called, and then the method from base class
RenderEngineCyc::~RenderEngineCyc()
{
	clear_session();
	delete output_context;
	delete labels_context;
	delete color_transform_context;
	delete update_context;
	delete baking_context;
	delete series_context;
}

void RenderEngineCyc::path_init(const XSI::CString& plugin_path)
{
	XSI::CString folder_path = XSI::CUtils::BuildPath(plugin_path, "..", "..");
	ccl::path_init(folder_path.GetAsciiString());

	// here we also can setup series rendering
	InputConfig input_config = get_input_config();
	if (input_config.is_init)
	{
		series_context->setup(input_config.series);
	}

	// init openvdb
	openvdb::initialize();
}

void RenderEngineCyc::clear_session()
{
	if (is_session)
	{
		session->cancel(true);
		session->device_free();
		delete session;
		session = NULL;
		is_session = false;
	}
}

// this method calls from output driver when next tile is come
// we should read pixels for all passes and save it into output array
void RenderEngineCyc::update_render_tile(const ccl::OutputDriver::Tile& tile)
{
	rendered_samples = session->progress.get_current_sample();

	// read tile size and offset
	unsigned int tile_width = tile.size.x;
	unsigned int tile_height = tile.size.y;
	unsigned int offset_x = tile.offset.x;  // this parameter defines offset inside the render buffer, not on the whole screen
	unsigned int offset_y = tile.offset.y;  // so, we should add it to image_corner to obtain actual in-screen offset
	ImageRectangle tile_roi = ImageRectangle(offset_x, offset_x + tile_width, offset_y, offset_y + tile_height);

	// create pixel buffer
	// we will use it for all passes
	std::vector<float> pixels((size_t)tile_width * tile_height * 4, 0.0f);
	// TODO: here is a problem when use tile rendering
	// more details in cycles_ui.cpp:133
	// shortly, tile contains some strange additional padding near actual tile pixels and required array with bigger size
	// and this padding has no constant size, so, it's hard properly extract actual pixels

	// we should get from tile pixels for each pass and save pixels into buffers (visual or output)
	bool is_get = false;
	if (render_type != RenderType::RenderType_Rendermap)
	{
		// use visual only for non baking render
		// get at first pixels for visual
		int visual_components = visual_buffer->get_components();
		is_get = tile.get_pass_pixels(visual_buffer->get_pass_name(), visual_components, pixels.data());
		if (is_get)
		{
			visual_buffer->add_pixels(tile_roi, pixels);

			// next we should create new fragment to visualize it at the screen
			// we does not need piszels anymore, so, we can apply color correction to this array
			// apply color correction only to combined pass
			if (render_type != RenderType_Shaderball && visual_buffer->get_pass_type() == ccl::PASS_COMBINED && visual_components == 4)  // actual Combined has 4 components, but lightgroups only 3
			{
				color_transform_context->apply(tile_width, tile_height, visual_components, &pixels[0]);
			}

			// for shaderball rendering apply simple sRGB
			// for all other cases use OCIO
			m_render_context.NewFragment(RenderTile(offset_x + image_corner_x, offset_y + image_corner_y, tile_width, tile_height, pixels, render_type == RenderType_Shaderball, visual_components));

		}
		else
		{
			log_message("Fails to get pixels for input render tile", XSI::siErrorMsg);
		}
	}

	// and next for each output pass
	for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
	{
		ccl::PassType pass_type = output_context->get_output_pass_type(i);
		ccl::ustring pass_name = output_context->get_output_pass_name(i);
		int pass_components = get_pass_components(pass_type, pass_type == ccl::PASS_COMBINED && is_start_from(pass_name, ccl::ustring("Combined_")));  // because lightgroup pass has the name Combined_... (length >= 9)
		is_get = tile.get_pass_pixels(pass_name, pass_components, &pixels[0]);
		if (is_get)
		{
			bool is_set = output_context->add_output_pixels(tile_roi, i, pixels);
		}
		else
		{
			log_message("Fails to get pixels of the pass " + XSI::CString(pass_name.c_str()) + " for input render tile", XSI::siErrorMsg);
		}
	}

	if (render_type == RenderType::RenderType_Pass && series_context->get_is_active())
	{
		if (rendered_samples - series_context->get_last_sample() > series_context->get_sampling_step())
		{
			series_context->set_last_sample(rendered_samples);
			if (series_context->need_beauty())
			{
				is_get = tile.get_pass_pixels(pass_to_name(ccl::PassType::PASS_COMBINED).GetAsciiString(), 4, &pixels[0]);
				if (is_get)
				{
					// we assume that tile contains the whole image
					write_output_exr(tile_width, tile_height, 4, series_context->build_output_path(ccl::PassType::PASS_COMBINED, eval_time).GetAsciiString(), &pixels[0]);
				}
			}

			if (series_context->need_albedo())
			{
				is_get = tile.get_pass_pixels(pass_to_name(ccl::PassType::PASS_DIFFUSE_COLOR).GetAsciiString(), 3, &pixels[0]);
				if (is_get)
				{
					write_output_exr(tile_width, tile_height, 3, series_context->build_output_path(ccl::PassType::PASS_DIFFUSE_COLOR, eval_time).GetAsciiString(), &pixels[0]);
				}
			}

			if (series_context->need_normal())
			{
				is_get = tile.get_pass_pixels(pass_to_name(ccl::PassType::PASS_NORMAL).GetAsciiString(), 3, &pixels[0]);
				if (is_get)
				{
					write_output_exr(tile_width, tile_height, 3, series_context->build_output_path(ccl::PassType::PASS_NORMAL, eval_time).GetAsciiString(), &pixels[0]);
				}
			}
		}
	}
}

// called in baking process by the Cycles engine
// when the scene is prepare for render
void RenderEngineCyc::read_render_tile(const ccl::OutputDriver::Tile& tile)
{
	int x = tile.offset.x;
	int y = tile.offset.y;
	int w = tile.size.x;
	int h = tile.size.y;

	ccl::vector<float> pixels(static_cast<size_t>(w) * h * 4);
	ImageRectangle rect = ImageRectangle(x, x + w, y, y + h);

	// PASS_BAKE_PRIMITIVE
	baking_context->get_buffer_primitive_id()->get_pixels(rect, &pixels[0]);
	bool is_primitive = tile.set_pass_pixels("BakePrimitive", 3, &pixels[0]);

	// PASS_BAKE_DIFFERENTIAL
	baking_context->get_buffer_differencial()->get_pixels(rect, &pixels[0]);
	bool is_differential = tile.set_pass_pixels("BakeDifferential", 4, &pixels[0]);

	// PASS_BAKE_SEED
	baking_context->get_buffer_seed()->get_pixels(rect, &pixels[0]);
	bool is_seed = tile.set_pass_pixels("BakeSeed", 1, &pixels[0]);

	pixels.clear();
	pixels.shrink_to_fit();
}

void RenderEngineCyc::postrender_visual_output()
{
	// overlay labels and apply color correction only for combined pass
	if (visual_buffer->get_pass_type() == ccl::PASS_COMBINED && render_type != RenderType_Shaderball && render_type != RenderType_Rendermap && visual_buffer->get_pass_name() == "Combined")
	{
		ULONG visual_width = visual_buffer->get_width();
		ULONG visual_height = visual_buffer->get_height();
		if (visual_width == visual_buffer->get_width() && visual_height == visual_buffer->get_height())
		{
			// here we shold use the copy of the visual buffer pixels, because original pixels may be used later in the update
			std::vector<float> visual_pixels = visual_buffer->get_buffer_pixels();
			size_t components = visual_buffer->get_components();

			// make color correction
			if (color_transform_context->get_use_correction())
			{
				color_transform_context->apply(visual_width, visual_height, components, &visual_pixels[0]);
			}

			// add labels
			if (output_context->get_is_labels())
			{
				overlay_pixels(visual_width, visual_height, output_context->get_labels_pixels(), &visual_pixels[0]);
			}

			// set render fragment
			m_render_context.NewFragment(RenderTile(image_corner_x, image_corner_y, visual_width, visual_height, visual_pixels, false, components));  // combined always have 4 components

			// clear vector
			visual_pixels.clear();
			visual_pixels.shrink_to_fit();
		}
		else
		{
			log_message("The size of visual buffer and labels buffer are different, this is not ok", XSI::siWarningMsg);
		}
	}

}

// in this callback we can show the actual progress of the render process
void RenderEngineCyc::progress_update_callback()
{
	double progress = session->progress.get_progress();
	m_render_context.ProgressUpdate("Rendering...", "Rendering...", int(progress * 100));
}

// int his callback we can stop the render, if we need this
void RenderEngineCyc::progress_cancel_callback()
{
	if (call_abort_render)
	{
		// Note: the Cycles stop rendering when we call, but actual stop after render curent tile
		// it updates tiles one times per second (or two), so, actual abort happens after some seconds
		session->progress.set_cancel("Abort render");
		call_abort_render = false;
	}
}

void RenderEngineCyc::pre_bake()
{
	// here we should only set output paths and formats
	const XSI::CString baking_object_type = baking_object.GetType();
	if (baking_object_type == XSI::siPolyMeshType && baking_object.IsValid() && baking_uv.Length() > 0)
	{
		baking_context->make_valid();
		baking_context->set_use_camera(false);
		XSI::Property bake_prop;
		bool is_bake_prop = get_xsi_object_property(baking_object, "CyclesBake", bake_prop);
		if (is_bake_prop)
		{
			XSI::CParameterRefArray prop_params = bake_prop.GetParameters();
			// there is a baking custom property
			image_full_size_width = powi(2, 5 + (int)(prop_params.GetValue("texture_size", eval_time)));
			image_full_size_height = image_full_size_width;
			image_size_width = image_full_size_width;
			image_size_height = image_full_size_width;

			baking_context->set_use_camera(((int)prop_params.GetValue("baking_view", eval_time)) == 1);

			XSI::CString out_file = prop_params.GetValue("output_name", eval_time);
			XSI::CString out_folder = prop_params.GetValue("output_folder", eval_time);
			XSI::CString out_ext = prop_params.GetValue("output_extension", eval_time);

			output_paths.Clear();
			output_paths.Add(out_folder + "\\" + out_file + "." + out_ext);
			output_formats.Clear();
			output_formats.Add(out_ext);

			// we support pfm(3, hdr), ppm(3, ldr), exr(3-4, hdr), png(3-4, ldr), bmp(3, ldr), tga(3, ldr), jpg(3, ldr), hdr(3, hdr)
			output_bits.clear();
			if (out_ext == "pfm" || out_ext == "exr" || out_ext == "hdr")
			{
				output_bits.push_back(21);
			}
			else
			{
				output_bits.push_back(3);
			}
			output_data_types.Clear();
			if (out_ext == "png" || out_ext == "exr")
			{
				output_data_types.Add("RGBA");
			}
			else
			{
				output_data_types.Add("RGB");
			}

			output_channels.Clear();
			output_channels.Add(prop_params.GetValue("baking_shader", eval_time));

			// also setup bake pass keys
			baking_context->set_keys(prop_params.GetValue("baking_filter_direct", eval_time),
				prop_params.GetValue("baking_filter_indirect", eval_time),
				prop_params.GetValue("baking_filter_color", eval_time),
				prop_params.GetValue("baking_filter_diffuse", eval_time),
				prop_params.GetValue("baking_filter_glossy", eval_time),
				prop_params.GetValue("baking_filter_transmission", eval_time),
				prop_params.GetValue("baking_filter_emission", eval_time));
		}
		else
		{
			// no custom property
			// check output extension
			for (ULONG i = 0; i < output_paths.GetCount(); i++)
			{
				XSI::CString format = output_formats[i];
				XSI::CString output_path = output_paths[i];
				if (!is_output_extension_supported(format))
				{
					// change output format to exr
					log_message("Invalid image extension " + format + " for output rendermap. Change it to exr.", XSI::siWarningMsg);
					ULONG dot_index = output_path.ReverseFindString(".");
					output_paths[i] = output_path.GetSubString(0, dot_index) + ".exr";
					output_formats[i] = "exr";
					output_bits[i] = 21;
					output_data_types[i] = "RGBA";
				}

				output_channels[i] = "Main";
			}

			baking_context->set_keys(true, true, true, true, true, true, true);
		}
	}
	else
	{
		// bake object is invalid
		// so, nothing to bake
		baking_context->make_invalid();
		log_message("Baking is fail, object is invalid" + 
			baking_object_type != XSI::siPolyMeshType ? ", type must be polymesh" : (
				baking_uv.Length() == 0 ? ", no uv coordinates" : ""), XSI::siWarningMsg);
	}
}

// nothing to do here
XSI::CStatus RenderEngineCyc::pre_render_engine()
{
	return XSI::CStatus::OK;
}

// called every time before scene updates
// here we should setup parameters, which should be done for both situations: create scene from scratch or update the scene
// if return Abort, then recreate the scene from scratch
XSI::CStatus RenderEngineCyc::pre_scene_process()
{
	is_recreate_session = false;
	create_new_scene = false;
	is_update_camera = false;
	if (!is_session)
	{
		// if there are no session object, create it
		// if we change display channel, then session already removed
		is_recreate_session = true;
	}

	// create baking context at the start
	// the scene will be create from scratch, this is the rule of the base implementation
	baking_context->reset();
	series_context->reset();

	if (render_type == RenderType::RenderType_Rendermap)
	{
		// in base implementation we get several parameters from default rendermap property
		// here we can upgrade it by using custom property and setup additional parameters for cycles rendering
		pre_bake();
	}
	else if(render_type == RenderType::RenderType_Pass)
	{
		series_context->activate();
	}

	update_context->set_current_render_parameters(m_render_parameters);
	update_context->set_image_size(image_full_size_width, image_full_size_height);
	update_context->set_camera(camera);
	update_context->set_time(eval_time);
	update_context->reset_need_update_background();

	// memorize current project path
	set_project_path();

	// we should setup output passes after parsing scene, because it may contains aovs and lightgroups
	output_context->set_render_type(render_type);
	output_context->set_output_size((ULONG)image_size_width, (ULONG)image_size_height);
	output_context->set_output_formats(output_paths, output_formats, output_data_types, output_channels, output_bits, eval_time);
	if (render_type == RenderType::RenderType_Pass)
	{
		output_context->set_cryptomatte_settings((bool)m_render_parameters.GetValue("output_crypto_object", eval_time),
			(bool)m_render_parameters.GetValue("output_crypto_material", eval_time),
			(bool)m_render_parameters.GetValue("output_crypto_asset", eval_time),
			(int)m_render_parameters.GetValue("output_crypto_levels", eval_time));
	}
	// actual passes will be setup after scene sync

	color_transform_context->update(m_render_parameters, eval_time);

	// if current dusplay channel is AOV and the previous was also aov with another name, then we should recreate the session and scene
	// in all other cases we can render other visual pass, but also should sync passes
	ccl::PassType visual_pass_type = channel_to_pass_type(m_display_channel_name);
	bool is_motion = m_render_parameters.GetValue("film_motion_use", eval_time);
	if (is_motion && visual_pass_type == ccl::PASS_MOTION)
	{
		// we can not show motion pass when motion blur is activated
		log_message("It's impossible to render Motion Pass with activated motion blur, output is invalid", XSI::siWarningMsg);
	}

	// m_display_channel_name contains general name of the cisual pass (Cycles AOV Color, for example)
	// here we should check actual visual pass (modified if it was aov)
	display_pass_name = channel_name_to_pass_name(m_render_parameters, m_display_channel_name, eval_time);
	if (visual_pass_type == ccl::PASS_AOV_COLOR || visual_pass_type == ccl::PASS_AOV_VALUE)
	{
		if (update_context->get_prev_display_pass_name() != display_pass_name)
		{
			is_recreate_session = true;
		}
	}

	// check is current session parameters coincide with the previous one
	session_params = get_session_params(render_type, m_render_parameters, eval_time);
	scene_params = get_scene_params(render_type, session_params, m_render_parameters, eval_time);

	if (is_session && !is_recreate_session)
	{
		// build it method does not catch samples, samples_offset parameters, time_limit change
		// temp_dir is also does not catch
		if (session->params.modified(session_params) || session->scene->params.modified(scene_params))
		{
			is_recreate_session = true;
		}

		if (!is_recreate_session)
		{
			// update samples values if we should use old session parameters
			// in other case we will use these actual parameters, where these values alredy correct
			if (render_type != RenderType_Shaderball)
			{
				set_session_samples(session->params, m_render_parameters, eval_time);
			}
		}
	}

	// if we should use previuos session, check may be we turn on motion blur
	// in this case we should recreate the scene from scratch
	// if we disable motion blur, then nothing to do
	in_update_motion_type = get_motion_type_from_parameters(m_render_parameters, eval_time, output_channels, m_display_channel_name);
	if (render_type == RenderType_Shaderball)
	{
		in_update_motion_type = MotionType_None;
	}
	if (!is_recreate_session)
	{
		if ((in_update_motion_type == MotionType_Blur || in_update_motion_type == MotionType_Pass) && update_context->get_motion_type() == MotionType_None)
		{
			is_recreate_session = true;
		}
	}

	// recreate the scene if we change denoising
	int denoise_mode = m_render_parameters.GetValue("denoise_mode", eval_time);
	bool use_denoising = denoise_mode != 0;
	if (!is_recreate_session)
	{
		if (update_context->get_use_denoising() != use_denoising)
		{
			is_recreate_session = true;
		}
	}
	update_context->set_use_denoising(use_denoising);

	// recreate scene if we change displacement settings
	int displacement_mode = render_type == RenderType_Shaderball ? get_shaderball_displacement_method() : (int)m_render_parameters.GetValue("options_displacement_method", eval_time);
	if (!is_recreate_session)
	{
		if (update_context->get_displacement_mode() != displacement_mode)
		{
			is_recreate_session = true;
		}
	}
	update_context->set_displacement_mode(displacement_mode);

	// set actual motoin type
	// this value used in integrator
	// even if we does not update other motion parameters but motion blur is disabled, then in will not be rendered
	update_context->set_motion_type(in_update_motion_type);
	update_context->set_motion_rolling(m_render_parameters.GetValue("film_motion_rolling_type", eval_time) == 1);
	update_context->set_motion_rolling_duration(m_render_parameters.GetValue("film_motion_rolling_duration", eval_time));

	// create temp folder
	// we will setup it to the session params later after scene is created
	// because this will be the common moment as for scene updates and creating from scratch
	bool use_tiles = m_render_parameters.GetValue("performance_memory_use_auto_tile", eval_time);
	if (use_tiles)
	{
		temp_path = create_temp_path();
	}
	else
	{
		temp_path == "";
	}
	

	// if recreate session, then automaticaly say that the scene is also new
	// this parameter used in post_scene, for example
	update_context->set_is_update_scene(is_recreate_session);

	if (is_recreate_session)
	{
		// so, we will create new session and new scene
		return XSI::CStatus::Abort;
	}
	else
	{
		// we will use old session and old scene
		return XSI::CStatus::OK;
	}
}

// return OK, if object successfully updates, Abort in other cases
XSI::CStatus RenderEngineCyc::update_scene(XSI::X3DObject& xsi_object, const UpdateType update_type)
{
	// NOTICE: change mesh geometry force recreate the scene
	// because the callback OnNestedObjectsChange is fired
	if (!is_session)
	{
		return XSI::CStatus::Abort;
	}

	// when change visibility of any object, recreate the scene
	// this is similar to create a new object or delete it
	if (update_type == UpdateType::UpdateType_Visibility)
	{
		return XSI::CStatus::Abort;
	}

	XSI::CStatus is_update = XSI::CStatus::OK;

	if (update_type == UpdateType::UpdateType_Camera)
	{
		is_update = sync_camera(session->scene, update_context);
		is_update_camera = true;
	}
	else if (update_type == UpdateType::UpdateType_GlobalAmbient)
	{
		is_update = update_background_color(session->scene, update_context);
	}
	else if (update_type == UpdateType::UpdateType_Transform)
	{
		is_update = update_transform(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_XsiLight)
	{
		XSI::Light xsi_light(xsi_object);
		is_update = update_xsi_light(session->scene, update_context, xsi_light);
		if (is_update == XSI::CStatus::OK)
		{
			is_update = update_transform(session->scene, update_context, xsi_object);
		}
	}
	else if (update_type == UpdateType::UpdateType_LightPrimitive)
	{
		is_update = update_custom_light(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_Hair)
	{
		is_update = update_hair(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_Mesh)
	{
		is_update = update_polymesh(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_Pointcloud)
	{
		PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
		if (pointcloud_type == PointcloudType::PointcloudType_Strands)
		{
			is_update = update_strands(session->scene, update_context, xsi_object);
		}
		else if (pointcloud_type == PointcloudType::PointcloudType_Points)
		{
			is_update = update_points(session->scene, update_context, xsi_object);
		}
		else if (pointcloud_type == PointcloudType::PointcloudType_Volume)
		{
			is_update = update_volume(session->scene, update_context, xsi_object);
		}
		else if (pointcloud_type == PointcloudType::PointcloudType_Instances)
		{
			return XSI::CStatus::Abort;
		}
		else
		{

		}
	}
	else if (update_type == UpdateType::UpdateType_VDBPrimitive)
	{
		is_update = update_vdb(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_MeshProperty)
	{
		// we can change subdivide parameters, in this case we should recreate the mesh
		// in all other cases we should simply update object properties
		is_update = update_polymesh(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_HairProperty)
	{
		// update only object properties
		is_update = update_hair_property(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType::UpdateType_PointcloudProperty)
	{
		PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
		if (pointcloud_type == PointcloudType::PointcloudType_Strands)
		{
			// we can change tip parameter, so, recreate the strands from scratch
			is_update = update_strands(session->scene, update_context, xsi_object);
		}
		else if (pointcloud_type == PointcloudType::PointcloudType_Points || pointcloud_type == PointcloudType::PointcloudType_Volume)
		{
			// if we change property for points, then simply update object properies
			// even if we activate or deactivate mmotion blur
			// for actual changes user should force update the scene
			is_update = update_points_property(session->scene, update_context, xsi_object);
		}
		else if (pointcloud_type == PointcloudType::PointcloudType_Instances)
		{
			return XSI::CStatus::Abort;
		}
	}
	else if (update_type == UpdateType::UpdateType_VolumeProperty)
	{
		is_update = update_volume_property(session->scene, update_context, xsi_object);
	}

	update_context->set_is_update_scene(true);

	if (is_update != XSI::CStatus::OK)
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

// update non-geometry object (pass, for example)
XSI::CStatus RenderEngineCyc::update_scene(XSI::SIObject& si_object, const UpdateType update_type)
{
	if (!is_session)
	{
		return XSI::CStatus::Abort;
	}

	update_context->set_is_update_scene(true);

	return XSI::CStatus::OK;
}

// update material
XSI::CStatus RenderEngineCyc::update_scene(XSI::Material& xsi_material, bool material_assigning)
{
	if (!is_session)
	{
		return XSI::CStatus::Abort;
	}

	XSI::CStatus is_update = XSI::CStatus::OK;

	ULONG material_id = xsi_material.GetObjectID();
	if (update_context->is_material_exists(material_id))
	{
		std::vector<XSI::CStringArray> aovs(2);
		aovs[0].Clear();
		aovs[1].Clear();
		is_update = update_material(session->scene, xsi_material, update_context->get_xsi_material_cycles_index(material_id), update_context->get_time(), aovs);

		if (update_context->is_displacement_material(material_id))
		{
			// get all objects, used by this material
			XSI::CRefArray used_objects = xsi_material.GetUsedBy();
			ULONG used_count = used_objects.GetCount();
			for (ULONG i = 0; i < used_count; i++)
			{
				XSI::CRef obj(used_objects[i]);
				if (obj.IsValid())
				{
					XSI::siClassID obj_class = obj.GetClassID();
					if (obj_class == XSI::siClassID::siX3DObjectID)
					{
						XSI::X3DObject xsi_obj(obj);
						XSI::CString xsi_type = xsi_obj.GetType();
						// use general update_scene function
						if (xsi_type == "polymsh")
						{
							is_update = update_scene(xsi_obj, UpdateType::UpdateType_Mesh);
						}
						else if (xsi_type == "hair")
						{
							is_update = update_scene(xsi_obj, UpdateType::UpdateType_Hair);
						}
						else if (xsi_type == "pointcloud")
						{
							is_update = update_scene(xsi_obj, UpdateType::UpdateType_Pointcloud);
						}
					}
					else
					{
						
					}
				}
			}
		}

		update_context->add_aov_names(aovs[0], aovs[1]);
	}
	else
	{
		// may be we in the shaderball render mode, and update not material but subshader
		if (render_type == RenderType_Shaderball)
		{
			ULONG shader_id = update_context->get_shaderball_material_node(material_id);
			if (update_context->is_material_exists(shader_id))
			{
				is_update = update_shaderball_shadernode(session->scene, shader_id, m_shaderball_type, update_context->get_xsi_material_cycles_index(shader_id), update_context->get_time());
			}
		}
	}

	update_context->set_is_update_scene(true);

	if (is_update != XSI::CStatus::OK)
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

// update render parameters
XSI::CStatus RenderEngineCyc::update_scene_render()
{
	if (!is_session)
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}

// here we create the scene for rendering from scratch
XSI::CStatus RenderEngineCyc::create_scene()
{
	clear_session();
	session = create_session(session_params, scene_params);

	is_session = true;
	create_new_scene = true;

	// reset updater and prepare to store actual data
	update_context->reset();  // in particular set is_update_scene = true and use_background_light = false

	// after reset we should set render type again
	update_context->set_render_type(render_type);

	// get all motion properties
	update_context->set_motion(m_render_parameters, output_channels, m_display_channel_name, in_update_motion_type);

	// denoising
	update_context->set_use_denoising((int)m_render_parameters.GetValue("denoise_mode", eval_time) != 0);

	// setup displacement mode after reset
	update_context->set_displacement_mode(render_type == RenderType_Shaderball ? get_shaderball_displacement_method() : (int)m_render_parameters.GetValue("options_displacement_method", eval_time));

	if (render_type == RenderType_Shaderball)
	{
		sync_shaderball_scene(session->scene, update_context, m_scene_list, m_shaderball_material, m_shaderball_type, m_shaderball_material_id);
	}
	else
	{
		sync_scene(session->scene, update_context, m_isolation_list, m_lights_list, XSI::Application().FindObjects(XSI::siX3DObjectID), XSI::Application().FindObjects(XSI::siModelID));
		if (render_type == RenderType::RenderType_Rendermap && baking_context->get_is_valid())
		{
			sync_baking(session->scene, update_context, baking_context, baking_object, baking_uv, image_full_size_width, image_full_size_height);
		}
	}
	is_update_camera = true;

	// setup callbacks
	// output driver calls every in the same time as display driver
	// so, we can try to use only output driver, because it allows to extract pixels for different passes
	session->set_output_driver(std::make_unique<XSIOutputDriver>(this));
	session->progress.set_update_callback(function_bind(&RenderEngineCyc::progress_update_callback, this));
	session->progress.set_cancel_callback(function_bind(&RenderEngineCyc::progress_cancel_callback, this));

	return XSI::CStatus::OK;
}

// call this method after scene created or updated but before unlock
// if return Abort, then the engine will recreate the scene
XSI::CStatus RenderEngineCyc::post_scene()
{
	call_abort_render = false;
	make_render = true;

	// setup labels
	labels_context->setup(session->scene, m_render_parameters, camera, eval_time);

	// check, should we start the render, or we can use previous render result
	std::unordered_set<std::string> changed_render_parameters = update_context->get_changed_parameters(m_render_parameters);

	if (!create_new_scene)
	{
		if (update_context->is_change_render_parameters_motion(changed_render_parameters))
		{
			return XSI::CStatus::Abort;
		}

		if (update_context->is_change_render_parameters_background(changed_render_parameters))
		{
			update_background_parameters(session->scene, update_context);
		}
	}

	if (update_context->is_need_update_background())
	{
		update_background(session->scene, update_context);
	}

	if (!is_update_camera && update_context->is_change_render_parameters_camera(changed_render_parameters))
	{
		sync_camera(session->scene, update_context);
		is_update_camera = true;
	}

	// initialize visual buffer at the very end
	// may be we should not render at all, in this case we should re-use previous visual buffer
	if (render_type == RenderType_Region && !update_context->get_is_update_scene() && update_context->is_changed_render_parameters_only_cm(changed_render_parameters))
	{// we make preview render, scene is not changed, only color management render parameter is changed
		// if current parameters for visual buffer is not changed, then disable the rendering
		if (visual_buffer->is_coincide((ULONG)image_full_size_width,
			(ULONG)image_full_size_height,
			(ULONG)image_corner_x,
			(ULONG)image_corner_y,
			(ULONG)image_size_width,
			(ULONG)image_size_height,
			display_pass_name,
			m_render_parameters, eval_time))
		{
			make_render = false;
		}
	}

	if(make_render)
	{
		visual_buffer->setup((ULONG)image_full_size_width,
			(ULONG)image_full_size_height,
			(ULONG)image_corner_x,
			(ULONG)image_corner_y,
			(ULONG)image_size_width,
			(ULONG)image_size_height,
			m_display_channel_name,
			display_pass_name,
			m_render_parameters, eval_time);

		// at the end sync passes (also set crypto passes for film and aproximate shadow catcher)
		sync_passes(session->scene, update_context, output_context, series_context, baking_context, visual_buffer);
		series_context->set_common_path(output_context);

		if (update_context->is_changed_render_paramters_film(changed_render_parameters))
		{
			// here we aupdate film and mist (but not transparent and motion)
			sync_film(session, update_context, m_render_parameters);
		}

		if (update_context->is_changed_render_paramters_integrator(changed_render_parameters))
		{
			sync_integrator(session, update_context, baking_context, m_render_parameters, render_type, get_input_config());
		}

		if (render_type == RenderType_Shaderball || update_context->is_change_render_parameters_shaders(changed_render_parameters))
		{
			sync_shader_settings(session->scene, m_render_parameters, render_type, get_shaderball_displacement_method(), eval_time);
		}

		// set temp directory for session parameters
		if (session->params.use_auto_tile && temp_path.Length() > 0)
		{
			session->params.temp_dir = temp_path.GetAsciiString();
		}
		else
		{
			if (session->params.use_auto_tile)
			{
				log_message("Render use tiles mode, but temp directory is not defined. Disabled this mode.", XSI::siWarningMsg);
			}
			session->params.use_auto_tile = false;
		}
		
		update_context->set_logging(m_render_parameters.GetValue("options_logging_log_rendertime", eval_time), m_render_parameters.GetValue("options_logging_log_details", eval_time));
	}

	int update_method = m_render_parameters.GetValue("options_update_method", eval_time);
	if (update_method == 0)
	{// Always Abort
		activate_force_recreate_scene();
	}

	// save values of used render parameters
	update_context->setup_prev_render_parameters(m_render_parameters);
	// and rendered visual channel
	update_context->set_prev_display_pass_name(display_pass_name);

	return XSI::CStatus::OK;
}

void RenderEngineCyc::render()
{
	if (make_render)
	{
		rendered_samples = 0;
		ccl::BufferParams buffer_params = get_buffer_params(image_full_size_width, image_full_size_height, image_corner_x, image_corner_y, image_size_width, image_size_height);
		session->reset(session->params, buffer_params);
		session->scene->enable_update_stats();

		session->progress.reset();
		session->stats.mem_peak = session->stats.mem_used;

		session->start();
		session->wait();
	}
}

XSI::CStatus RenderEngineCyc::post_render_engine()
{
	// get render time
	// here we count only actual (in Cycles) render time, without prepare stage
	double render_time = (finish_render_time - start_render_time) / CLOCKS_PER_SEC;
	if (make_render)
	{
		labels_context->set_render_time(render_time);
		// when the render has the time limit, then return 0 samples, try to obtain actual samples count
		int current_samples = session->progress.get_current_sample();
		labels_context->set_render_samples(current_samples == 0 ? rendered_samples : session->progress.get_current_sample());
		labels_context->set_render_triangles(session->scene);
	}

	// the render is done, add labels to the output (if we need it)
	output_context->set_labels_buffer(labels_context);

	// make final color correction, add labels over visual buffer and output to the screen
	postrender_visual_output();

	// save outputs only for pass and baking rendering
	if (render_type == RenderType_Pass || render_type == RenderType_Rendermap)
	{
		write_outputs(output_context, color_transform_context, m_render_parameters);
	}

	//log render time
	if (render_type != RenderType_Shaderball && make_render && render_time > 0.00001)
	{
		if (update_context->get_is_log_rendertime())
		{
			log_message("Render time: " + XSI::CString(render_time) + " seconds");
		}
		if (update_context->get_is_log_details())
		{
			// otuput scene prepare time
			double prepare_time = (start_render_time - start_prepare_render_time) / CLOCKS_PER_SEC;
			log_message("Scene export time: " + XSI::CString(prepare_time) + " seconds");

			ccl::RenderStats stats;
			session->collect_statistics(&stats);
			log_message(stats.full_report().c_str());
		}
	}

	// remove temp directory (if it exists)
	remove_temp_path(temp_path);

	// clear output context object
	output_context->reset();

	// reset baking, and also clear the scene
	// next render will be from scratch
	// WARNING: there is non-critical bag here
	// if we use baking, then it does not properly unload addon dll from the memory when we manually request it
	// but after close Softimage, it unload it properly
	bool is_clear = baking_context->get_is_valid();
	if (is_clear)
	{
		clear_session();
	}
	baking_context->reset();


	return XSI::CStatus::OK;
}

void RenderEngineCyc::abort()
{
	if (is_session)
	{
		session->progress.set_cancel("Abort render");
		call_abort_render = true;
	}
}

void RenderEngineCyc::clear_engine()
{
	clear_session();
}