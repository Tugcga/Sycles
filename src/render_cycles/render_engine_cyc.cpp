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
#include "../utilities/logs.h"
#include "../output/write_image.h"
#include "../utilities/arrays.h"
#include "../utilities/files_io.h"

using namespace std::chrono_literals;

RenderEngineCyc::RenderEngineCyc()
{
	is_session = false;
	call_abort_render = false;
	output_context = new OutputContext();
	labels_context = new LabelsContext();
	color_transform_context = new ColorTransformContext();
	update_context = new UpdateContext();

	make_render = true;
	is_recreate_session = true;
	display_pass_name = "";
	create_new_scene = true;
	is_update_camera = false;
}

// when we delete the engine, then at first this method is called, and then the method from base class
RenderEngineCyc::~RenderEngineCyc()
{
	// TODO: there is a bug in Cycles (or OIIO)
	// when use texture image, then it does not properly unload and the scene can not unload from memory
	// this leads to error after close Softimage
	// may be updates will resolve this
	clear_session();
	delete output_context;
	delete labels_context;
	delete color_transform_context;
	delete update_context;
}

void RenderEngineCyc::path_init(const XSI::CString& plugin_path)
{
	XSI::CString folder_path = XSI::CUtils::BuildPath(plugin_path, "..", "..");
	ccl::path_init(folder_path.GetAsciiString());
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

// this method calls from output driver when nex tile is come
// we should read pixels for all passes and save it into output array
void RenderEngineCyc::update_render_tile(const ccl::OutputDriver::Tile& tile)
{
	// read tile size and offset
	unsigned int tile_width = tile.size.x;
	unsigned int tile_height = tile.size.y;
	unsigned int offset_x = tile.offset.x;  // this parameter defines offset inside the render buffer, not on the whole screen
	unsigned int offset_y = tile.offset.y;  // so, we should add it to image_corner to obtain actual in-screen offset
	OIIO::ROI tile_roi = OIIO::ROI(offset_x, offset_x + tile_width, offset_y, offset_y + tile_height);

	// create pixel buffer
	// we will use it for all passes
	std::vector<float> pixels((size_t)tile_width * tile_height * 4, 0.0f);

	// we should get from tile pixels for each pass and save pixels into buffers (visual or output)
	// get at first pixels for visual
	int visual_components = visual_buffer->get_components();
	bool is_get = tile.get_pass_pixels(visual_buffer->get_pass_name(), visual_components, &pixels[0]);
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

	// and next for each output pass
	for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
	{
		ccl::PassType pass_type = output_context->get_output_pass_type(i);
		ccl::ustring pass_name = output_context->get_output_pass_name(i);
		int pass_components = get_pass_components(pass_type, pass_type == ccl::PASS_COMBINED && pass_name.size() >= 9);  // because lightgroup pass has the name Combined_... (length >= 9)
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
	output_context->set_cryptomatte_settings((bool)m_render_parameters.GetValue("output_crypto_object", eval_time),
		(bool)m_render_parameters.GetValue("output_crypto_material", eval_time),
		(bool)m_render_parameters.GetValue("output_crypto_asset", eval_time),
		(int)m_render_parameters.GetValue("output_crypto_levels", eval_time));
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

	// m_display_channel_name contains general name of the cisual pass (Sycles AOV Color, for example)
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

	// set actual motoin type
	// this value used in integrator
	// even if we does not update other motion parameters but motion blur is disabled, then in will not be rendered
	update_context->set_motion_type(in_update_motion_type);
	update_context->set_motion_rolling(m_render_parameters.GetValue("film_motion_rolling_type", eval_time) == 1);
	update_context->set_motion_rolling_duration(m_render_parameters.GetValue("film_motion_rolling_duration", eval_time));

	// create temp folder
	// we will setup it to the session params later after scene is created
	// because this will be the common moment as for scene updates and creating from scratch
	// TODO: there is a bug somewhere, even if we setup the temp directory into session parameters, it crashes when render small tiles
	// so, try to fix it later, for now simply disable tiling
	// temp_path = create_temp_path();

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
	log_message("update x3dobject " + xsi_object.GetFullName() + " " + XSI::CString(update_type));
	if (!is_session)
	{
		return XSI::CStatus::Abort;
	}

	// when change visibility of any object, recreate the scene
	// this is similar to create a new object or delete it
	if (update_type == UpdateType_Visibility)
	{
		return XSI::CStatus::Abort;
	}

	XSI::CStatus is_update = XSI::CStatus::OK;

	if (update_type == UpdateType_Camera)
	{
		is_update = sync_camera(session->scene, update_context);
		is_update_camera = true;
	}
	else if (update_type == UpdateType_GlobalAmbient)
	{
		is_update = update_background_color(session->scene, update_context);
	}
	else if (update_type == UpdateType_Transform)
	{
		is_update = update_transform(session->scene, update_context, xsi_object);
	}
	else if (update_type == UpdateType_XsiLight)
	{
		XSI::Light xsi_light(xsi_object);
		is_update = update_xsi_light(session->scene, update_context, xsi_light);
		if (is_update == XSI::CStatus::OK)
		{
			is_update = update_transform(session->scene, update_context, xsi_object);
		}
	}
	else if (update_type == UpdateType_LightPrimitive)
	{
		is_update = update_custom_light(session->scene, update_context, xsi_object);
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

		update_context->add_aov_names(aovs[0], aovs[1]);
	}
	else
	{
		// may be we in the shaderball render mode, but update not material but subshader
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
	log_message("---create session---");
	clear_session();

	session = create_session(session_params, scene_params);
	is_session = true;
	create_new_scene = true;

	// reset updater and prepare to store actual data
	update_context->reset();  // in particular set is_update_scene = true

	// after reset we should set render type again
	update_context->set_render_type(render_type);

	// get all motion properties
	update_context->set_motion(m_render_parameters, output_channels, m_display_channel_name, in_update_motion_type);

	update_context->setup_scene_objects(m_isolation_list, m_lights_list, m_scene_list, XSI::Application().FindObjects(XSI::siX3DObjectID));
	sync_scene(session->scene, update_context, m_render_parameters, m_shaderball_material, m_shaderball_type, m_shaderball_material_id);
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
			update_background_parameters(session->scene, update_context, m_render_parameters);
		}
	}

	if (update_context->is_need_update_background())
	{
		update_background(session->scene, update_context, m_render_parameters);
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
		// RenderVisualBuffer store pixels in OIIO::ImageBuf
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
		sync_passes(session->scene, output_context, visual_buffer, update_context->get_motion_type(), update_context->get_lightgropus(), update_context->get_color_aovs(), update_context->get_value_aovs());

		if (update_context->is_changed_render_paramters_film(changed_render_parameters))
		{
			// here we aupdate film and mist (but not transparent and motion)
			sync_film(session, update_context, m_render_parameters);
		}

		if (update_context->is_changed_render_paramters_integrator(changed_render_parameters))
		{
			sync_integrator(session, update_context, m_render_parameters, render_type, get_input_config());
		}

		if (render_type == RenderType_Shaderball || update_context->is_change_render_parameters_shaders(changed_render_parameters))
		{
			sync_shader_settings(session->scene, m_render_parameters, render_type, get_shaderball_displacement_method(), eval_time);
		}

		// qick fix for displacement problem
		// with active displacement, vertices of the mesh is shifted with respect to displacement map
		// and at the next update it shift these vertices again
		int settings_displacement_type = render_type == RenderType_Shaderball ? get_shaderball_displacement_method() : (int)m_render_parameters.GetValue("options_displacement_method", eval_time);
		if (settings_displacement_type != 0)
		{// 0 - bump displacement
			bool is_find_displacement = find_scene_shaders_displacement(session->scene);
			if (is_find_displacement)
			{
				// a the next update the scene will be recreated from scratch
				activate_force_recreate_scene();
			}
		}

		// TODO: try to fix this bug
		// set temp directory for session parameters
		// session->params.temp_dir = temp_path.GetAsciiString();

		update_context->set_logging(m_render_parameters.GetValue("options_logging_log_rendertime", eval_time), m_render_parameters.GetValue("options_logging_log_details", eval_time));
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
	double render_time = (finish_render_time - start_prepare_render_time) / CLOCKS_PER_SEC;
	if (make_render)
	{
		labels_context->set_render_time(render_time);
		// this value is coorect only when the render use one tile
		// now we always use one-tile render (withou tiles with small size)
		// TODO: when the render has the time limit, then return 0 samples, try to obtain actual samples count
		labels_context->set_render_samples(session->progress.get_current_sample());
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
	if (render_type != RenderType_Shaderball && make_render)
	{
		if (update_context->get_is_log_rendertime())
		{
			log_message("Render statistics: " + XSI::CString(render_time) + " seconds");
		}
		if (update_context->get_is_log_details())
		{
			ccl::RenderStats stats;
			session->collect_statistics(&stats);
			log_message(stats.full_report().c_str());
		}
	}

	// TODO: fix crash when using tiling rendering
	// remove_temp_path(temp_path);

	// clear output context object
	output_context->reset();

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