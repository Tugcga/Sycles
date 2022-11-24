#pragma once
#include <xsi_application.h>

// remove digits from the given string
XSI::CString remove_digits(const XSI::CString& orignal_str);

// replace each splitter symbol to replacer in he input string
XSI::CString replace_symbols(const XSI::CString& input_string, const XSI::CString& splitter, const XSI::CString& replacer);

// return common part (from the last \\ symbol) for input array of strings
// this method used for generating full path for combined and cryptomatte output
// we get all output pathes for images, select the common part and then attach "combined" or "cryptomatte"
XSI::CString get_common_substring(const XSI::CStringArray& strings_array);