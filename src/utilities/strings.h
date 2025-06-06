#pragma once
#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_polygonmesh.h>
#include <xsi_hairprimitive.h>
#include <xsi_primitive.h>
#include <xsi_x3dobject.h>

#include <vector>
#include <string>

#include "util/array.h"
#include "OpenImageIO/ustring.h"

// remove digits from the given string
XSI::CString remove_digits(const XSI::CString& orignal_str);

// replace each splitter symbol to replacer in he input string
XSI::CString replace_symbols(const XSI::CString& input_string, const XSI::CString& splitter, const XSI::CString& replacer);

// return common part (from the last \\ symbol) for input array of strings
// this method used for generating full path for combined and cryptomatte output
// we get all output pathes for images, select the common part and then attach "combined" or "cryptomatte"
XSI::CString get_common_substring(const XSI::CStringArray& strings_array);

// return a new path of the image to save output aov
// the name adds at the end before the dot and frame
// if inputs are D:\images\pass.1.png aov_name then the result will be
// D:\images\pass.aov_name.1.png
XSI::CString add_aov_name_to_path(const XSI::CString &file_path, const XSI::CString &aov_name);

std::vector<size_t> get_symbol_positions(const XSI::CString& string, char symbol);

ccl::array<int> string_to_array(const XSI::CString& string);
std::string build_source_image_path(const XSI::CString& path, const XSI::CString& source_type, bool is_cyclic, int sequence_start, int sequence_frames, int sequence_offset, const XSI::CTime& eval_time, bool allow_tile, bool& change_to_udims);

std::string replace_all_substrings(const std::string& input_string, const std::string& what_part, const std::string& replace_part);
// return true if input string ends with a given fragment
bool is_ends_with(const OIIO::ustring& input_string, const XSI::CString& end_fragment);
// return true if str starts with prefix
bool is_start_from(const OIIO::ustring& str, const OIIO::ustring& prefix);
XSI::CString vdbprimitive_inputs_to_path(const XSI::CParameterRefArray& params, const XSI::CTime& eval_time);

// these functions used for define Cycles geometry name from different types of XSI objects (for example for mesh->name)
XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::PolygonMesh& xsi_polymesh);
XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::HairPrimitive& xsi_hair);
XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::Primitive& xsi_primitive);
XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::CString& name);