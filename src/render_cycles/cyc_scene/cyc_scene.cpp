#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/pointcloud.h"
#include "scene//light.h"
#include "util/hash.h"

#include <unordered_set>

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
#include <xsi_group.h>

#include "../../render_base/type_enums.h"
#include "../../input/input.h"
#include "../../utilities/xsi_properties.h"
#include "../../utilities/logs.h"
#include "../../utilities/math.h"
#include "../cyc_session/cyc_session.h"
#include "cyc_scene.h"
#include "cyc_geometry/cyc_geometry.h"
#include "primitives_geometry.h"

// cube from -1 to 1 (the edge size is 2)
ccl::Mesh* build_cube(ccl::Scene* scene)
{
	return build_primitive(scene, cube_vertex_count, cube_vertices, cube_faces_count, cube_face_sizes, cube_face_indexes);
}

ccl::Mesh* build_sphere(ccl::Scene* scene)
{
	return build_primitive(scene, sphere_vertex_count, sphere_vertices, sphere_faces_count, sphere_face_sizes, sphere_face_indexes);
}

void sync_shader_settings(ccl::Scene* scene, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const ULONG shaderball_displacement, const XSI::CTime& eval_time)
{
	// set common shader parameters for all shaders
	int emission_sampling = render_type == RenderType_Shaderball ? 1 /*Auto*/ : (int)render_parameters.GetValue("options_shaders_emission_sampling", eval_time);
	bool transparent_shadows = render_type == RenderType_Shaderball ? true : (bool)render_parameters.GetValue("options_shaders_transparent_shadows", eval_time);
	int disp_method = render_type == RenderType_Shaderball ? shaderball_displacement : (int)render_parameters.GetValue("options_displacement_method", eval_time);
	bool bump_correction = render_type == RenderType_Shaderball ? true : (bool)render_parameters.GetValue("options_shaders_bump_map_correction", eval_time);

	for (size_t i = 0; i < scene->shaders.size(); i++)
	{
		ccl::Shader* shader = scene->shaders[i];
		shader->set_emission_sampling_method(emission_sampling == 0 ? ccl::EmissionSampling::EMISSION_SAMPLING_NONE :
			(emission_sampling == 1 ? ccl::EmissionSampling::EMISSION_SAMPLING_AUTO : 
			(emission_sampling == 2 ? ccl::EmissionSampling::EMISSION_SAMPLING_FRONT : 
			(emission_sampling == 3 ? ccl::EmissionSampling::EMISSION_SAMPLING_BACK : ccl::EmissionSampling::EMISSION_SAMPLING_FRONT_BACK))));
		shader->set_use_transparent_shadow(transparent_shadows);
		shader->set_displacement_method(disp_method == 0 ? ccl::DisplacementMethod::DISPLACE_BUMP : (disp_method == 1 ? ccl::DisplacementMethod::DISPLACE_TRUE : ccl::DisplacementMethod::DISPLACE_BOTH));
		shader->set_use_bump_map_correction(bump_correction);
		int background_volume_sampling = render_parameters.GetValue("background_volume_sampling", eval_time);
		shader->set_volume_sampling_method(background_volume_sampling == 2 ? ccl::VolumeSampling::VOLUME_SAMPLING_MULTIPLE_IMPORTANCE : (background_volume_sampling == 1 ? ccl::VolumeSampling::VOLUME_SAMPLING_EQUIANGULAR : ccl::VolumeSampling::VOLUME_SAMPLING_DISTANCE));
		int background_volume_interpolation = render_parameters.GetValue("background_volume_interpolation", eval_time);
		shader->set_volume_interpolation_method(background_volume_interpolation == 1 ? ccl::VolumeInterpolation::VOLUME_INTERPOLATION_CUBIC : ccl::VolumeInterpolation::VOLUME_INTERPOLATION_LINEAR);
		shader->set_volume_step_rate(render_parameters.GetValue("performance_volume_step_rate", eval_time));

		shader->tag_emission_sampling_method_modified();
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
	ULONG xsi_material_id = 0;  // reassign if shaderball rendered for material
	if (shaderball_type != ShaderballType_Unknown)
	{
		if (shaderball_type == ShaderballType_Material)
		{
			XSI::Material xsi_material(shaderball_material);
			std::vector<XSI::CStringArray> aovs(2);
			aovs[0].Clear();
			aovs[1].Clear();

			xsi_material_id = xsi_material.GetObjectID();

			shader_index = sync_material(scene, xsi_material, update_context);
		}
		else if (shaderball_type == ShaderballType_SurfaceShader)
		{
			XSI::Shader xsi_shader(shaderball_material);
			shader_index = sync_shaderball_shadernode(scene, xsi_shader, true, update_context);
		}
		else if (shaderball_type == ShaderballType_VolumeShader)
		{
			XSI::Shader xsi_shader(shaderball_material);
			shader_index = sync_shaderball_shadernode(scene, xsi_shader, false, update_context);
		}
		else if (shaderball_type == ShaderballType_Texture)
		{
			XSI::Texture xsi_texture(shaderball_material);
			shader_index = sync_shaderball_texturenode(scene, xsi_texture, update_context);
		}
		else
		{
			shader_index = create_default_shader(scene);
		}
	}

	if (shader_index >= 0) 
	{
		update_context->add_material_index(shaderball_material_id,
			shader_index, 
			scene->shaders[shader_index]->has_displacement && xsi_material_id > 0,
			shaderball_type);

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
							sync_shaderball_background_object(scene, update_context, xsi_object, shaderball_type);
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

void sync_instance_children_pointcloud_volume(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_id, XSI::X3DObject &xsi_object, ULONG xsi_master_object_id, const std::vector<ULONG>& master_ids, ULONG object_id, const std::vector<XSI::MATH::CTransformation> &instance_object_tfm_array, size_t main_motion_step)
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

void sync_instance_children(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& children, const XSI::KinematicState &master_kine, ULONG xsi_master_object_id, const std::vector<XSI::MATH::CTransformation> &tfms_array, const std::vector<ULONG> &master_ids, ULONG object_id, bool need_motion, const std::vector<double>& motion_times, size_t main_motion_step, const XSI::CTime& eval_time)
{
	for (size_t i = 0; i < children.GetCount(); i++)
	{
		XSI::X3DObject xsi_object(children[i]);
		sync_object_materials(scene, update_context, xsi_object);

		ULONG xsi_id = xsi_object.GetObjectID();
		XSI::CString xsi_object_type = xsi_object.GetType();

		std::vector<XSI::MATH::CTransformation> instance_object_tfm_array =
			calc_instance_object_tfm(
				master_kine,
				xsi_object.GetKinematics().GetGlobal(),
				tfms_array,
				need_motion, motion_times, eval_time);

		if (is_render_visible(xsi_object, true, eval_time))  // for instance we set ignore hide master
		{
			if (xsi_object_type == "polymsh")
			{
				bool is_contains_explosia = is_explosia(xsi_object, eval_time);
				bool export_as_volume = false;
				if (is_contains_explosia)
				{
					PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
					if (pointcloud_type == PointcloudType::PointcloudType_Volume)
					{
						sync_instance_children_pointcloud_volume(scene, update_context, xsi_id, xsi_object, xsi_master_object_id, master_ids, object_id, instance_object_tfm_array, main_motion_step);
						export_as_volume = true;
					}
				}

				if (!export_as_volume)
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
			}
			else if (xsi_object_type == "crvlist") {
				// we can create curve object only if it contains CyclesCurve property
				XSI::Property curve_prop = get_xsi_object_property(xsi_object, "CyclesCurve");
				if (curve_prop.IsValid()) {
					ccl::Object* curve_object = scene->create_node<ccl::Object>();
					ccl::Hair* curve_geom = sync_curve_object(scene, curve_object, update_context, xsi_object, curve_prop);

					curve_object->set_geometry(curve_geom);
					size_t object_index = scene->objects.size() - 1;
					update_context->add_object_index(xsi_id, object_index);

					sync_transforms(curve_object, instance_object_tfm_array, main_motion_step);

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_geometry_instance_data(object_id, object_index, m_ids);
				}
			}
			else if (xsi_object_type == "surfmsh") {
				XSI::Property surface_prop = get_xsi_object_property(xsi_object, "CyclesSurface");
				if (surface_prop.IsValid()) {
					ccl::Object* surface_object = scene->create_node<ccl::Object>();
					ccl::Mesh* surface_geom = sync_surface_object(scene, surface_object, update_context, xsi_object, surface_prop);

					surface_object->set_geometry(surface_geom);
					size_t object_index = scene->objects.size() - 1;
					update_context->add_object_index(xsi_id, object_index);

					sync_transforms(surface_object, instance_object_tfm_array, main_motion_step);

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_geometry_instance_data(object_id, object_index, m_ids);
				}
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
					sync_instance_children_pointcloud_volume(scene, update_context, xsi_id, xsi_object, xsi_master_object_id, master_ids, object_id, instance_object_tfm_array, main_motion_step);
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Instances)
				{
					sync_poitcloud_instances(scene, update_context, xsi_object, instance_object_tfm_array);
				}
			}
			else if (xsi_object_type == "VDBPrimitive")
			{
				ccl::Object* vdb_object = scene->create_node<ccl::Object>();
				XSI::CustomPrimitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));
				ccl::Volume* vdb_geom = sync_vdb_volume_object(scene, vdb_object, update_context, xsi_object, get_vdb_data(xsi_prim));
				vdb_object->set_geometry(vdb_geom);

				size_t object_index = scene->objects.size() - 1;
				update_context->add_object_index(xsi_id, object_index);
				sync_transform(vdb_object, update_context, xsi_object.GetKinematics().GetGlobal());

				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_geometry_instance_data(object_id, object_index, m_ids);
			}
			else if (xsi_object_type == "light")
			{
				// create copy of the light
				XSI::Light xsi_light(xsi_object);

				// in this method we set master object transform
				sync_xsi_light(scene, xsi_light, update_context);
				size_t light_index = scene->objects.size() - 1;
				ccl::Object* light_object = scene->objects[light_index];
				sync_light_tfm(light_object, tweak_xsi_light_transform(instance_object_tfm_array[main_motion_step], xsi_light, eval_time).GetMatrix4());

				// add data to update context about indices of masters and cycles objects
				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master_object_id);
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_light_instance_data(object_id, light_index, m_ids);
			}
			else if (xsi_object_type == "cyclesPoint" || xsi_object_type == "cyclesSun" || xsi_object_type == "cyclesSpot" || xsi_object_type == "cyclesArea")  // does not consider background, because it should be unique
			{
				size_t pre_objects_count = scene->objects.size();
				sync_custom_light(scene, xsi_object, update_context);
				size_t post_objects_count = scene->objects.size();
				// check is we actually add the light
				// because potentually (for invalid light object) we skip create new light process in sync_custom_light function
				if (post_objects_count > pre_objects_count) {
					size_t light_index = scene->objects.size() - 1;
					ccl::Object* light_object = scene->objects[light_index];
					sync_light_tfm(light_object, instance_object_tfm_array[main_motion_step].GetMatrix4());

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master_object_id);
					m_ids.push_back(xsi_object.GetObjectID());
					update_context->add_light_instance_data(object_id, light_index, m_ids);
				}
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
	std::vector<double> motion_times = update_context->get_motion_times();
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
	update_context->add_sync_profiler_time_start(SyncType::PoincloudInstances, xsi_object.GetObjectID(), xsi_object.GetFullName());

	XSI::CTime eval_time = update_context->get_time();
	std::vector<double> motion_times = update_context->get_motion_times();
	size_t motion_times_count = motion_times.size();

	bool use_root_tfm = root_tfms.size() > 0;
	if (use_root_tfm)
	{
		use_root_tfm = root_tfms.size() == motion_times_count;
	}

	bool override_color = true;
	XSI::Property pc_property = get_xsi_object_property(xsi_object, "CyclesPointcloud");
	bool is_property = pc_property.IsValid();
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

	XSI::ICEAttribute age_attribute = xsi_geometry.GetICEAttributeFromName("Age");
	XSI::CICEAttributeDataArrayFloat age_data;
	age_attribute.GetDataArray(age_data);
	ULONG age_data_count = age_data.GetCount();

	XSI::ICEAttribute lifetime_attribute = xsi_geometry.GetICEAttributeFromName("AgeLimit");
	XSI::CICEAttributeDataArrayFloat lifetime_data;
	lifetime_attribute.GetDataArray(lifetime_data);
	ULONG lifetime_data_count = lifetime_data.GetCount();

	XSI::ICEAttribute velocity_attribute = xsi_geometry.GetICEAttributeFromName("PointVelocity");
	XSI::CICEAttributeDataArrayVector3f velocity_data;
	velocity_attribute.GetDataArray(velocity_data);
	ULONG velocity_data_count = velocity_data.GetCount();

	XSI::ICEAttribute angular_velocity_attribute = xsi_geometry.GetICEAttributeFromName("AngularVelocity");
	XSI::CICEAttributeDataArrayRotationf angular_velocity_data;
	angular_velocity_attribute.GetDataArray(angular_velocity_data);
	ULONG angular_velocity_data_count = angular_velocity_data.GetCount();

	size_t shape_data_count = shape_data.GetCount();
	int scale_count = scale_attribute.GetElementCount();
	bool is_scale_define = scale_attribute.IsDefined();

	// array of transforms for points
	// the first array - for the first motion step (contains all point), the second array - for the second step and so on
	// if array contains only one array, then there are no motions
	std::vector<std::vector<XSI::MATH::CTransformation>> time_points_tfms = build_time_points_transforms(xsi_object, motion_times);

	XSI::KinematicState pointcloud_kine = xsi_object.GetKinematics().GetGlobal();

	// create the particle system for pointcloud
	ccl::ParticleSystem* psys = scene->create_node<ccl::ParticleSystem>();
	psys->particles.clear();
	psys->tag_update(scene);
	
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

			size_t start_objects_index = scene->objects.size();
			if (shape_type == XSI::siICEShapeReference)
			{
				XSI::MATH::CShape shape = shape_data[i];
				bool is_branch_selected = shape.IsBranchSelected();  // if true, then we should export the whole hierarchy, if false - then only the root object
				ULONG shape_ref_id = shape.GetReferenceID();
				XSI::X3DObject master_root = (XSI::X3DObject)XSI::Application().GetObjectFromID(shape_ref_id);
				if (!master_root.IsValid()) {
					log_warning("Invalid master object with id " + XSI::CString(shape_ref_id) + " for the instance.");
					continue;
				}

				// now we are ready to create instance of the root object
				XSI::CRefArray children = get_instance_children(master_root, is_branch_selected);
				// add all children ids to abort update transforms inside update context
				update_context->add_abort_update_transform_id(children);

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
			}
			else
			{
				ccl::Object* point_object = scene->create_node<ccl::Object>();
				sync_point_primitive_shape(scene, point_object, update_context, shape_type, point_color, point_tfms, xsi_object, eval_time);
			}

			// add additional attributes to all newly created objects
			size_t objects_count = scene->objects.size();
			for (size_t object_index = start_objects_index; object_index < objects_count; object_index++)
			{
				ccl::Object* new_object = scene->objects[object_index];

				// if we would like not override colors for child pointclouds, then use custom property
				// it's hard properly define when override should be by default
				if (override_color)
				{
					// override object colors by color from the point
					// for primitives we will make it twise, it does not matter
					new_object->set_color(color4_to_float3(point_color));
					new_object->set_alpha(point_color.GetA());

					new_object->tag_color_modified();
					new_object->tag_alpha_modified();
				}

				// next define particle attributes
				if (new_object->get_geometry()->need_attribute(scene, ccl::ATTR_STD_PARTICLE))
				{
					ccl::Particle pa;
					pa.index = i;
					if (i < age_data_count) { pa.age = age_data[i]; } else { pa.age = 0.0f; }
					if (i < lifetime_data_count) { pa.lifetime = lifetime_data[i]; } else { pa.lifetime = 0.0f; }
					pa.location = vector3_to_float3(current_point_tfm.GetTranslation());
					XSI::MATH::CQuaternion q = current_point_tfm.GetRotationQuaternion();
					pa.rotation = quaternion_to_float4(q);
					pa.size = size_data[i];
					if (i < velocity_data_count) { pa.velocity = vector3_to_float3(velocity_data[i]); } else { pa.velocity = ccl::make_float3(0.0, 0.0, 0.0); }
					if (i < angular_velocity_data_count) { pa.angular_velocity = rotation_to_float3(angular_velocity_data[i]); } else { pa.angular_velocity = ccl::make_float3(0.0, 0.0, 0.0); }

					psys->particles.push_back_slow(pa);

					new_object->set_particle_system(psys);
					new_object->set_particle_index(psys->particles.size() - 1);
				}
			}
		}
	}
	ULONG xsi_id = xsi_object.GetObjectID();
	update_context->add_abort_update_transform_id(xsi_id);

	update_context->add_sync_profiler_time_finish(SyncType::PoincloudInstances, xsi_object.GetObjectID());
}

void sync_xsi_pointcloud_volume(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_id, XSI::X3DObject& xsi_object)
{
	update_context->add_sync_profiler_time_start(SyncType::Volume, xsi_id, xsi_object.GetFullName());

	ccl::Object* volume_object = scene->create_node<ccl::Object>();
	ccl::Volume* volume_geom = sync_volume_object(scene, volume_object, update_context, xsi_object);
	volume_object->set_geometry(volume_geom);

	update_context->add_object_index(xsi_id, scene->objects.size() - 1);
	sync_transform(volume_object, update_context, xsi_object.GetKinematics().GetGlobal());

	update_context->add_sync_profiler_time_finish(SyncType::Volume, xsi_id);
}

void sync_scene_object(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRef &object_ref, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
{
	// at first get all required materials and export it
	// and only then export actual object
	sync_object_materials(scene, update_context, object_ref);

	XSI::siClassID object_class = object_ref.GetClassID();

	if (object_class == XSI::siLightID)
	{// built-in light
		XSI::X3DObject xsi_object(object_ref);
		if (is_render_visible(xsi_object, false, eval_time))
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

		if (is_render_visible(xsi_object, false, eval_time))
		{
			if (object_type == "polymsh")
			{
				bool export_as_volume = false;
				if (is_explosia(xsi_object, eval_time))
				{
					// may be this is pointcloud
					PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
					if (pointcloud_type == PointcloudType::PointcloudType_Volume)
					{
						// use the same process as for ice volumes
						sync_xsi_pointcloud_volume(scene, update_context, xsi_id, xsi_object);

						export_as_volume = true;
					}
					// if this mesh does not contains volume attributes, export it as polymesh
				}
				if(!export_as_volume)
				{
					update_context->add_sync_profiler_time_start(SyncType::Polymesh, xsi_object.GetObjectID(), xsi_object.GetFullName());

					ccl::Object* mesh_object = scene->create_node<ccl::Object>();
					ccl::Mesh* mesh_geom = sync_polymesh_object(scene, mesh_object, update_context, xsi_object);

					mesh_geom->tag_update(scene, true);
					mesh_object->set_geometry(mesh_geom);

					update_context->add_object_index(xsi_id, scene->objects.size() - 1);
					sync_transform(mesh_object, update_context, xsi_object.GetKinematics().GetGlobal());

					update_context->add_sync_profiler_time_finish(SyncType::Polymesh, xsi_object.GetObjectID());
				}
				
			}
			else if (object_type == "crvlist") {
				// create curve only if contains CyclesCurve property
				XSI::Property curve_prop = get_xsi_object_property(xsi_object, "CyclesCurve");
				if (curve_prop.IsValid()) {
					update_context->add_sync_profiler_time_start(SyncType::Curve, xsi_object.GetObjectID(), xsi_object.GetFullName());

					ccl::Object* curve_object = scene->create_node<ccl::Object>();
					ccl::Hair* curve_geom = sync_curve_object(scene, curve_object, update_context, xsi_object, curve_prop);
					curve_object->set_geometry(curve_geom);

					update_context->add_object_index(xsi_id, scene->objects.size() - 1);
					sync_transform(curve_object, update_context, xsi_object.GetKinematics().GetGlobal());

					update_context->add_sync_profiler_time_finish(SyncType::Curve, xsi_object.GetObjectID());
				}
			}
			else if (object_type == "surfmsh") {
				XSI::Property surface_prop = get_xsi_object_property(xsi_object, "CyclesSurface");
				if (surface_prop.IsValid()) {
					update_context->add_sync_profiler_time_start(SyncType::Surface, xsi_object.GetObjectID(), xsi_object.GetFullName());

					ccl::Object* surface_object = scene->create_node<ccl::Object>();
					ccl::Mesh* surface_geom = sync_surface_object(scene, surface_object, update_context, xsi_object, surface_prop);
					surface_object->set_geometry(surface_geom);

					update_context->add_object_index(xsi_id, scene->objects.size() - 1);
					sync_transform(surface_object, update_context, xsi_object.GetKinematics().GetGlobal());

					update_context->add_sync_profiler_time_finish(SyncType::Surface, xsi_object.GetObjectID());
				}
			}
			else if (object_type == "hair")
			{
				update_context->add_sync_profiler_time_start(SyncType::Hair, xsi_object.GetObjectID(), xsi_object.GetFullName());

				ccl::Object* hair_object = scene->create_node<ccl::Object>();
				// WARNING: there is a strange bug
				// if we create cycles object inside the function, then the render is crash
				ccl::Hair* hair_geom = sync_hair_object(scene, hair_object, update_context, xsi_object);
				hair_object->set_geometry(hair_geom);

				update_context->add_object_index(xsi_id, scene->objects.size() - 1);
				sync_transform(hair_object, update_context, xsi_object.GetKinematics().GetGlobal());

				update_context->add_sync_profiler_time_finish(SyncType::Hair, xsi_object.GetObjectID());
			}
			else if (object_type == "pointcloud")
			{
				PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
				if (pointcloud_type == PointcloudType::PointcloudType_Strands)
				{
					update_context->add_sync_profiler_time_start(SyncType::Strands, xsi_object.GetObjectID(), xsi_object.GetFullName());

					ccl::Object* strands_object = scene->create_node<ccl::Object>();
					ccl::Hair* strands_geom = sync_strands_object(scene, strands_object, update_context, xsi_object);
					strands_object->set_geometry(strands_geom);

					update_context->add_object_index(xsi_id, scene->objects.size() - 1);
					sync_transform(strands_object, update_context, xsi_object.GetKinematics().GetGlobal());

					update_context->add_sync_profiler_time_finish(SyncType::Strands, xsi_object.GetObjectID());
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Points)
				{
					update_context->add_sync_profiler_time_start(SyncType::Points, xsi_object.GetObjectID(), xsi_object.GetFullName());

					ccl::Object* points_object = scene->create_node<ccl::Object>();
					ccl::PointCloud* points_geom = sync_points_object(scene, points_object, update_context, xsi_object);
					points_object->set_geometry(points_geom);

					update_context->add_object_index(xsi_id, scene->objects.size() - 1);
					sync_transform(points_object, update_context, xsi_object.GetKinematics().GetGlobal());

					update_context->add_sync_profiler_time_finish(SyncType::Points, xsi_object.GetObjectID());
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Volume)
				{
					// use separate function, because it is the same for mesh with explosia attributes
					sync_xsi_pointcloud_volume(scene, update_context, xsi_id, xsi_object);
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
			else if (object_type == "VDBPrimitive")
			{
				update_context->add_sync_profiler_time_start(SyncType::VDB, xsi_object.GetObjectID(), xsi_object.GetFullName());

				ccl::Object* vdb_object = scene->create_node<ccl::Object>();
				XSI::CustomPrimitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));
				ccl::Volume* vdb_geom = sync_vdb_volume_object(scene, vdb_object, update_context, xsi_object, get_vdb_data(xsi_prim));
				vdb_object->set_geometry(vdb_geom);

				update_context->add_object_index(xsi_id, scene->objects.size() - 1);
				sync_transform(vdb_object, update_context, xsi_object.GetKinematics().GetGlobal());

				update_context->add_sync_profiler_time_finish(SyncType::VDB, xsi_object.GetObjectID());
			}
		}
	}
	else if (object_class == XSI::siModelID) {
		/* In isolation mode, when the model is isolated but its constituent objects are not, the master is not exported.
		*  But be careful with Scene_Root; it is also a model and can be in isolation as well.
		*  If, for each isolated model, we export all its sub‑objects, we may export the same object multiple times.
		*  There is something wrong with objects in isolation.
		*  When we add only the model, nothing is added — only a null object appears.
		*  When we select the model hierarchy with the middle mouse button and add it, it is added to the view, but none of the objects from the hierarchy appear in the list.
		*  When we manually add the object, it appears
		*/
		XSI::Model xsi_model(object_ref);
		if (xsi_model.IsValid() && is_render_visible(xsi_model, false, eval_time))
		{
			XSI::siModelKind model_kind = xsi_model.GetModelKind();
			if (model_kind == XSI::siModelKind_Instance)
			{// this is instance model
				// we should implement the function here, because in ohter file it produce the crash
				sync_instance_model(scene, update_context, xsi_model);
			}
		}
	}
	else if (object_class == XSI::siCameraID || object_class == XSI::siNullID || object_class == XSI::siCameraRigID)
	{
		// ignore nothing to do
	}
}

void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& isolation_list, const XSI::CRefArray& lights_list, const XSI::CRefArray& all_x3dobjects_list, const XSI::CRefArray &all_models_list)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	update_context->clear_aovs();

	sync_camera(scene, update_context);

	if (isolation_list.GetCount() > 0) {
		// we should use all objects from isolation list and all light objects (build-in and custom) from all objects list
		size_t isolation_objects_count = isolation_list.GetCount();

		std::unordered_set<ULONG> exported_ids;
		for (size_t i = 0; i < isolation_objects_count; i++)
		{
			XSI::CRef object_ref = isolation_list[i];
			XSI::X3DObject xsi_object(object_ref);
			if (xsi_object.IsValid()) { exported_ids.insert(xsi_object.GetObjectID()); }
			sync_scene_object(scene, update_context, object_ref, render_parameters, eval_time);
		}

		// but also we should add all lights from scene, if one of them is not already exported
		// for this we should enumerate all scene objects and select only lights
		size_t objects_count = all_x3dobjects_list.GetCount();
		for (size_t i = 0; i < objects_count; i++)
		{
			XSI::CRef object_ref = all_x3dobjects_list[i];
			bool is_ref_light = is_light(object_ref);
			if (is_ref_light)
			{
				XSI::X3DObject xsi_object(object_ref);
				ULONG xsi_id = xsi_object.GetObjectID();
				if (!exported_ids.contains(xsi_id))
				{
					exported_ids.insert(xsi_id);
					// this light object is not exported, do it now
					sync_scene_object(scene, update_context, object_ref, render_parameters, eval_time);
				}
			}
		}

		exported_ids.clear();
	}
	else
	{// render general scene view
		// in this case we should enumerate objects from complete list
		size_t objects_count = all_x3dobjects_list.GetCount();
		for (size_t i = 0; i < objects_count; i++)
		{
			XSI::CRef object_ref = all_x3dobjects_list[i];
			sync_scene_object(scene, update_context, object_ref, render_parameters, eval_time);
		}

		// next iterate models
		size_t models_count = all_models_list.GetCount();
		for (size_t i = 0; i < models_count; i++)
		{
			XSI::CRef xsi_model_ref = all_models_list[i];
			sync_scene_object(scene, update_context, xsi_model_ref, render_parameters, eval_time);
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
	else if (object_type == "polymsh" || object_type == "crvlist" || object_type == "hair" || object_type == "surfmsh" || object_type == "VDBPrimitive")
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
	else if (object_type == "pointcloud")
	{
		PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
		// for pointcloud instances we does not need to update transforms
		// because in this case we recreate the scene from scratch
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
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}



XSI::CStatus reset_positions(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object) {
	ULONG xsi_id = xsi_object.GetObjectID();
	XSI::CTime eval_time = update_context->get_time();
	if (update_context->has_positions(xsi_id) && update_context->is_object_exists(xsi_id)) {
		const ccl::array<ccl::packed_float3>* positions = update_context->get_positions(xsi_id);
		if (positions) {
			// next for different type of object we should extrac Cycles object in different ways
			XSI::CString xsi_type = xsi_object.GetType();
			if (xsi_type == "polymsh") {
				XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
				ULONG xsi_polymesh_id = xsi_primitive.GetObjectID();
				return reset_on_geometry(scene, update_context, xsi_polymesh_id, positions);
			}
			else if (xsi_type == "hair") {
				XSI::HairPrimitive xsi_hair_prim(xsi_object.GetActivePrimitive(eval_time));
				ULONG xsi_hair_id = xsi_hair_prim.GetObjectID();
				return reset_on_geometry(scene, update_context, xsi_hair_id, positions);
			}
			else if (xsi_type == "pointcloud") {
				PointcloudType pointcloud_type = get_pointcloud_type(xsi_object, eval_time);
				if (pointcloud_type == PointcloudType::PointcloudType_Strands) {
					XSI::Primitive xsi_strands_prim(xsi_object.GetActivePrimitive(eval_time));
					ULONG xsi_strands_id = xsi_strands_prim.GetObjectID();
					return reset_on_geometry(scene, update_context, xsi_strands_id, positions);
				}
				else if (pointcloud_type == PointcloudType::PointcloudType_Points) {
					XSI::Primitive xsi_points_prim(xsi_object.GetActivePrimitive(eval_time));
					ULONG xsi_points_id = xsi_points_prim.GetObjectID();
					return reset_on_geometry(scene, update_context, xsi_points_id, positions);
				}
			}
			else if (xsi_type == "surfmsh") {
				XSI::Primitive xsi_surface_prim(xsi_object.GetActivePrimitive(eval_time));
				ULONG xsi_surface_id = xsi_surface_prim.GetObjectID();
				return reset_on_geometry(scene, update_context, xsi_surface_id, positions);
			}
			else if (xsi_type == "crvlist") {
				XSI::Primitive xsi_curve_prim(xsi_object.GetActivePrimitive(eval_time));
				ULONG xsi_curve_id = xsi_curve_prim.GetObjectID();
				return reset_on_geometry(scene, update_context, xsi_curve_id, positions);
			}
		}
		else {
			return XSI::CStatus::Fail;
		}
	} 
	else {
		// no stored positions, we can not reset it
		return XSI::CStatus::Fail;
	}

	return XSI::CStatus::Fail;
}