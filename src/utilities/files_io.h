#pragma once
#include <string>
#include <map>

#include <xsi_application.h>

bool create_dir(const std::string& file_path);
XSI::CString create_temp_path();
void remove_temp_path(const XSI::CString& temp_path);

// kye - tile number, value - full path to the image
// input image_path is a path to selected image
std::map<int, XSI::CString> sync_image_tiles(const XSI::CString& image_path);

std::vector<float> load_image(const XSI::CString& file_path, ULONG& out_width, ULONG& out_height, ULONG& out_channels, bool &out_sucess);
bool is_ext_ldr(std::string ext);
bool is_output_extension_supported(const XSI::CString &extension);