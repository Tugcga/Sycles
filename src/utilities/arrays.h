#pragma once

#include <xsi_x3dobject.h>

#include <vector>
#include <string>

//return true if value in the array
bool is_contains(const std::vector<std::string>& array, const std::string &value);
bool is_contains(const std::vector<XSI::CString>& array, const XSI::CString& value);
bool is_contains(const std::vector<ULONG> &array, const ULONG value);
bool is_contains(const XSI::CRefArray &array, const XSI::CRef &object);

//return -1, if value is not in array, otherwise return the index
int get_index_in_array(const std::vector<ULONG> &array, const ULONG value);
int get_index_in_array(const std::vector<std::string>& array, const std::string& value);

float get_maximum(const std::vector<float> &array);