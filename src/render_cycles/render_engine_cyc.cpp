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

using namespace std::chrono_literals;

RenderEngineCyc::RenderEngineCyc()
{
	is_session = false;
	call_abort_render = false;
	output_context = new OutputContext();
	labels_context = new LabelsContext();
}

// when we delete the engine, then at first this method is called, and then the method from base class
RenderEngineCyc::~RenderEngineCyc()
{
	clear_session();
	delete output_context;
	delete labels_context;
}

void RenderEngineCyc::clear_session()
{
	if (is_session)
	{
		session->cancel(true);
		delete session;
		session = NULL;
		is_session = false;
	}
}

// nothing to do here
XSI::CStatus RenderEngineCyc::pre_render_engine()
{
	return XSI::CStatus::OK;
}

// called every time before scene updates
// here we should setup parameters, which should be done for both situations: create scene from scratch or update the scene
XSI::CStatus RenderEngineCyc::pre_scene_process()
{
	//get visual pass
	XSI::Framebuffer frame_buffer = m_render_context.GetDisplayFramebuffer();
	XSI::RenderChannel render_channel = frame_buffer.GetRenderChannel();
	
	// we should setup output passes after parsing scene, because it may contains aovs and lightgroups
	// so, setup visual here, but output passes in sync_session
	output_context->set_render_type(render_type);
	output_context->set_visual_pass(render_channel.GetName());
	output_context->set_output_size((ULONG)image_size_width, (ULONG)image_size_height);
	output_context->set_output_formats(output_paths, output_formats, output_data_types, output_channels, output_bits, eval_time);

	// RenderVisualBuffer store pixels in OIIO::ImageBuf
	visual_buffer->create((ULONG)image_full_size_width,
		(ULONG)image_full_size_height,
		(ULONG)image_corner_x,
		(ULONG)image_corner_y,
		(ULONG)image_size_width,
		(ULONG)image_size_height,
		output_context->get_visual_pass_components());

	return XSI::CStatus::OK;
}

// return OK, if object successfully updates, Abort in other cases
XSI::CStatus RenderEngineCyc::update_scene(XSI::X3DObject& xsi_object, const UpdateType update_type)
{
	return XSI::CStatus::Abort;

}

// update non-geometry object (pass, for example)
XSI::CStatus RenderEngineCyc::update_scene(XSI::SIObject& si_object, const UpdateType update_type)
{
	return XSI::CStatus::Abort;
}

// update material
XSI::CStatus RenderEngineCyc::update_scene(XSI::Material& xsi_material, bool material_assigning)
{
	return XSI::CStatus::Abort;
	
}

// update render parameters
XSI::CStatus RenderEngineCyc::update_scene_render()
{
	return XSI::CStatus::OK;
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
	int visual_components = output_context->get_visual_pass_components();
	bool is_get = tile.get_pass_pixels(output_context->get_visual_pass_name(), visual_components, &pixels[0]);
	if (is_get)
	{
		visual_buffer->add_pixels(tile_roi, pixels);

		// next we should create new fragment to visualize it at the screen
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

void RenderEngineCyc::combine_labels_over_visual()
{
	if (output_context->get_is_labels() && output_context->get_visal_pass_type() == ccl::PASS_COMBINED)
	{
		ULONG visual_width = visual_buffer->get_width();
		ULONG visual_height = visual_buffer->get_height();
		if (visual_width == output_context->get_width() && visual_height == output_context->get_height())
		{
			// here we shold use the copy of the visual buffer pixels, because original pixels may be used later in the update
			std::vector<float> visual_pixels = visual_buffer->get_buffer_pixels();
			overlay_pixels(visual_width, visual_height, output_context->get_labels_pixels(), &visual_pixels[0]);

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

// here we create the scene for rendering from scratch
XSI::CStatus RenderEngineCyc::create_scene()
{
	clear_session();
	session = create_session();
	is_session = true;

	sync_session(session, m_render_context, output_context);  // here we also sync the scene

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

	// setup labels
	labels_context->setup(session->scene, m_render_parameters, camera, eval_time);

	return XSI::CStatus::OK;
}

void RenderEngineCyc::render()
{
	ccl::BufferParams buffer_params = get_buffer_params(image_full_size_width, image_full_size_height, image_corner_x, image_corner_y, image_size_width, image_size_height);
	session->reset(session->params, buffer_params);

	session->progress.reset();
	session->stats.mem_peak = session->stats.mem_used;

	session->start();
	session->wait();
}

XSI::CStatus RenderEngineCyc::post_render_engine()
{
	// get render time
	double render_time = (finish_render_time - start_prepare_render_time) / CLOCKS_PER_SEC;
	labels_context->set_render_time(render_time);

	// the render is done, add labels to the output (if we need it)
	output_context->set_labels_buffer(labels_context);

	// add labels over visual buffer and output to the screen
	combine_labels_over_visual();

	// save outputs
	write_outputs(output_context, m_render_parameters);

	//log render time
	if (render_type != RenderType_Shaderball)
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