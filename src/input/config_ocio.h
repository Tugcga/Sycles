#pragma once
#include <vector>
#include <xsi_application.h>

#include "OpenColorIO/OpenColorIO.h"
namespace OCIO = OCIO_NAMESPACE;

struct OCIODisplay
{
	XSI::CString name;
	size_t views_count;
	std::vector<XSI::CString> views;
};

struct OCIOConfig
{
	bool is_init;
	bool is_file_exist;
	OCIO::ConstConfigRcPtr config;
	XSI::CString config_file_path;
	size_t displays_count;
	std::vector<OCIODisplay> displays;
	size_t looks_count;
	std::vector<XSI::CString> looks;

	size_t default_display;
	size_t default_view;
	size_t default_look;
};