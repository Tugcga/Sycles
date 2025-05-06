#pragma once
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/hair.h"

#include <xsi_renderercontext.h>
#include <xsi_camera.h>
#include <xsi_status.h>
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_texture.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_transformation.h>
#include <xsi_kinematicstate.h>

#include "../../render_cycles/update_context.h"
#include "../../render_base/type_enums.h"
#include "../../input/input.h"

void sync_shader_settings(ccl::Scene* scene, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const ULONG shaderball_displacement, const XSI::CTime& eval_time);
bool find_scene_shaders_displacement(ccl::Scene* scene);
void sync_shaderball_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& scene_list, const XSI::CRef& shaderball_material, ShaderballType shaderball_type, ULONG shaderball_material_id);
void sync_instance_children(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& children, const XSI::KinematicState& master_kine, ULONG xsi_master_object_id, const std::vector<XSI::MATH::CTransformation>& tfms_array, const std::vector<ULONG>& master_ids, ULONG object_id, bool need_motion, const std::vector<float>& motion_times, size_t main_motion_step, const XSI::CTime& eval_time);
void sync_instance_model(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model& instance_model, const std::vector<XSI::MATH::CTransformation>& override_instance_tfm_array, std::vector<ULONG> master_ids, ULONG override_root_id);
void sync_instance_model(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model& instance_model);
void sync_poitcloud_instances(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object, const std::vector<XSI::MATH::CTransformation>& root_tfms = {});
void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRefArray& isolation_list, const XSI::CRefArray& lights_list, const XSI::CRefArray& all_x3dobjects_list, const XSI::CRefArray& all_models_list);
XSI::CStatus update_transform(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_camera
XSI::CStatus sync_camera(ccl::Scene* scene, UpdateContext* update_context);

// cyc_light
ccl::uint light_visibility_flag(bool use_camera, bool use_diffuse, bool use_glossy, bool use_transmission, bool use_scatter);
void sync_light_tfm(ccl::Object* light_object, const XSI::MATH::CMatrix4& xsi_tfm_matrix);
XSI::MATH::CTransformation tweak_xsi_light_transform(const XSI::MATH::CTransformation& xsi_tfm, const XSI::Light& xsi_light, const XSI::CTime& eval_time);
void sync_xsi_light(ccl::Scene* scene, const XSI::Light &xsi_light, UpdateContext* update_context);
void sync_custom_background(ccl::Scene* scene, const XSI::X3DObject& xsi_object, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);
void sync_custom_light(ccl::Scene* scene, const XSI::X3DObject& xsi_object, UpdateContext* update_context);
void sync_background_color(ccl::Scene* scene, UpdateContext* update_context);

// this method called when we change ambience color
// when we change background light shader, then engine calls update material
XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context);
XSI::CStatus update_background_parameters(ccl::Scene* scene, UpdateContext* update_context);
XSI::CStatus update_xsi_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);
XSI::CStatus update_custom_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object);
XSI::CStatus update_xsi_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);
XSI::CStatus update_custom_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object);
void update_background(ccl::Scene* scene, UpdateContext* update_context);

// cyc_materials
// this method used only for developing
void log_all_shaders();
// for test only, create some default shader and return the index in the sahders array
int create_default_shader(ccl::Scene* scene);
int create_emission_checker(ccl::Scene* scene, float checker_scale);
int sync_material(ccl::Scene* scene, const XSI::Material& xsi_material, const XSI::CTime& eval_time, std::vector<XSI::CStringArray>& aovs);  // return shader index in the Cycles shaders array
int sync_shaderball_shadernode(ccl::Scene* scene, const XSI::Shader& xsi_shader, bool is_surface, const XSI::CTime& eval_time);
int sync_shaderball_texturenode(ccl::Scene* scene, const XSI::Texture& xsi_texture, const XSI::CTime& eval_time);
void sync_scene_materials(ccl::Scene* scene, UpdateContext* update_context);
XSI::CStatus update_material(ccl::Scene* scene, const XSI::Material& xsi_material, size_t shader_index, const XSI::CTime& eval_time, std::vector<XSI::CStringArray>& aovs);
XSI::CStatus update_shaderball_shadernode(ccl::Scene* scene, ULONG xsi_id, ShaderballType shaderball_type, size_t shader_index, const XSI::CTime& eval_time);
bool get_material_id_from_name(const XSI::CString& material_identificator, ULONG& io_id);
XSI::CStatus sync_missed_material(ccl::Scene* scene, UpdateContext* update_context, int material_id);

// cyc_shaderball
void sync_shaderball_background_object(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object, ShaderballType shaderball_type);
void sync_shaderball_hero(ccl::Scene* scene, const XSI::X3DObject& xsi_object, int shader_index, ShaderballType shaderball_type);
void sync_shaderball_light(ccl::Scene* scene, ShaderballType shaderball_type);
void sync_shaderball_camera(ccl::Scene* scene, UpdateContext* update_context, ShaderballType shaderball_type);

// cyc_isntance
XSI::MATH::CTransformation calc_instance_object_tfm(const XSI::MATH::CTransformation& master_root_tfm, const XSI::MATH::CTransformation& master_object_tfm, const XSI::MATH::CTransformation& instance_root_tfm);
std::vector<XSI::MATH::CTransformation> calc_instance_object_tfm(const XSI::KinematicState& master_root, const XSI::KinematicState& master_object, const std::vector<XSI::MATH::CTransformation>& instance_root_tfm_array, bool need_motion, const std::vector<double> &motion_times, const XSI::CTime& eval_time);
XSI::CStatus update_instance_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model& xsi_model);
XSI::CStatus update_instance_transform_from_master_object(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_transform
void sync_transform(ccl::Object* object, UpdateContext* update_context, const XSI::KinematicState &xsi_kine);
void sync_transforms(ccl::Object* object, const std::vector<XSI::MATH::CTransformation>& xsi_tfms_array, size_t main_motion_step);
XSI::CStatus sync_geometry_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object);
std::vector<XSI::MATH::CTransformation> build_transforms_array(const XSI::KinematicState& xsi_kine, bool need_motion, const std::vector<double>& motion_times, const XSI::CTime& eval_time);

// cyc_light_linking
void sync_light_linking(ccl::Scene* scene, UpdateContext* update_context);