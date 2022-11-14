#include <xsi_framebuffer.h>
#include <xsi_primitive.h>
#include <xsi_projectitem.h>
#include <xsi_shader.h>
#include <xsi_clusterproperty.h>

#include <ctime>

#include "render_engine_base.h"
#include "../utilities/logs.h"
#include "../si_callbacks/si_callbacks.h"
#include "../output/write_image.h"
#include "../utilities/arrays.h"

RenderEngineBase::RenderEngineBase()
{
	ready_to_render = true;
	force_recreate_scene = true;
	note_abort = true;
	prev_isolated_objects.Clear();
	prev_render_type = RenderType_Unknown;
}

RenderEngineBase::~RenderEngineBase()
{
	ready_to_render = false;
	force_recreate_scene = true;
	prev_isolated_objects.Clear();
	visual_buffer.clear();
}

void RenderEngineBase::set_render_options_name(const XSI::CString &options_name)
{
	XSI::CStringArray parts = options_name.Split(" ");
	render_options_name = XSI::CString(parts[0]);
	for (LONG i = 0; i < parts.GetCount(); i++)
	{
		render_options_name += "_" + parts[i];
	}
}

//next three mathods define ui behaviour
XSI::CStatus RenderEngineBase::render_options_update(XSI::PPGEventContext &event_context)
{
	log_message("[Base Render] Render options update is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::render_option_define_layout(XSI::Context &contextt)
{
	log_message("[Base Render] Layout define is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::render_option_define(XSI::CustomProperty &property)
{
	log_message("[Base Render] Render parameters definitions is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

//next methods should be implemented in render engine
XSI::CStatus RenderEngineBase::pre_render_engine()
{
	log_message("[Base Render] Pre-render method is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::pre_scene_process()
{
	log_message("[Base Render] Pre-scene method is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::create_scene()
{
	log_message("[Base Render] Create scene method is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::update_scene(XSI::X3DObject &xsi_object, const UpdateType update_type)
{
	log_message("[Base Render] Update scene for X3DObject is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::update_scene(XSI::SIObject &si_object, const UpdateType update_type)
{
	log_message("[Base Render] Update scene for SIObject is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::update_scene(XSI::Material &xsi_material, bool material_assigning)
{
	log_message("[Base Render] Update scene for material is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::update_scene_render()
{
	log_message("[Base Render] Update scene for render parameters is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::post_scene()
{
	log_message("[Base Render] Post update scene event is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineBase::post_render_engine()
{
	log_message("[Base Render] Post-render method is not implemented", XSI::siWarningMsg);
	return XSI::CStatus::OK;
}

void RenderEngineBase::abort()
{
	log_message("[Base Render] Abort render engine is not implemented", XSI::siWarningMsg);
}

void RenderEngineBase::clear_engine()
{
	log_message("[Base Render] Clear render engine is not implemented", XSI::siWarningMsg);
}

//main render method, where we start and update rendering process
void RenderEngineBase::render()
{
	//this is very basic render process
	//we fill the frame by randomly selected grey color
	//create random color
	float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	std::vector<float> frame_pixels(static_cast<size_t>(visual_buffer.width) * visual_buffer.height * 4);
	for (ULONG y = 0; y < visual_buffer.height; y++)
	{
		for (ULONG x = 0; x < visual_buffer.width; x++)
		{
			frame_pixels[4 * (static_cast<size_t>(visual_buffer.width) * y + x)] = r;
			frame_pixels[4 * (static_cast<size_t>(visual_buffer.width) * y + x) + 1] = r;
			frame_pixels[4 * (static_cast<size_t>(visual_buffer.width) * y + x) + 2] = r;
			frame_pixels[4 * (static_cast<size_t>(visual_buffer.width) * y + x) + 3] = 1.0;
		}
	}
	m_render_context.NewFragment(RenderTile(visual_buffer.corner_x, visual_buffer.corner_y, visual_buffer.width, visual_buffer.height, frame_pixels, false, 4));
	frame_pixels.clear();
	frame_pixels.shrink_to_fit();
}

//---------------------------------------------------------------------------------
//----------------------------------called from Softimage--------------------------

inline bool is_file_exists(const std::string& name)
{
	if (FILE *file = fopen(name.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	else
	{
		return false;
	}
}

XSI::CStatus RenderEngineBase::pre_render(XSI::RendererContext &render_context)
{
	//save render context
	m_render_context.ResetObject();
	m_render_context = render_context;
	m_render_property.ResetObject();
	m_render_property = m_render_context.GetRendererProperty(render_context.GetTime());
	m_render_parameters = m_render_property.GetParameters();
	//current time
	eval_time = render_context.GetTime();
	start_render_time = clock();

	//get pathes to save images
	output_paths.Clear();

	XSI::CString render_type_str = render_context.GetAttribute("RenderType");
	render_type = render_type_str == XSI::CString("Pass") ? RenderType_Pass :
		(render_type_str == XSI::CString("Region") ? RenderType_Region : (
			render_type_str == XSI::CString("Shaderball") ? RenderType_Shaderball : RenderType_Rendermap));

	archive_folder = render_context.GetAttribute("ArchiveFileName");
	if (render_type == RenderType_Pass && archive_folder.Length() > 0)
	{
		render_type = RenderType_Export;
	}

	//for RenderType_Pass and RenderType_Export we should get outputs from the render settings
	//but for RenderType_Rendermap we should get it from property of the object
	if (render_type == RenderType_Rendermap)
	{
		XSI::CRefArray rendermap_list = m_render_context.GetAttribute("RenderMapList");

		if (rendermap_list.GetCount() > 0)
		{
			XSI::Property rendermap_prop(rendermap_list[0]);  // get the first object
			XSI::CParameterRefArray params = rendermap_prop.GetParameters();

			//get the output path and set other output data
			output_paths.Add(params.GetValue("imagefilepath"));
			output_formats.Add(params.GetValue("imageformat"));  // format here can be unsupported on the renderer, so, it should be checked and replaced on the render implementation side
			output_data_types.Add(params.GetValue("imagedatatype"));
			output_channels.Add("Main");  // what actual channel we should use should be selected on the render implementation
			output_bits.push_back(params.GetValue("imagebitdepth"));
		}
		//if we call rendermap with empty objects list, then nothing to do
	}
	else
	{
		bool is_skip = m_render_context.GetAttribute("SkipExistingFiles");
		if (render_type != RenderType_Pass)
		{
			is_skip = false;  // ignore skip for export mode, for other modes this parameter does not used, because for other modes output pathes are empty
		}
		bool file_output = m_render_context.GetAttribute("FileOutput");


		XSI::CRefArray frame_buffers = render_context.GetFramebuffers();
		const LONG fb_count = frame_buffers.GetCount();
		for (LONG i = 0; i < fb_count; ++i)
		{
			XSI::Framebuffer fb(frame_buffers[i]);
			XSI::CValue enbable_val = fb.GetParameterValue("Enabled", eval_time.GetTime());
			if (file_output && enbable_val)
			{
				XSI::CString output_path = fb.GetResolvedPath(eval_time);
				//check is this file exists if is_skip is active
				if (is_skip && is_file_exists(output_path.GetAsciiString()))
				{
					//we shpuld skip this pass to render
					log_message("File " + output_path + " exists, skip it.");
				}
				else
				{
					XSI::CString fb_format = fb.GetParameterValue("Format");
					XSI::CString fb_data_type = fb.GetParameterValue("DataType");
					int fb_bits = fb.GetParameterValue("BitDepth");
					XSI::CString fb_channel_name = remove_digits(fb.GetName());

					if (fb_bits >= 0 && fb_data_type.Length() > 0 && fb_format.Length() > 0)
					{
						//add path to the render
						output_paths.Add(output_path);
						//save output data to array buffers
						output_formats.Add(fb_format);
						output_data_types.Add(fb_data_type);
						output_channels.Add(fb_channel_name);
						output_bits.push_back(fb_bits);
					}
				}
			}
		}

		if (render_type == RenderType_Pass && file_output && output_paths.GetCount() == 0)
		{//this is Pass render, but nothing to render
			return XSI::CStatus::Abort;
		}
	}

	return pre_render_engine();
}

XSI::CStatus RenderEngineBase::interrupt_update_scene()
{
	ready_to_render = true;
	return end_render_event(m_render_context, output_paths, true);
}

bool RenderEngineBase::is_recreate_isolated_view(const XSI::CRefArray &visible_objects)
{
	ULONG current_isolated = visible_objects.GetCount();
	ULONG prev_isolated = prev_isolated_objects.GetCount();
	if (current_isolated != prev_isolated)
	{//switch from one mode to other
		if (current_isolated == 0)
		{//now the mode is non-isolated
			prev_isolated_objects.Clear();
		}
		else
		{//the mode is isolated, save links to objects
			prev_isolated_objects.Clear();
			for (LONG i = 0; i < visible_objects.GetCount(); i++)
			{
				prev_isolated_objects.Add(visible_objects[i]);
			}
		}

		return true;
	}

	//mode is the same as in the previous render session
	if (current_isolated == 0)
	{// no isolation
		prev_isolated_objects.Clear();
		return false;
	}
	else
	{//isolation, and previous is also isolation
		bool is_recreate = false;
		for (LONG i = 0; i < visible_objects.GetCount(); i++)
		{
			if (!is_contains(prev_isolated_objects, visible_objects[i]))
			{
				i = visible_objects.GetCount();
				is_recreate = true;
			}
		}
		//recreate links
		prev_isolated_objects.Clear();
		for (LONG i = 0; i < visible_objects.GetCount(); i++)
		{
			prev_isolated_objects.Add(visible_objects[i]);
		}
		return is_recreate;
	}
}

XSI::CStatus RenderEngineBase::scene_process()
{
	ready_to_render = false;
	note_abort = false;

	if (render_type == RenderType_Rendermap)
	{
		//get map size from the property
		XSI::CRefArray rendermap_list = m_render_context.GetAttribute("RenderMapList");
		if (rendermap_list.GetCount() > 0)
		{
			XSI::Property rendermap_prop(rendermap_list[0]);
			XSI::CParameterRefArray params = rendermap_prop.GetParameters();

			image_full_size_width = params.GetValue("resolutionx");
			image_full_size_height = params.GetValue("resolutiony");
			if ((bool)params.GetValue("squaretex"))
			{
				image_full_size_height = image_full_size_width;
			}
			image_corner_x = 0;
			image_corner_y = 0;
			image_size_width = image_full_size_width;
			image_size_height = image_full_size_height;
		}
		else
		{
			image_full_size_width = 0;
			image_full_size_height = 0;
			image_corner_x = 0;
			image_corner_y = 0;
			image_size_width = 0;
			image_size_height = 0;
		}
	}
	else
	{
		//get render image
		image_full_size_width = m_render_context.GetAttribute("ImageWidth");
		image_full_size_height = m_render_context.GetAttribute("ImageHeight");
		image_corner_x = m_render_context.GetAttribute("CropLeft");
		image_corner_y = m_render_context.GetAttribute("CropBottom");
		image_size_width = m_render_context.GetAttribute("CropWidth");
		image_size_height = m_render_context.GetAttribute("CropHeight");
	}

	//setup visual buffer
	visual_buffer = RenderVisualBuffer(
		(ULONG)image_full_size_width,
		(ULONG)image_full_size_height,
		(ULONG)image_corner_x,
		(ULONG)image_corner_y,
		(ULONG)image_size_width,
		(ULONG)image_size_height,
		4);

	if (output_paths.GetCount() > 0)
	{//prepare output pixel buffers
		ULONG size = (LONG)image_size_width * image_size_height * 4 * output_paths.GetCount();
		output_pixels = std::vector<float>(size, 0.0f);
	}

	//memorize isolation list
	m_isolation_list = m_render_context.GetArrayAttribute("ObjectList");

	//next all other general staff for the engine
	XSI::CStatus status = pre_scene_process();
	if (render_type != prev_render_type)
	{
		//WARNING: in this case, when we have active material preview and render view, then it recreate the scene from scratch twise - for material render and scene render
		//and each update in recreate the scene, because it at first executes scene render, then switch context and update material render (with new geometry)
		//then again switch context to the scene and so on.
		//TODO: try to solve it
		//use different render instances for different subjects
		//now we have one render singleton and use it for all rendering tasks
		//it seems that Softimage use one render object (it share userdata) for different tasks
		status = XSI::CStatus::Abort;
	}
	prev_render_type = render_type;
	//if status is Abort, then drop update and recreate the scene

	//next we start update or rectreate the scene
	//we should recreate the scene when 
	//- force it
	//- with Pass render mode
	//- rendermap render mode
	//- empty dirty list
	//- recreate isolated view
	//- if we have undefined dirty list update

	XSI::CValue dirty_refs_value = m_render_context.GetAttribute("DirtyList");

	if (status != XSI::CStatus::OK ||
		force_recreate_scene || 
		render_type == RenderType_Pass || 
		render_type == RenderType_Rendermap ||
		render_type == RenderType_Export ||
		dirty_refs_value.IsEmpty())
	{//recreate the scene
		force_recreate_scene = false;
		create_scene();
	}
	else
	{
		//check isolated view
		XSI::CRefArray visible_objects = m_render_context.GetAttribute("ObjectList");
		//get object in the isolated view
		if (visible_objects.GetCount() > 0)
		{//add to objects in isolated view all lights
			XSI::CRefArray lights_array = m_render_context.GetAttribute("Lights");
			for (LONG i = 0; i < lights_array.GetCount(); i++)
			{
				XSI::CRef ref = lights_array.GetItem(i);
				if (!is_contains(visible_objects, ref))
				{
					visible_objects.Add(ref);
				}
			}
		}
		bool is_isolated_new = is_recreate_isolated_view(visible_objects);
		if (is_isolated_new)
		{
			//new isolation view, recreate the scene
			force_recreate_scene = false;
			create_scene();
		}
		else
		{
			//so, here we can update the scene
			//we should check all dirty objects
			XSI::Primitive camera_prim(m_render_context.GetAttribute("Camera"));
			XSI::X3DObject camera_obj = camera_prim.GetOwners()[0];

			XSI::CRefArray dirty_refs = dirty_refs_value;
			for (LONG i = 0; i < dirty_refs.GetCount(); i++)
			{
				XSI::CRef in_ref(dirty_refs[i]);
				XSI::SIObject xsi_obj = XSI::SIObject(in_ref);
				XSI::siClassID class_id = in_ref.GetClassID();
				XSI::CStatus update_status(XSI::CStatus::Undefined);
				switch (class_id)
				{
					case XSI::siStaticKinematicStateID:
					case XSI::siConstraintWithUpVectorID:
					case XSI::siKinematicStateID:
					{
						bool is_global = strstr(in_ref.GetAsText().GetAsciiString(), ".global");
						if (is_global)
						{
							//update global transform
							XSI::X3DObject xsi_3d_obj(XSI::SIObject(XSI::SIObject(in_ref).GetParent()).GetParent());
							if (xsi_3d_obj.GetObjectID() == camera_obj.GetObjectID())
							{
								//update camera transform
								update_status = update_scene(xsi_3d_obj, UpdateType_Camera);
							}
							else
							{
								//update global position of the scene object
								//but here we can move not only geometric object
								update_status = update_scene(xsi_3d_obj, UpdateType_Transform);
							}
						}
						//local and other transform should be ignored
						break;
					}
					case XSI::siCustomPrimitiveID:
					case XSI::siPrimitiveID:
					case XSI::siClusterID:
					case XSI::siClusterPropertyID:
					{
						XSI::X3DObject xsi_3d_obj(xsi_obj.GetParent());
						if (xsi_obj.GetType() == XSI::siPolyMeshType)
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_Mesh);
						}
						else if (xsi_obj.GetType() == "pointcloud")
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_Pointcloud);
						}
						else if (xsi_obj.GetType() == "light")
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_XsiLight);
						}
						else if (xsi_obj.GetType() == "camera" && xsi_3d_obj.GetObjectID() == camera_obj.GetObjectID())
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_Camera);
						}
						else if (xsi_obj.GetType() == "poly" || xsi_obj.GetType() == "sample")
						{
							//change cluster in the polygonmesh
							XSI::X3DObject xsi_object = XSI::X3DObject(XSI::SIObject(xsi_obj.GetParent()).GetParent());
							update_status = update_scene(xsi_object, UpdateType_Mesh);
						}
						else
						{
							//unknown update of the primitive or cluster
						}
						
						break;
					}
					case XSI::siMaterialID:
					{
						//change material
						//also called when we assign other material to the mesh
						XSI::Material xsi_material(in_ref);
						//xsi_material is a local instance of the material inside the object
						//it has another id with respect to the same material in the library
						//moreover, this material does not belongs to any library
						update_status = update_scene(xsi_material, xsi_material.GetParent().GetClassID() != XSI::siMaterialLibraryID);
						break;
					}
					case XSI::siParameterID:
					{
						//here called update for the ambience parameter, but we catch it in another place
						//or some other unknown parameter
						break;
					}
					case XSI::siCustomPropertyID:
					case XSI::siPropertyID:
					{
						XSI::CString property_type(xsi_obj.GetType());
						XSI::X3DObject xsi_3d_obj(xsi_obj.GetParent());
						if (property_type == "visibility")
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_Visibility);
						}
						else if (property_type == "geomapprox")
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_Mesh);
						}
						else if (property_type == "RenderRegion")
						{
							update_status = update_scene_render();
						}
						else if (property_type == render_options_name)
						{
							update_status = update_scene_render();
						}
						else if (property_type == "AmbientLighting")
						{
							update_status = update_scene(xsi_3d_obj, UpdateType_GlobalAmbient);
						}
						else
						{
							//get parent object, and if this is a pointcloud, hair or mesh - update it
							if (xsi_3d_obj.GetType() == XSI::siPolyMeshType)
							{
								update_status = update_scene(xsi_3d_obj, UpdateType_Mesh);
							}
							else if (xsi_3d_obj.GetType() == "pointcloud")
							{
								update_status = update_scene(xsi_3d_obj, UpdateType_Pointcloud);
							}
							else if (xsi_3d_obj.GetType() == "light")
							{
								update_status = update_scene(xsi_3d_obj, UpdateType_XsiLight);
							}
							else
							{
								//update unknown property
							}
						}
						break;
					}
					case XSI::siHairPrimitiveID:
					{
						XSI::X3DObject xsi_3d_obj(xsi_obj.GetParent());
						update_status = update_scene(xsi_3d_obj, UpdateType_Hair);
						break; 
					}
					case XSI::siShaderID:
					{
						XSI::Shader xsi_shader(in_ref);
						XSI::CRef shader_root = xsi_shader.GetRoot();
						if (shader_root.GetClassID() == XSI::siMaterialID)
						{
							XSI::Material shader_material(shader_root);
							update_status = update_scene(shader_material, false);
						}
						else
						{
							//update shader inside material
							//ignore this update, because we will catch material update in other place
						}
						break;
					}
					case XSI::siSIObjectID:  //call this when we delete the cluster, for example
					case XSI::siX3DObjectID:
					{
						//ignore this update
						break;
					}
					case XSI::siPassID:
					{
						update_status = update_scene(xsi_obj, UpdateType_Pass);
						break;
					}
					default:
					{
						//unknown update
					}
				}

				if (update_status == XSI::CStatus::Abort)
				{
					//update is fail, recreate the scene
					force_recreate_scene = false;
					create_scene();
					break;
				}
			}
		}
	}

	XSI::CStatus post_status = post_scene();
	if (post_status == XSI::CStatus::Abort)
	{
		force_recreate_scene = false;
		create_scene();

		post_status = post_scene();
	}

	//after scene creation, clear dirty list
	bool empty_dirty_list = dirty_refs_value.IsEmpty();
	if (empty_dirty_list)
	{
		m_render_context.SetObjectClean(XSI::CRef());
	}
	else
	{
		XSI::CRefArray dirty_refs = dirty_refs_value;
		for (int i = 0; i < dirty_refs.GetCount(); i++)
		{
			m_render_context.SetObjectClean(dirty_refs[i]);
		}
	}

	//if we obtain abort command during scene process, does not start rendering
	if (note_abort)
	{
		return XSI::CStatus::Abort;
	}

	return post_status;
}

XSI::CStatus RenderEngineBase::start_render()
{
	XSI::CStatus status;

	if (begin_render_event(m_render_context, output_paths) != XSI::CStatus::OK)
	{
		return XSI::CStatus::Fail;
	}

	m_render_context.NewFrame(visual_buffer.full_width, visual_buffer.full_height);

	start_prepare_render_time = clock();

	render();

	finish_render_time = clock();

	if (end_render_event(m_render_context, output_paths, false) != XSI::CStatus::OK)
	{
		return XSI::CStatus::Fail;
	}
	ready_to_render = true;
	return status;
}

XSI::CStatus RenderEngineBase::post_render()
{
	//save output images
	for (ULONG i = 0; i < output_paths.GetCount(); i++)
	{
		//construct subvector
		ULONG shift_a = i * static_cast<ULONG>(image_size_width) * static_cast<ULONG>(image_size_height) * 4;
		ULONG shift_b = (i + 1) * static_cast<ULONG>(image_size_width) * static_cast<ULONG>(image_size_height) * 4;
		std::vector<float>::const_iterator first = output_pixels.begin() + shift_a;
		std::vector<float>::const_iterator last = output_pixels.begin() + shift_b;
		std::vector<float> pixels(first, last);

		//save these pixels
		write_float(output_paths[i], output_formats[i], output_data_types[i], image_size_width, image_size_height, 4, pixels);

		pixels.clear();
		pixels.shrink_to_fit();
	}

	XSI::CStatus status = post_render_engine();

	//clear output arrays if we need this
	output_paths.Clear();
	output_formats.Clear();
	output_data_types.Clear();
	output_channels.Clear();
	output_bits.clear();
	output_bits.shrink_to_fit();
	//clear output_pixels
	output_pixels.clear();
	output_pixels.shrink_to_fit();

	return status;
}

void RenderEngineBase::on_object_add(XSI::CRef& in_ctxt)
{
	force_recreate_scene = true;
}

void RenderEngineBase::on_object_remove(XSI::CRef& in_ctxt)
{
	force_recreate_scene = true;
}

void RenderEngineBase::abort_render()
{
	note_abort = true;

	//call abort in the engine
	abort();
}

void RenderEngineBase::clear()
{
	output_paths.Clear();
	prev_isolated_objects.Clear();
	visual_buffer.clear();

	output_formats.Clear();
	output_data_types.Clear();
	output_channels.Clear();
	output_bits.clear();
	output_bits.shrink_to_fit();
	output_pixels.clear();
	output_pixels.shrink_to_fit();

	clear_engine();
}