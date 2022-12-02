#pragma once
#include <string>

#include <xsi_application.h>

bool create_dir(const std::string& file_path);
XSI::CString create_temp_path();
void remove_temp_path(const XSI::CString& temp_path);