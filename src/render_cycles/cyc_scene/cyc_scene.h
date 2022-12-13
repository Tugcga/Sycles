#pragma once
#include "scene/scene.h"

#include <xsi_renderercontext.h>
#include <xsi_camera.h>
#include <xsi_status.h>

#include "../../render_cycles/update_context.h"
#include "../../render_base/type_enums.h"
#include "../../input/input.h"

void sync_scene(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters, const XSI::CRef& shaderball_material, ShaderballType shaderball_type);
XSI::CStatus update_transform(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_camera
XSI::CStatus sync_camera(ccl::Scene* scene, UpdateContext* update_context);

// cyc_light
void sync_xsi_lights(ccl::Scene* scene, const std::vector<XSI::Light>& xsi_lights, UpdateContext* update_context);
void sync_background_color(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);

// this method called when we change ambience color
// when we change background light shader, then engine calls update material
XSI::CStatus update_background_color(ccl::Scene* scene, UpdateContext* update_context);
XSI::CStatus update_background_parameters(ccl::Scene* scene, UpdateContext* update_context, const XSI::CParameterRefArray& render_parameters);
XSI::CStatus update_xsi_light(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);
XSI::CStatus update_xsi_light_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::Light& xsi_light);

// cyc_materials
// for test only, create some default shader and return the index in the sahders array
int create_default_shader(ccl::Scene* scene);
int create_emission_checker(ccl::Scene* scene, float checker_scale);

// cyc_shadeerball
void sync_shaderball_hero(ccl::Scene* scene, const XSI::X3DObject& xsi_object, int shader_index, ShaderballType shaderball_type);
void sync_shaderball_light(ccl::Scene* scene, ShaderballType shaderball_type);
void sync_shaderball_camera(ccl::Scene* scene, UpdateContext* update_context, ShaderballType shaderball_type);

// cyc_polymesh
ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes);