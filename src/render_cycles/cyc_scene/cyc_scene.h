#pragma once
#include "scene/scene.h"

#include <xsi_renderercontext.h>
#include <xsi_camera.h>
#include <xsi_status.h>
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_texture.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>

#include "../../render_cycles/update_context.h"
#include "../../render_base/type_enums.h"
#include "../../input/input.h"

void sync_shader_settings(ccl::Scene* scene, const XSI::CParameterRefArray& render_parameters, RenderType render_type, const XSI::CTime& eval_time);
bool find_scene_shaders_displacement(ccl::Scene* scene);
void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CRef& shaderball_material, ShaderballType shaderball_type, ULONG shaderball_material_id);
XSI::CStatus update_transform(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_camera
XSI::CStatus sync_camera(ccl::Scene* scene, UpdateContext* update_context);

// cyc_light
void sync_xsi_lights(ccl::Scene* scene, const std::vector<XSI::Light>& xsi_lights, UpdateContext* update_context);
void sync_custom_lights(ccl::Scene* scene, const std::vector<XSI::X3DObject>& custom_lights, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);
void sync_background_color(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);

// this method called when we change ambience color
// when we change background light shader, then engine calls update material
XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context);
XSI::CStatus update_background_parameters(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);
XSI::CStatus update_xsi_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);
XSI::CStatus update_custom_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object);
XSI::CStatus update_xsi_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);
XSI::CStatus update_custom_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object);
void update_background(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);

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

// cyc_shadeerball
void sync_shaderball_hero(ccl::Scene* scene, const XSI::X3DObject& xsi_object, int shader_index, ShaderballType shaderball_type);
void sync_shaderball_light(ccl::Scene* scene, ShaderballType shaderball_type);
void sync_shaderball_camera(ccl::Scene* scene, UpdateContext* update_context, ShaderballType shaderball_type);

// cyc_polymesh
ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes);