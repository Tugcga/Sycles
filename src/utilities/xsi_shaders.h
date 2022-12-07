#pragma once
#include <xsi_ref.h>
#include <xsi_shaderparameter.h>
#include <xsi_shader.h>

enum ShaderParameterType
{
	ParameterType_Unknown,
	ParameterType_Float,
	ParameterType_Integer,
	ParameterType_Boolean,
	ParameterType_String,
	ParameterType_Color3,
	ParameterType_Color4,
	ParameterType_Vector3
};

// return type of the shader parameter
ShaderParameterType get_shader_parameter_type(XSI::ShaderParameter& parameter);

// return shader parameter of the material root node, which has root_parameter_name name and connected to first-level node in the tree
// if there is no port with the name, return invalid parameter
XSI::ShaderParameter get_root_shader_parameter(const XSI::CRefArray& first_level_shaders, const XSI::CString& root_parameter_name, bool check_substring);

// return true if input shader node is compound
bool is_shader_compound(const XSI::Shader& shader);

// return shader node (may be inside compound), connected to the parameter
// parameter is a right port, shader node at the left
// *node*------>*parameter*
// if ignore_converters is true, then we should goes through color_to_sceler and scalar_to_color
XSI::Shader get_input_node(const XSI::ShaderParameter& parameter, bool ignore_converters = false);

// return parameter, connected to the input parameter of the shader node
// if it connect to the primitive node, then return the last parameter before this connection
// if it connect to compound input, then return this input
// go deeper through passthrough nodes
XSI::Parameter get_source_parameter(XSI::Parameter& parameter);

float get_float_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
int get_int_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
bool get_bool_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::CString get_string_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::MATH::CColor4f get_color_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::MATH::CVector3 get_vector_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);