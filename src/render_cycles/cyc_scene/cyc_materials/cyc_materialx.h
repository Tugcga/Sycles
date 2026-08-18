#pragma once
#include "scene/scene.h"
#include "scene/shader_graph.h"

#include <xsi_shader.h>

#include "../../update_context.h"

bool sync_materialx_material(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, UpdateContext* update_context, const XSI::Shader& root_node, const XSI::CString& material_name);