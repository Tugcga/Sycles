#include <chrono>
#include <thread>

#include "xsi_primitive.h"
#include "xsi_application.h"
#include "xsi_project.h"
#include "xsi_library.h"
#include "xsi_renderchannel.h"
#include "xsi_framebuffer.h"
#include "xsi_projectitem.h"

#include "render_engine_cyc.h"
#include "cyc_session/cyc_session.h"
#include "cyc_output/output_drivers.h"
#include "../utilities/logs.h"
#include "../output/write_image.h"
#include "../utilities/arrays.h"

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
}

// when we delete the engine, then at first this method is called, and then the method from base class
RenderEngineCyc::~RenderEngineCyc()
{
	clear_session();
	delete output_context;
	delete labels_context;
	delete color_transform_context;
	delete update_context;
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
		if (visual_buffer->get_pass_type() == ccl::PASS_COMBINED)
		{
			color_transform_context->apply(tile_width, tile_height, visual_components, &pixels[0]);
		}

		m_render_context.NewFragment(RenderTile(offset_x + image_corner_x, offset_y + image_corner_y, tile_width, tile_height, pixels, false, visual_components));

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
		int pass_components = get_pass_components(pass_type);
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
	if (visual_buffer->get_pass_type() == ccl::PASS_COMBINED)
	{
		ULONG visual_width = visual_buffer->get_width();
		ULONG visual_height = visual_buffer->get_height();
		if (visual_width == visual_buffer->get_width() && visual_height == visual_buffer->get_height())
		{
			// here we shold use the copy of the visual buffer pixels, because original pixels may be used later in the update
			std::vector<float> visual_pixels = visual_buffer->get_buffer_pixels();

			// make color correction
			if (color_transform_context->get_use_correction())
			{
				color_transform_context->apply(visual_width, visual_height, 4, &visual_pixels[0]);
			}

			// add labels
			if (output_context->get_is_labels())
			{
				overlay_pixels(visual_width, visual_height, output_context->get_labels_pixels(), &visual_pixels[0]);
			}

			// set render fragment
			m_render_context.NewFragment(RenderTile(image_corner_x, image_corner_y, visual_width, visual_height, visual_pixels, false, 4));  // combined always have 4 components

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
	if (!is_session)
	{
		// if there are no session object, create it
		// if we change display channel, then session already removed
		is_recreate_session = true;
	}

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
	if (!is_recreate_session)
	{
		ccl::SessionParams new_session_params = get_session_params();
		ccl::SceneParams new_scene_params = get_scene_params();

		if (session->params.modified(new_session_params) || session->scene->params.modified(new_scene_params))
		{
			is_recreate_session = true;
		}
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
	if (update_type == UpdateType_Camera)
	{
		
	}

	update_context->set_is_update_scene(true);

	return XSI::CStatus::Abort;

}

// update non-geometry object (pass, for example)
XSI::CStatus RenderEngineCyc::update_scene(XSI::SIObject& si_object, const UpdateType update_type)
{
	update_context->set_is_update_scene(true);

	return XSI::CStatus::Abort;
}

// update material
XSI::CStatus RenderEngineCyc::update_scene(XSI::Material& xsi_material, bool material_assigning)
{
	update_context->set_is_update_scene(true);

	return XSI::CStatus::Abort;
}

// update render parameters
XSI::CStatus RenderEngineCyc::update_scene_render()
{
	return XSI::CStatus::OK;
}

// here we create the scene for rendering from scratch
XSI::CStatus RenderEngineCyc::create_scene()
{
	clear_session();
	session = create_session();
	is_session = true;

	// rest updater and prepare to store actual data
	update_context->reset();

	sync_session(session, m_render_context, output_context, visual_buffer);  // here we also sync the scene

	// setup callbacks
	// output driver calls every in the same time as display driver
	// so, we can try to use only output driver, because it allows to extract pixels for different passes
	session->set_output_driver(std::make_unique<XSIOutputDriver>(this));
	session->progress.set_update_callback(function_bind(&RenderEngineCyc::progress_update_callback, this));
	session->progress.set_cancel_callback(function_bind(&RenderEngineCyc::progress_cancel_callback, this));

	return XSI::CStatus::OK;
}

// call this method after scene created or updated but before unlock
XSI::CStatus RenderEngineCyc::post_scene()
{
	call_abort_render = false;
	make_render = true;

	// setup labels
	labels_context->setup(session->scene, m_render_parameters, camera, eval_time);

	// check, should we start the render, or we can use previous render result
	std::unordered_set<std::string> changed_render_parameters = update_context->get_changed_parameters(m_render_parameters);

	// initialize visual buffer at the very end
	// may be we should not render at all, in this case we should reuse previous visual buffer
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

		// at the end cync passes
		sync_passes(session->scene, output_context, visual_buffer);
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
		log_message("Render statistics: " + XSI::CString(render_time) + " seconds");
	}

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