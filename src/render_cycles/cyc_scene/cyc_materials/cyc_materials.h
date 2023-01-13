#pragma once
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"

#include <xsi_shader.h>
#include <xsi_time.h>
#include <xsi_shaderparameter.h>

#include <unordered_map>

#include "../../../render_base/type_enums.h"

// common
// retun type of the shader node
// also output the name of the node of corresponding type
// for example, if node is OSL then out_type = "osl"
// if node is Cycles node, then out_type is it short name (VectorMapRange instead of CyclesVectorMapRange)
ShadernodeType get_shadernode_type(const XSI::Shader& xsi_shader, XSI::CString& out_type);
bool make_nodes_connection(ccl::ShaderGraph* shader_graph, ccl::ShaderNode* output_node, ccl::ShaderNode* input_node, const XSI::Shader& xsi_output_shader, const XSI::CString& xsi_output_name, const std::string& input_name, const XSI::CTime& eval_time);

// cyc_materials
// these methods used from other external files (for lights or osl)
void sync_float3_parameter(ccl::Scene* scene, 
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const std::string& cycles_name,
	const XSI::CTime& eval_time);
void sync_float_parameter(ccl::Scene* scene, 
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const std::string& cycles_name,
	const XSI::CTime& eval_time);

// cycles shader nodes
ccl::ShaderNode* sync_cycles_shader(ccl::Scene* scene, const XSI::Shader& xsi_shader, const XSI::CString& shader_type, const XSI::CParameterRefArray& xsi_parameters, const XSI::CTime& eval_time, ccl::ShaderGraph* shader_graph, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs);

// osl
ccl::ShaderNode* sync_osl_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray> &aovs, const XSI::CTime& eval_time);

// XSI native shaders
ccl::ShaderNode* sync_xsi_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, const XSI::CString& shader_type, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs, const XSI::CTime& eval_time);

// GLTF shaders
ccl::ShaderNode* sync_gltf_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, const XSI::CString& shader_type, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs, const XSI::CTime& eval_time);
