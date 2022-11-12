#pragma once

#include <xsi_application.h>
#include <xsi_string.h>
#include <xsi_color.h>
#include <xsi_color4f.h>
#include <xsi_doublearray.h>

#include <vector>
#include <string>
#include <set>

//output the message to the console
void log_message(const XSI::CString &message, XSI::siSeverityType level = XSI::siSeverityType::siInfoMsg);

//convert data to string
XSI::CString to_string(const XSI::CFloatArray& array);
XSI::CString to_string(const XSI::CLongArray& array);
XSI::CString to_string(const XSI::CDoubleArray& array);
XSI::CString to_string(const std::vector<std::string>& array);
XSI::CString to_string(const std::vector<ULONG> &array);
XSI::CString to_string(const std::set<ULONG>& array);
XSI::CString to_string(const std::vector<unsigned int> &array);
XSI::CString to_string(const std::vector<float> &array);
XSI::CString to_string(const std::vector<unsigned short>& array);
XSI::CString to_string(const std::vector<double>& array);
XSI::CString to_string(const XSI::CColor &color);
XSI::CString to_string(const XSI::MATH::CColor4f &color);
XSI::CString to_string(const XSI::MATH::CVector3 &vector);
XSI::CString to_string(const XSI::CStringArray &array);

//remove digits from the given string
XSI::CString remove_digits(const XSI::CString& orignal_str);

//replace each splitter symbol to replacer in he input string
XSI::CString replace_symbols(const XSI::CString& input_string, const XSI::CString& splitter, const XSI::CString& replacer);