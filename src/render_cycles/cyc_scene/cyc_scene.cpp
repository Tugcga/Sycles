#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/pointcloud.h"
#include "util/hash.h"

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
#include "cyc_scene.h"
#include "cyc_geometry/cyc_geometry.h"
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
	
	/*ccl::Object* cube_object = scene->create_node<ccl::Object>();
	cube_object->set_geometry(cube_mesh);
	cube_object->name = "cube";
	cube_object->set_asset_name(ccl::ustring("cube"));
	ccl::Transform cube_tfm = ccl::transform_identity();
	cube_tfm = cube_tfm * ccl::transform_translate(ccl::make_float3(0, 2, 0)) * ccl::transform_scale(2.0f, 2.0f, 2.0f);
	cube_object->set_tfm(cube_tfm);*/
	ccl::Hair* hair_geom = scene->create_node<ccl::Hair>();
	hair_geom->set_used_shaders(used_shaders);
	hair_geom->reserve_curves(0, 0);
	hair_geom->add_curve_key(ccl::make_float3(0.0, 0.0, 0.0), 1.0);
	hair_geom->add_curve_key(ccl::make_float3(0.0, 4.0, 0.0), 1.0);
	hair_geom->add_curve(0, 0);

	ccl::Object* hair_object = scene->create_node<ccl::Object>();
	hair_object->set_geometry(hair_geom);

	// add one more cube
	ccl::Object* second_cube_object = new ccl::Object();
	second_cube_object->set_geometry(cube_mesh);
	second_cube_object->name = "second_cube";
	second_cube_object->set_asset_name(ccl::ustring("second cube"));
	ccl::Transform second_cube_tfm = ccl::transform_identity();
	second_cube_tfm = second_cube_tfm * ccl::transform_translate(ccl::make_float3(3.5, 1, 2)) * ccl::transform_scale(1.0f, 1.0f, 1.0f);
	second_cube_object->set_tfm(second_cube_tfm);
	scene->objects.push_back(second_cube_object);
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

// find all children, not only at first level
XSI::CRefArray get_model_children(const XSI::X3DObject& xsi_object)
{
	XSI::CStringArray str_families_subobject;
	return xsi_object.FindChildren2("", "", str_families_subobject);
}

XSI::CRefArray get_instance_children(XSI::X3DObject& xsi_root, bool is_branch_selected)
{
	if (is_branch_selected)
	{
		XSI::CString xsi_object_type = xsi_root.GetType();
		if (xsi_object_type == "#model")
		{
			XSI::Model xsi_model(xsi_root);
			XSI::siModelKind model_kind = xsi_model.GetModelKind();
			if (model_kind == XSI::siModelKind_Instance)
			{
				XSI::CRefArray to_return;
				to_return.Add(xsi_root);
				return to_return;
			}
		}

		return get_model_children(xsi_root);
	}
	else
	{
		XSI::CRefArray to_return;
		to_return.Add(xsi_root);
		return to_return;
	}
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

void sync_instance_children(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& children, const XSI::KinematicState &master_kine, ULONG xsi_master_object_id, const std::vector<XSI::MATH::CTransformation> &tfms_array, const std::vector<ULONG> &master_ids, ULONG object_id, bool need_motion, const std::vector<float>& motion_times, size_t main_motion_step, const XSI::CTime& eval_time)
{
	for (size_t i = 0; i < children.GetCount(); i++)
	{
		XSI::X3DObject xsi_object(children[i]);
		ULONG xsi_id = xsi_object.GetObjectID();
		XSI::CString xsi_object_type = xsi_object.GetType();

		std::vector<XSI::MATH::CTransformation> instance_object_tfm_array =
			calc_instance_object_tfm(
				master_kine,
				xsi_object.GetKinematics().GetGlobal(),
				tfms_array,
				need_motion, motion_times, eval_time);

		if (is_render_visible(xsi_object, eval_time))
		{
			if (xsi_object_type == "polymsh")
			{
				ccl::Object* mesh_object = scene->create_node<ccl::Object>();
				ccl::Mesh* mesh_geom = sync_polymesh_object(scene, mesh_object, update_context, xsi_object);

				mesh_object->set_geometry(mesh_geom);
				size_t object_index = scene->objects.size() - 1;
				update_context->add_object_index(xsi_id, object_index);

				sync_transforms(mesh_object, instance_object_tfm_array, main_motion_step);

				// add data to update context about indices of masters and cycles objects
				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_geometry_instance_data(object_id, object_index, m_ids);
			}
			else if (xsi_object_type == "hair")
			{
				ccl::Object* hair_object = scene->create_node<ccl::Object>();
				ccl::Hair* hair_geom = sync_hair_object(scene, hair_object, update_context, xsi_object);

				hair_object->set_geometry(hair_geom);
				size_t object_index = scene->objects.size() - 1;
				update_context->add_object_index(xsi_id, object_index);

				sync_transforms(hair_object, instance_object_tfm_array, main_motion_step);

				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_geometry_instance_data(object_id, object_index, m_ids);
			}
			else if (xsi_object_type == "pointcloud")
			{
				PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
				if (pointcloud_type == PointcloudType::PointcloudType_Strands)
				{
					ccl::Object* strands_object = scene->create_node<ccl::Object>();
					ccl::Hair* strands_geom = sync_strands_object(scene, strands_object, update_context, xsi_object);

					strands_object->set_geometry(strands_geom);
					size_t object_index = scene->objects.size() - 1;
					update_context->add_object_index(xsi_id, object_index);

					sync_transforms(strands_object, instance_object_tfm_array, main_motion_step);

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_geometry_instance_data(object_id, object_index, m_ids);
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Points)
				{
					ccl::Object* points_object = scene->create_node<ccl::Object>();
					ccl::PointCloud* point_geom = sync_points_object(scene, points_object, update_context, xsi_object);

					points_object->set_geometry(point_geom);
					size_t object_index = scene->objects.size() - 1;
					update_context->add_object_index(xsi_id, object_index);

					sync_transforms(points_object, instance_object_tfm_array, main_motion_step);

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_geometry_instance_data(object_id, object_index, m_ids);
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Volume)
				{
					ccl::Object* volume_object = scene->create_node<ccl::Object>();
					ccl::Volume* volume_geom = sync_volume_object(scene, volume_object, update_context, xsi_object);

					volume_object->set_geometry(volume_geom);
					size_t object_index = scene->objects.size() - 1;
					update_context->add_object_index(xsi_id, object_index);

					sync_transforms(volume_object, instance_object_tfm_array, main_motion_step);

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_geometry_instance_data(object_id, object_index, m_ids);
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Instances)
				{
					sync_poitcloud_instances(scene, update_context, xsi_object, instance_object_tfm_array);
				}
			}
			else if (xsi_object_type == "light")
			{
				// create copy of the light
				XSI::Light xsi_light(xsi_object);

				// in this method we set master object transform
				sync_xsi_light(scene, xsi_light, update_context);
				size_t light_index = scene->lights.size() - 1;
				ccl::Light* light = scene->lights[light_index];
				sync_light_tfm(light, tweak_xsi_light_transform(instance_object_tfm_array[main_motion_step], xsi_light, eval_time).GetMatrix4());

				// add data to update context about indices of masters and cycles objects
				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_light_instance_data(object_id, light_index, m_ids);
			}
			else if (xsi_object_type == "cyclesPoint" || xsi_object_type == "cyclesSun" || xsi_object_type == "cyclesSpot" || xsi_object_type == "cyclesArea")  // does not consider background, because it should be unique
			{
				sync_custom_light(scene, xsi_object, update_context);

				size_t light_index = scene->lights.size() - 1;
				ccl::Light* light = scene->lights[light_index];
				sync_light_tfm(light, instance_object_tfm_array[main_motion_step].GetMatrix4());

				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_light_instance_data(object_id, light_index, m_ids);
			}
			else if (xsi_object_type == "#model")
			{
				XSI::Model xsi_model(xsi_object);
				XSI::siModelKind model_kind = xsi_model.GetModelKind();
				if (model_kind == XSI::siModelKind_Instance)
				{// this is instance inside the instance
					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					// and pass it to the sync method as real global transform fo the instanciated root object
					sync_instance_model(scene, update_context, xsi_model, instance_object_tfm_array, m_ids, object_id);

					// we should write to the update context the data about nested instance
					update_context->add_nested_instance_data(xsi_model.GetObjectID(), object_id);
				}
			}
			else
			{
				log_message("unknown object in instance master " + xsi_object.GetName() + " type: " + xsi_object_type);
			}
		}
	}
}

// this function has three optional arguments: override_instance_tfm, master_ids and override_root_id
// these arguments used when the instance contains nested instance
// in this case we process this nested instance, but pass as these arguments data from the top level
// so, override_instance_tfm is either identical or transfrom of the top level instance object
// master_ids either empty or the path of id to the nested subinstance
// override_root_id is id of the top level instance
void sync_instance_model(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model& instance_model, const std::vector<XSI::MATH::CTransformation>& override_instance_tfm_array, std::vector<ULONG> master_ids, ULONG override_root_id)
{
	bool use_override = master_ids.size() > 0;

	XSI::Model xsi_master = instance_model.GetInstanceMaster();
	XSI::CRefArray children = get_model_children(xsi_master);
	XSI::CTime eval_time = update_context->get_time();

	bool need_motion = update_context->get_need_motion();
	std::vector<float> motion_times = update_context->get_motion_times();
	size_t main_motion_step = update_context->get_main_motion_step();

	// build array of transforms for motions
	// if motion is disabled, then this array contains only one transform
	std::vector<XSI::MATH::CTransformation> instance_tfms_array = build_transforms_array(instance_model.GetKinematics().GetGlobal(), need_motion, motion_times, eval_time);

	sync_instance_children(scene,
		update_context, 
		children, 
		xsi_master.GetKinematics().GetGlobal(), 
		xsi_master.GetObjectID(), 
		use_override ? override_instance_tfm_array : instance_tfms_array,
		master_ids, 
		use_override ? override_root_id : instance_model.GetObjectID(), 
		need_motion,
		motion_times, 
		main_motion_step, 
		eval_time);
}

void sync_instance_model(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model& instance_model)
{
	sync_instance_model(scene, update_context, instance_model, { XSI::MATH::CTransformation() }, {}, 0);
}

void sync_poitcloud_instances(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object, const std::vector<XSI::MATH::CTransformation>& root_tfms)
{
	XSI::CTime eval_time = update_context->get_time();
	std::vector<float> motion_times = update_context->get_motion_times();
	size_t motion_times_count = motion_times.size();

	bool override_color = true;
	XSI::Property pc_property;
	bool is_property = get_xsi_object_property(xsi_object, "CyclesPointcloud", pc_property);
	if (is_property)
	{
		XSI::CParameterRefArray prop_params = pc_property.GetParameters();
		override_color = prop_params.GetValue("use_pc_color", eval_time);
	}

	// current time
	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);

	XSI::ICEAttribute shape_attribute = xsi_geometry.GetICEAttributeFromName("Shape");
	XSI::CICEAttributeDataArrayShape shape_data;
	shape_attribute.GetDataArray(shape_data);

	XSI::ICEAttribute position_attribute = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::CICEAttributeDataArrayVector3f position_data;
	position_attribute.GetDataArray(position_data);

	XSI::ICEAttribute orientation_attribute = xsi_geometry.GetICEAttributeFromName("Orientation");
	XSI::CICEAttributeDataArrayRotationf rotation_data;
	orientation_attribute.GetDataArray(rotation_data);

	XSI::ICEAttribute size_attribute = xsi_geometry.GetICEAttributeFromName("Size");
	XSI::CICEAttributeDataArrayFloat size_data;
	size_attribute.GetDataArray(size_data);

	XSI::ICEAttribute scale_attribute = xsi_geometry.GetICEAttributeFromName("Scale");
	XSI::CICEAttributeDataArrayVector3f scale_data;
	scale_attribute.GetDataArray(scale_data);

	XSI::ICEAttribute color_attribute = xsi_geometry.GetICEAttributeFromName("Color");
	XSI::CICEAttributeDataArrayColor4f color_data;
	color_attribute.GetDataArray(color_data);

	size_t shape_data_count = shape_data.GetCount();
	int scale_count = scale_attribute.GetElementCount();
	bool is_scale_define = scale_attribute.IsDefined();

	std::vector<std::vector<XSI::MATH::CTransformation>> time_points_tfms = build_time_points_transforms(xsi_object, motion_times);

	XSI::KinematicState pointcloud_kine = xsi_object.GetKinematics().GetGlobal();
	bool use_root_tfm = root_tfms.size() > 0;
	if (use_root_tfm)
	{
		use_root_tfm = root_tfms.size() == motion_times_count;
	}
	size_t export_point_index = 0;
	for (size_t i = 0; i < shape_data_count; i++)
	{
		XSI::siICEShapeType shape_type = shape_data[i].GetType();
		if (is_valid_shape(shape_type))
		{
			// if time_points_tfms contains only one array, then elements of this array are actual transforms for all points
			// if it contains several arrays, then these arrays are transforms for all valid points in different times
			// index i is index of the point in pointcloud, but it can be invalid
			// so, we should check with export_point_index index value in predefined transforms array
			// time_points_tfms contains array of times, each time contains array of point transforms
			XSI::MATH::CTransformation current_point_tfm = build_point_transform(position_data[i], rotation_data[i], size_data[i], i < scale_count ? scale_data[i] : XSI::MATH::CVector3f(1.0f, 1.0f, 1.0f), is_scale_define);

			std::vector<XSI::MATH::CTransformation> point_tfms(motion_times_count);
			for (size_t t = 0; t < motion_times_count; t++)
			{
				// use array of transforms at time t
				if (time_points_tfms[t].size() > export_point_index)
				{
					point_tfms[t] = time_points_tfms[t][export_point_index];
				}
				else
				{
					// there is no point transform at time t
					// at time t the number of valid points less then current index export_point_index
					// use current transform
					// WARNING: this produce incorrect result when there are different number of particels during the time
					// all particles rendered at the end position without motion blur
					point_tfms[t] = current_point_tfm;
				}
				
				if (use_root_tfm)
				{
					// if root transforms are not empty, then use it instead of real pointcloud root transfrom
					// because it contains particles transforms
					point_tfms[t].MulInPlace(root_tfms[t]);
				}
				else
				{
					// apply pointcloud root transform to each time transform
					point_tfms[t].MulInPlace(xsi_object.GetKinematics().GetGlobal().GetTransform(motion_times[t]));
				}
			}

			XSI::MATH::CColor4f point_color = color_data[i];

			// increase index of exported point
			export_point_index++;

			if (shape_type == XSI::siICEShapeReference)
			{
				XSI::MATH::CShape shape = shape_data[i];
				bool is_branch_selected = shape.IsBranchSelected();  // if true, then we should export the whole hierarchy, if false - then only the root object
				ULONG shape_ref_id = shape.GetReferenceID();
				XSI::X3DObject master_root = (XSI::X3DObject)XSI::Application().GetObjectFromID(shape_ref_id);

				// now we are ready to create instance of the root object
				XSI::CRefArray children = get_instance_children(master_root, is_branch_selected);
				// add all children ids to abort update transforms inside update context
				update_context->add_abort_update_transform_id(children);

				size_t start_objects_index = scene->objects.size();
				sync_instance_children(scene,  // scene
					update_context,  // update context
					children,  // children array
					master_root.GetKinematics().GetGlobal(),  // masster object kine
					master_root.GetObjectID(),  // master object id
					point_tfms,  // array with global transforms of the instance root
					{},  // path for nested instances, contains master ids
					xsi_object.GetObjectID(),  // instance root object id
					// here is a problem, because one pointcloud play the role of several root instances
					// there are no actual instance root, because this is a point in the cloud
					// this id used to construct the path for update instance transforms
					update_context->get_need_motion(),  // use motion
					update_context->get_motion_times(),  // motion times
					update_context->get_main_motion_step(),  // main motion step
					eval_time);

				if (override_color)
				{
					size_t objects_count = scene->objects.size();
					// override object colors by color from the point
					for (size_t object_index = start_objects_index; object_index < objects_count; object_index++)
					{
						ccl::Object* new_object = scene->objects[object_index];
						new_object->set_color(color4_to_float3(point_color));
						new_object->set_alpha(point_color.GetA());

						new_object->tag_color_modified();
						new_object->tag_alpha_modified();
					}
				}
			}
			else
			{
				ccl::Object* point_object = scene->create_node<ccl::Object>();
				sync_point_primitive_shape(scene, point_object, update_context, shape_type, point_color, point_tfms, xsi_object, eval_time);
			}
		}
	}
	ULONG xsi_id = xsi_object.GetObjectID();
	update_context->add_pointcloud_instance_id(xsi_id);
	update_context->add_abort_update_transform_id(xsi_id);
}

void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& isolation_list, const XSI::CRefArray& lights_list, const XSI::CRefArray& all_x3dobjects_list, const XSI::CRefArray &all_models_list)
{
	RenderType render_type = update_context->get_render_type();
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

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
				ULONG xsi_id = xsi_object.GetObjectID();
				XSI::CString object_type = xsi_object.GetType();

				if (is_render_visible(xsi_object, eval_time))
				{
					if (object_type == "polymsh")
					{
						ccl::Object* mesh_object = scene->create_node<ccl::Object>();
						ccl::Mesh* mesh_geom = sync_polymesh_object(scene, mesh_object, update_context, xsi_object);
						mesh_object->set_geometry(mesh_geom);

						update_context->add_object_index(xsi_id, scene->objects.size() - 1);
						sync_transform(mesh_object, update_context, xsi_object.GetKinematics().GetGlobal());
					}
					else if (object_type == "hair")
					{
						ccl::Object* hair_object = scene->create_node<ccl::Object>();
						// WARNING: there is a strange bug
						// if we create cycles object inside the funciotn, then the render is crash
						ccl::Hair* hair_geom = sync_hair_object(scene, hair_object, update_context, xsi_object);
						hair_object->set_geometry(hair_geom);

						update_context->add_object_index(xsi_id, scene->objects.size() - 1);
						sync_transform(hair_object, update_context, xsi_object.GetKinematics().GetGlobal());
					}
					else if (object_type == "pointcloud")
					{
						PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
						if (pointcloud_type == PointcloudType::PointcloudType_Strands)
						{
							ccl::Object* strands_object = scene->create_node<ccl::Object>();
							ccl::Hair* strands_geom = sync_strands_object(scene, strands_object, update_context, xsi_object);
							strands_object->set_geometry(strands_geom);

							update_context->add_object_index(xsi_id, scene->objects.size() - 1);
							sync_transform(strands_object, update_context, xsi_object.GetKinematics().GetGlobal());
						}
						else if(pointcloud_type == PointcloudType::PointcloudType_Points)
						{
							ccl::Object* points_object = scene->create_node<ccl::Object>();
							ccl::PointCloud* points_geom = sync_points_object(scene, points_object, update_context, xsi_object);
							points_object->set_geometry(points_geom);

							update_context->add_object_index(xsi_id, scene->objects.size() - 1);
							sync_transform(points_object, update_context, xsi_object.GetKinematics().GetGlobal());
						}
						else if (pointcloud_type == PointcloudType::PointcloudType_Volume)
						{
							ccl::Object* volume_object = scene->create_node<ccl::Object>();
							ccl::Volume* volume_geom = sync_volume_object(scene, volume_object, update_context, xsi_object);
							volume_object->set_geometry(volume_geom);

							update_context->add_object_index(xsi_id, scene->objects.size() - 1);
							sync_transform(volume_object, update_context, xsi_object.GetKinematics().GetGlobal());
						}
						else if (pointcloud_type == PointcloudType::PointcloudType_Instances)
						{
							sync_poitcloud_instances(scene, update_context, xsi_object);
						}
						else
						{

						}
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
					// we should implement the function here, because in ohter file it produce the crash
					sync_instance_model(scene, update_context, xsi_model);
				}
			}
		}
	}

	if (!update_context->get_use_background_light())
	{
		sync_background_color(scene, update_context);
	}
}

XSI::CStatus update_transform(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CString object_type = xsi_object.GetType();
	XSI::CTime eval_time = update_context->get_time();
	ULONG xsi_id = xsi_object.GetObjectID();

	if (update_context->is_abort_update_transform_id_exist(xsi_id)) { return XSI::CStatus::Abort; }

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
			return update_instance_transform(scene, update_context, xsi_model);
		}
	}
	else if (object_type == "polymsh")
	{
		XSI::CStatus is_update = sync_geometry_transform(scene, update_context, xsi_object);
		// here we set the same transform for all instances of the object
		// so, we should to sync instance transforms

		if (is_update == XSI::CStatus::OK)
		{
			is_update = update_instance_transform_from_master_object(scene, update_context, xsi_object);
		}
		return is_update;
	}
	else if (object_type == "hair")
	{
		XSI::CStatus is_update = sync_geometry_transform(scene, update_context, xsi_object);
		if (is_update == XSI::CStatus::OK)
		{
			is_update = update_instance_transform_from_master_object(scene, update_context, xsi_object);
		}

		return is_update;
	}
	else if (object_type == "pointcloud")
	{
		PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
		if (pointcloud_type == PointcloudType::PointcloudType_Strands || pointcloud_type == PointcloudType::PointcloudType_Points || pointcloud_type == PointcloudType::PointcloudType_Volume)
		{
			XSI::CStatus is_update = sync_geometry_transform(scene, update_context, xsi_object);
			if (is_update == XSI::CStatus::OK)
			{
				is_update = update_instance_transform_from_master_object(scene, update_context, xsi_object);
			}

			return is_update;
		}
	}
	else
	{// unknown object type
		log_message("update transform for unknown " + object_type);
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}