#pragma once
#include "render_visual_buffer.h"
#include "render_tile.h"

#include <xsi_renderercontext.h>
#include <xsi_x3dobject.h>
#include <xsi_material.h>
#include <xsi_ppgeventcontext.h>
#include <xsi_camera.h>

#include <vector>
#include <string>

#include "type_enums.h"

class RenderEngineBase
{
public:
	//these public variables can be used from render engine
	RenderType render_type;
	XSI::Camera camera;
	XSI::RendererContext m_render_context;  // save it in the pre_render method
	XSI::CRefArray m_isolation_list;
	XSI::CRefArray m_lights_list;
	XSI::CRefArray m_scene_list;
	XSI::CRef m_shaderball_material;
	ShaderballType m_shaderball_type;
	ULONG m_shaderball_material_id;
	XSI::CString archive_folder;  // full path to the folder for archive export. Non-empty only if the user call export scene
	XSI::CString m_display_channel_name;  // name of the channel for output to the screen (selected channel for the preview render)
	XSI::Property m_render_property;
	XSI::CParameterRefArray m_render_parameters;
	XSI::CTime eval_time;
	RenderVisualBuffer* visual_buffer;
	//output data arrays
	XSI::CStringArray output_paths;  // full path to output image
	XSI::CStringArray output_formats;  // jpg, exr and so on (without points, in fact as was defined in init callback)
	XSI::CStringArray output_data_types;  // RGB, RBGA, XYZ and so on
	XSI::CStringArray output_channels;  // the names of the selected render channels
	std::vector<int> output_bits;  // Integer1 = 0, Integer2 = 1, Integer4 = 2, Integer8 = 3, Integer16 = 4, Integer32 = 5, Float16 = 20, Float32 = 21

	int image_size_width;
	int image_size_height;
	int image_corner_x;
	int image_corner_y;
	int image_full_size_width;
	int image_full_size_height;

	XSI::X3DObject baking_object;
	XSI::CString baking_uv;  // the name of uv for baking

	double start_prepare_render_time;
	double start_render_time;
	double finish_render_time;

	RenderEngineBase();
	~RenderEngineBase();

	//-----------------------------------------------------------
	//this virtual methods should be implemented in the render engine
	//setup render parameters
	virtual XSI::CStatus render_option_define(XSI::CustomProperty &property);

	//setup PPG UI of render settings
	virtual XSI::CStatus render_options_update(XSI::PPGEventContext &event_context);

	//behaviour of the UI when parameters are changed
	virtual XSI::CStatus render_option_define_layout(XSI::Context &context);
	
	//next method calls when Softimage start to render the frame
	//step 1: pre_render_engine called before Softimage lock the scene to get full list of the rendering data
	virtual XSI::CStatus pre_render_engine();

	//step 2: pre_scene_process called before actual scene update
	//in this method we can setup general parameters, which does not differ for new or updated scene
	//if this method return Abort, then we should drop update process and recreate the scene from scratch
	virtual XSI::CStatus pre_scene_process();  
	
	//step 3: call this method on the engine when we need to create the scene from scratch
	virtual XSI::CStatus create_scene();

	//setp 3: or, if the scene should be updated, then the following method calls for different changed aspects of the scene
	//here we update 3d-object
	//update methods return OK, if update success and Abort if update fails, in the last case we will start rectreate the scene
	virtual XSI::CStatus update_scene(XSI::X3DObject &xsi_object, const UpdateType update_type); 
	//here we update material (change any shader parameter)
	//if material_assigning = true, then we assign material to an object
	//in this case xsi_material is not material from library, but local subobject material (it has different id and empty library)
	virtual XSI::CStatus update_scene(XSI::Material &xsi_material, bool material_assigning);
	//this can be called when pass is updated
	virtual XSI::CStatus update_scene(XSI::SIObject &si_object, const UpdateType update_type);
	//this method called when we change render settings
	virtual XSI::CStatus update_scene_render();
	
	//call after all scene create or update but before unlock
	virtual XSI::CStatus post_scene();

	//after these methods Softimage unlock the scene

	//step 4: actual command to render
	virtual void render();
	
	//setp 5: call after render is complete
	//this method called after we save output images in the base class
	//in the engine we can calculate something statistical data
	virtual XSI::CStatus post_render_engine();

	//in this method we send the signal to the engine to stop the render process
	virtual void abort();

	//in clear() method we should clear all internal datas, which can be stored between render sessions
	//this called when, for example, new scene is created, or we close the Softimage
	virtual void clear_engine();
	
	//-----------------------------------------------------------
	//-----------------------External base methods---------------
	//these functions shouldn't be called from the engine, all of them are called from Softimage side
	//return true, if the engine is not active
	bool is_ready_to_render() { return ready_to_render; }

	void set_render_options_name(const XSI::CString &options_name);

	//call before we start prepare the scene
	XSI::CStatus pre_render(XSI::RendererContext &render_context);
	
	//call after successfull render process, here we can save output images and so on
	XSI::CStatus post_render();

	//in this method we should decide update scene or recreate it from scratch and call corresponding virtual methods
	XSI::CStatus scene_process();

	//call events and start actual render process
	XSI::CStatus start_render();

	//there are some errors in the scene process, so, does not start the render process
	XSI::CStatus interrupt_update_scene();
	void abort_render();

	void clear();

	void activate_force_recreate_scene(const XSI::CString& message = XSI::CString());

	//call these methods when we change scene and force to recreate the scene from scratch
	void on_object_add(XSI::CRef& in_ctxt);
	void on_object_remove(XSI::CRef& in_ctxt);
	void on_nested_objects_changed(XSI::CRef& in_ctx);

private:
	XSI::CString render_options_name;  // used for update process, to detect that we change parameters of the quick view render settings
	bool ready_to_render;
	bool force_recreate_scene;
	bool note_abort;  // set true when call abort_render method, set float at the start of scene_process after we lock the scene
	XSI::CRefArray prev_isolated_objects;
	RenderType prev_render_type;
	
	bool is_recreate_isolated_view(const XSI::CRefArray &visible_objects);
};
