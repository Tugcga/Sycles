#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/camera.h"

#include <xsi_renderercontext.h>
#include <xsi_primitive.h>
#include <xsi_camera.h>
#include <xsi_kinematics.h>
#include <xsi_x3dobject.h>
#include <xsi_light.h>
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_texture.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_model.h>

#include "../../utilities/logs.h"
#include "../../utilities/math.h"
#include "../cyc_session/cyc_session.h"
#include "../cyc_scene/cyc_scene.h"
#include "primitives_geometry.h"
#include "../../render_base/type_enums.h"
#include "../../input/input.h"
#include "../../utilities/xsi_properties.h"

// cube from -1 to 1 (the edge size is 2)
ccl::Mesh* build_cube(ccl::Scene* scene)
{
	return build_primitive(scene, cube_vertex_count, cube_vertices, cube_faces_count, cube_face_sizes, cube_face_indexes);
}

ccl::Mesh* build_sphere(ccl::Scene* scene)
{
	return build_primitive(scene, sphere_vertex_count, sphere_vertices, sphere_faces_count, sphere_face_sizes, sphere_face_indexes);
}

void sync_demo_scene(ccl::Scene *scene, UpdateContext* update_context)
{
	// for test purpose only we create simple scene with one plane and one cube with sky light
	// from actual xsi scene we get only camera position
	// for meshes use default surface shader (it has index 0)

	// create shader for shpere
	ccl::Shader* sphere_shader = new ccl::Shader();
	sphere_shader->name = "sphere_shader";
	ccl::ShaderGraph* sphere_shader_graph = new ccl::ShaderGraph();
	ccl::SubsurfaceScatteringNode* sss_node = sphere_shader_graph->create_node<ccl::SubsurfaceScatteringNode>();
	sphere_shader_graph->add(sss_node);
	ccl::ColorNode* color_node = sphere_shader_graph->create_node<ccl::ColorNode>();
	sphere_shader_graph->add(color_node);
	color_node->set_value(ccl::make_float3(1.0, 0.2, 0.2));
	ccl::OutputAOVNode* color_aov_node = sphere_shader_graph->create_node<ccl::OutputAOVNode>();
	sphere_shader_graph->add(color_aov_node);
	color_aov_node->set_name(ccl::ustring(add_prefix_to_aov_name(XSI::CString("sphere_color_aov"), true).GetAsciiString()));
	// we use changed names for attributes, but output to the passes original name
	// these names will be changed by the same function
	ccl::OutputAOVNode* value_aov_node = sphere_shader_graph->create_node<ccl::OutputAOVNode>();
	sphere_shader_graph->add(value_aov_node);
	value_aov_node->set_name(ccl::ustring(add_prefix_to_aov_name("sphere_value_aov", false).GetAsciiString()));
	ccl::NoiseTextureNode* noise_node = sphere_shader_graph->create_node<ccl::NoiseTextureNode>();
	sphere_shader_graph->add(noise_node);
	// make connections
	sphere_shader_graph->connect(color_node->output("Color"), sss_node->input("Color"));
	sphere_shader_graph->connect(color_node->output("Color"), color_aov_node->input("Color"));
	sphere_shader_graph->connect(noise_node->output("Color"), value_aov_node->input("Value"));
	sphere_shader_graph->connect(noise_node->output("Color"), sss_node->input("Scale"));

	ccl::ShaderNode* sphere_out = sphere_shader_graph->output();
	sphere_shader_graph->connect(sss_node->output("BSSRDF"), sphere_out->input("Surface"));
	sphere_shader->set_graph(sphere_shader_graph);
	sphere_shader->tag_update(scene);
	scene->shaders.push_back(sphere_shader);
	int sphere_shader_id = scene->shaders.size() - 1;

	// create shader for plane
	ccl::Shader* plane_shader = new ccl::Shader();
	plane_shader->name = "plane_shader";
	ccl::ShaderGraph* plane_shader_graph = new ccl::ShaderGraph();
	ccl::GlossyBsdfNode* glossy_node = plane_shader_graph->create_node<ccl::GlossyBsdfNode>();
	plane_shader_graph->add(glossy_node);
	glossy_node->set_roughness(0.25f);
	ccl::OutputAOVNode* plane_value_aov_node = plane_shader_graph->create_node<ccl::OutputAOVNode>();
	plane_shader_graph->add(plane_value_aov_node);
	plane_value_aov_node->set_name(ccl::ustring(add_prefix_to_aov_name("plane_value_aov", false).GetAsciiString()));
	ccl::CheckerTextureNode* checker_node = plane_shader_graph->create_node<ccl::CheckerTextureNode>();
	plane_shader_graph->add(checker_node);
	checker_node->set_scale(0.3f);
	checker_node->set_color1(ccl::make_float3(0.2, 0.2, 0.2));
	checker_node->set_color2(ccl::make_float3(0.8, 0.8, 0.8));
	// connections
	plane_shader_graph->connect(checker_node->output("Color"), plane_value_aov_node->input("Value"));
	plane_shader_graph->connect(checker_node->output("Color"), glossy_node->input("Color"));
	ccl::ShaderNode* plane_out = plane_shader_graph->output();
	plane_shader_graph->connect(glossy_node->output("BSDF"), plane_out->input("Surface"));
	plane_shader->set_graph(plane_shader_graph);
	plane_shader->tag_update(scene);

	scene->shaders.push_back(plane_shader);
	int plane_shader_id = scene->shaders.size() - 1;

	// add plane
	ccl::Mesh* plane_mesh = scene->create_node<ccl::Mesh>();

	ccl::array<ccl::Node*> plane_used_shaders;
	plane_used_shaders.push_back_slow(scene->shaders[plane_shader_id]);
	plane_mesh->set_used_shaders(plane_used_shaders);

	ccl::Object* plane_object = new ccl::Object();
	plane_object->set_geometry(plane_mesh);
	plane_object->name = "plane";
	plane_object->set_asset_name(ccl::ustring("plane"));
	//plane_object->set_is_shadow_catcher(true);

	ccl::Transform plane_tfm = ccl::transform_identity();
	plane_object->set_tfm(plane_tfm);
	scene->objects.push_back(plane_object);

	plane_mesh->reserve_mesh(4, 2);  // on plane 4 vertices, 2 triangles
	float plane_radius = 48.0;  // large plane for shadow catcher
	ccl::array<ccl::float3> vertices(4);
	vertices[0] = ccl::make_float3(plane_radius, 0, plane_radius);
	vertices[1] = ccl::make_float3(-plane_radius, 0, plane_radius);
	vertices[2] = ccl::make_float3(-plane_radius, 0, -plane_radius);
	vertices[3] = ccl::make_float3(plane_radius, 0, -plane_radius);
	plane_mesh->set_verts(vertices);

	// triangles
	plane_mesh->add_triangle(0, 1, 2, 0, false);
	plane_mesh->add_triangle(0, 2, 3, 0, false);

	// uvs
	ccl::Attribute* uv_attr = plane_mesh->attributes.add(ccl::ATTR_STD_UV, ccl::ustring("std_uv"));
	ccl::float2* default_uv = uv_attr->data_float2();
	float scale = 0.75f;
	default_uv[0] = ccl::make_float2(scale, scale);
	default_uv[1] = ccl::make_float2(0.0, scale);
	default_uv[2] = ccl::make_float2(0.0, 0.0);
	default_uv[3] = ccl::make_float2(scale, scale);
	default_uv[4] = ccl::make_float2(0.0, 0.0);
	default_uv[5] = ccl::make_float2(scale, 0.0);

	// shaders
	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[0]);

	// add sphere
	ccl::array<ccl::Node*> sphere_used_shaders;
	sphere_used_shaders.push_back_slow(scene->shaders[sphere_shader_id]);
	ccl::Mesh* sphere_mesh = build_sphere(scene);
	sphere_mesh->set_used_shaders(sphere_used_shaders);
	ccl::Object* sphere_object = new ccl::Object();
	sphere_object->set_geometry(sphere_mesh);
	sphere_object->name = "sphere";
	sphere_object->set_asset_name(ccl::ustring("sphere"));
	ccl::Transform sphere_tfm = ccl::transform_identity();
	sphere_tfm = sphere_tfm * ccl::transform_translate(ccl::make_float3(0, 6, 0)) * ccl::transform_scale(2.0f, 2.0f, 2.0f);
	sphere_object->set_tfm(sphere_tfm);
	scene->objects.push_back(sphere_object);

	// add cube
	ccl::Mesh* cube_mesh = build_cube(scene);
	cube_mesh->set_used_shaders(used_shaders);
	ccl::Object* cube_object = new ccl::Object();
	cube_object->set_geometry(cube_mesh);
	cube_object->name = "cube";
	cube_object->set_asset_name(ccl::ustring("cube"));
	ccl::Transform cube_tfm = ccl::transform_identity();
	cube_tfm = cube_tfm * ccl::transform_translate(ccl::make_float3(0, 2, 0)) * ccl::transform_scale(2.0f, 2.0f, 2.0f);
	cube_object->set_tfm(cube_tfm);
	scene->objects.push_back(cube_object);

	// add one more cube
	ccl::Object* second_cube_object = new ccl::Object();
	second_cube_object->set_geometry(cube_mesh);
	second_cube_object->name = "second_cube";
	second_cube_object->set_asset_name(ccl::ustring("second cube"));
	ccl::Transform second_cube_tfm = ccl::transform_identity();
	second_cube_tfm = second_cube_tfm * ccl::transform_translate(ccl::make_float3(3.5, 1, 2)) * ccl::transform_scale(1.0f, 1.0f, 1.0f);
	second_cube_object->set_tfm(second_cube_tfm);
	scene->objects.push_back(second_cube_object);

	// background
	/*ccl::Shader* bg_shader = scene->default_background;
	ccl::ShaderGraph* bg_graph = new ccl::ShaderGraph();
	ccl::BackgroundNode* bg_node = bg_graph->create_node<ccl::BackgroundNode>();
	bg_node->input("Color")->set(ccl::make_float3(1.0, 1.0, 1.0));
	bg_node->input("Strength")->set(0.25);
	ccl::ShaderNode* bg_out = bg_graph->output();
	bg_graph->add(bg_node);
	bg_graph->connect(bg_node->output("Background"), bg_out->input("Surface"));
	ccl::SkyTextureNode* sky_node = bg_graph->create_node<ccl::SkyTextureNode>();
	sky_node->tex_mapping.rotation = ccl::make_float3(-0.5 * XSI::MATH::PI, 0, 0);
	sky_node->set_sun_rotation(DEG2RADF(45.0));
	sky_node->set_sun_elevation(DEG2RADF(36.0));
	sky_node->set_sun_size(0.001f);
	bg_graph->add(sky_node);
	bg_graph->connect(sky_node->output("Color"), bg_node->input("Color"));
	bg_shader->set_graph(bg_graph);
	bg_shader->tag_update(scene);

	scene->background->set_transparent(true);*/
}

void sync_shader_settings(ccl::Scene* scene, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const ULONG shaderball_displacement, const XSI::CTime& eval_time)
{
	// set common shader parameters for all shaders
	bool use_mis = render_type == RenderType_Shaderball ? true : (bool)render_parameters.GetValue("options_shaders_use_mis", eval_time);
	bool transparent_shadows = render_type == RenderType_Shaderball ? true : (bool)render_parameters.GetValue("options_shaders_transparent_shadows", eval_time);
	int disp_method = render_type == RenderType_Shaderball ? shaderball_displacement : (int)render_parameters.GetValue("options_displacement_method", eval_time);

	for (size_t i = 0; i < scene->shaders.size(); i++)
	{
		ccl::Shader* shader = scene->shaders[i];
		shader->set_use_mis(use_mis);
		shader->set_use_transparent_shadow(transparent_shadows);
		shader->set_displacement_method(disp_method == 0 ? ccl::DisplacementMethod::DISPLACE_BUMP : (disp_method == 1 ? ccl::DisplacementMethod::DISPLACE_TRUE : ccl::DisplacementMethod::DISPLACE_BOTH));

		shader->tag_use_mis_modified();
		shader->tag_use_transparent_shadow_modified();
		shader->tag_displacement_method_modified();
	}
}

bool find_scene_shaders_displacement(ccl::Scene* scene)
{
	for (size_t i = 0; i < scene->shaders.size(); i++)
	{
		ccl::Shader* shader = scene->shaders[i];
		if (shader->has_displacement)
		{
			return true;
		}
	}
	return false;
}

void gather_all_subobjects(const XSI::X3DObject& xsi_object, XSI::CRefArray& output)
{
	output.Add(xsi_object.GetRef());
	XSI::CRefArray children = xsi_object.GetChildren();
	for (ULONG i = 0; i < children.GetCount(); i++)
	{
		gather_all_subobjects(children[i], output);
	}
}

XSI::CRefArray gather_all_subobjects(const XSI::Model& root)
{
	XSI::CRefArray output;
	XSI::CRefArray children = root.GetChildren();
	for (ULONG i = 0; i < children.GetCount(); i++)
	{
		gather_all_subobjects(children[i], output);
	}
	return output;
}

void sync_shaderball_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& scene_list, const XSI::CRef& shaderball_material, ShaderballType shaderball_type, ULONG shaderball_material_id)
{
	int shader_index = -1;
	XSI::CTime eval_time = update_context->get_time();
	if (shaderball_type != ShaderballType_Unknown)
	{
		if (shaderball_type == ShaderballType_Material)
		{
			XSI::Material xsi_material(shaderball_material);
			std::vector<XSI::CStringArray> aovs(2);
			aovs[0].Clear();
			aovs[1].Clear();

			shader_index = sync_material(scene, xsi_material, eval_time, aovs);
		}
		else if (shaderball_type == ShaderballType_SurfaceShader)
		{
			XSI::Shader xsi_shader(shaderball_material);
			shader_index = sync_shaderball_shadernode(scene, xsi_shader, true, eval_time);
		}
		else if (shaderball_type == ShaderballType_VolumeShader)
		{
			XSI::Shader xsi_shader(shaderball_material);
			shader_index = sync_shaderball_shadernode(scene, xsi_shader, false, eval_time);
		}
		else if (shaderball_type == ShaderballType_Texture)
		{
			XSI::Texture xsi_texture(shaderball_material);
			shader_index = sync_shaderball_texturenode(scene, xsi_texture, eval_time);
		}
		else
		{
			shader_index = create_default_shader(scene);
		}
	}

	if (shader_index >= 0) {
		update_context->add_material_index(shaderball_material_id, shader_index, shaderball_type);

		// setup shaderball polymesh
		bool assign_hero = false;
		for (size_t i = 0; i < scene_list.GetCount(); i++)
		{
			XSI::CRef object_ref = scene_list[i];
			XSI::siClassID object_class = object_ref.GetClassID();
			// ignore cameras and lights, consider only polymeshes inside models
			if (object_class == XSI::siModelID)
			{
				XSI::Model xsi_model(object_ref);
				XSI::CRefArray model_objects = gather_all_subobjects(xsi_model);
				for (LONG j = 0; j < model_objects.GetCount(); j++)
				{
					XSI::X3DObject xsi_object(model_objects[j]);
					XSI::CString xsi_type = xsi_object.GetType();
					if (xsi_type == "polymsh")
					{
						if (!assign_hero)
						{
							sync_shaderball_hero(scene, xsi_object, shader_index, shaderball_type);
							assign_hero = true;
						}
						else
						{
							// mesh is background object
							// TODO: sync polymeshes
						}
					}
				}
			}
		}

		// lights
		sync_shaderball_light(scene, shaderball_type);

		// camera
		sync_shaderball_camera(scene, update_context, shaderball_type);
	}
}

void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CRefArray& isolation_list, const XSI::CRefArray& lights_list, const XSI::CRefArray& all_x3dobjects_list, const XSI::CRefArray &all_models_list)
{
	RenderType render_type = update_context->get_render_type();
	XSI::CTime eval_time = update_context->get_time();

	sync_scene_materials(scene, update_context);

	sync_camera(scene, update_context);

	if (isolation_list.GetCount() > 0)
	{// render isolation view
		// we should use all objects from isolation list and all light objects (build-in and custom) from all objects list

	}
	else
	{// render general scene view
		// in this case we should enumerate objects from complete list
		size_t objects_count = all_x3dobjects_list.GetCount();
		for (size_t i = 0; i < objects_count; i++)
		{
			XSI::CRef object_ref = all_x3dobjects_list[i];
			XSI::siClassID object_class = object_ref.GetClassID();
			if (object_class == XSI::siLightID)
			{// built-in light
				XSI::X3DObject xsi_object(object_ref);
				if (is_render_visible(xsi_object, eval_time))
				{
					XSI::Light xsi_light(xsi_object);
					sync_xsi_light(scene, xsi_light, update_context);
				}
			}
			else if (object_class == XSI::siX3DObjectID)
			{
				XSI::X3DObject xsi_object(object_ref);
				XSI::CString object_type = xsi_object.GetType();

				if (is_render_visible(xsi_object, eval_time))
				{
					if (object_type == "polymsh")
					{
						
					}
					else if (object_type == "cyclesPoint" || object_type == "cyclesSun" || object_type == "cyclesSpot" || object_type == "cyclesArea")
					{
						sync_custom_light(scene, xsi_object, update_context);
					}
					else if (object_type == "cyclesBackground")
					{
						sync_custom_background(scene, xsi_object, update_context, render_parameters, eval_time);
					}
					else
					{
						log_message("unknown x3dobject " + object_type);
					}
				}
			}
			else if (object_class == XSI::siCameraID || object_class == XSI::siNullID || object_class == XSI::siCameraRigID)
			{
				// ignore nothing to do
			}
			else
			{
				log_message("unknown object class " + XSI::CString(object_class));
			}
		}

		// next iterate models
		size_t models_count = all_models_list.GetCount();
		for (size_t i = 0; i < models_count; i++)
		{
			XSI::CRef xsi_model_ref = all_models_list[i];
			XSI::Model xsi_model(xsi_model_ref);
			if (xsi_model.IsValid() && is_render_visible(xsi_model, eval_time))
			{
				XSI::siModelKind model_kind = xsi_model.GetModelKind();
				if (model_kind == XSI::siModelKind_Instance)
				{// this is instance model
					sync_instance_model(scene, update_context, xsi_model);
				}
			}
		}
	}

	if (!update_context->get_use_background_light())
	{
		sync_background_color(scene, update_context, render_parameters);
	}

	sync_demo_scene(scene, update_context);
}

/*void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CRef& shaderball_material, ShaderballType shaderball_type, ULONG shaderball_material_id)
{
	RenderType render_type = update_context->get_render_type();
	XSI::CTime eval_time = update_context->get_time();
	if (render_type == RenderType_Shaderball)
	{
		// for renderball we create a separate scene
		int shader_index = -1;
		if (shaderball_type != ShaderballType_Unknown)
		{
			if (shaderball_type == ShaderballType_Material)
			{
				XSI::Material xsi_material(shaderball_material);
				std::vector<XSI::CStringArray> aovs(2);
				aovs[0].Clear();
				aovs[1].Clear();

				shader_index = sync_material(scene, xsi_material, eval_time, aovs);
			}
			else if (shaderball_type == ShaderballType_SurfaceShader)
			{
				XSI::Shader xsi_shader(shaderball_material);
				shader_index = sync_shaderball_shadernode(scene, xsi_shader, true, eval_time);
			}
			else if (shaderball_type == ShaderballType_VolumeShader)
			{
				XSI::Shader xsi_shader(shaderball_material);
				shader_index = sync_shaderball_shadernode(scene, xsi_shader, false, eval_time);
			}
			else if (shaderball_type == ShaderballType_Texture)
			{
				XSI::Texture xsi_texture(shaderball_material);
				shader_index = sync_shaderball_texturenode(scene, xsi_texture, eval_time);
			}
			else
			{
				shader_index = create_default_shader(scene);
			}
		}

		if (shader_index >= 0) {
			update_context->add_material_index(shaderball_material_id, shader_index, shaderball_type);

			// setup shaderball polymesh
			sync_shaderball_hero(scene, update_context->get_shaderball(), shader_index, shaderball_type);

			// lights
			sync_shaderball_light(scene, shaderball_type);

			// camera
			sync_shaderball_camera(scene, update_context, shaderball_type);

			// TODO: setup background objects
		}
	}
	else
	{
		// in all other cases use scene from the Softimage
		sync_scene_materials(scene, update_context);
		sync_camera(scene, update_context);
		sync_xsi_lights(scene, update_context->get_xsi_lights(), update_context);
		sync_custom_lights(scene, update_context->get_custom_lights(), update_context, render_parameters);
		if (!update_context->get_use_background_light())
		{
			sync_background_color(scene, update_context, render_parameters);
		}

		sync_demo_scene(scene, update_context);
	}
}*/

XSI::CStatus update_transform(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CString object_type = xsi_object.GetType();

	if (object_type == "light")
	{// default Softimage light
		XSI::Light xsi_light(xsi_object);
		XSI::CStatus is_update = update_xsi_light_transform(scene, update_context, xsi_light);
		if (is_update == XSI::CStatus::OK)
		{
			// try to update instances (if it exists)
			is_update = update_instance_transform_from_master_object(scene, update_context, xsi_object);
		}

		return is_update;
	}
	else if (object_type == "cyclesPoint" || object_type == "cyclesSun" || object_type == "cyclesSpot" || object_type == "cyclesArea")
	{
		XSI::CStatus is_update = update_custom_light_transform(scene, update_context, xsi_object);
		if (is_update == XSI::CStatus::OK)
		{
			is_update = update_instance_transform_from_master_object(scene, update_context, xsi_object);
		}
		return is_update;
	}
	else if (object_type == "#model")
	{
		XSI::Model xsi_model(xsi_object);
		XSI::siModelKind model_kind = xsi_model.GetModelKind();
		if (model_kind == XSI::siModelKind_Instance)
		{// this is instance model
			// TODO: may be we change transform of the instance root, but it contained in some master root
			return update_instance_transform(scene, update_context, xsi_model);
		}
	}
	else
	{// unknown object type
		log_message("update transform for unknown " + object_type);
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}