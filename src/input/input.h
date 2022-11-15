#pragma once
#include <xsi_application.h>

#include "config_ini.h"
#include "config_ocio.h"

// plugin path stored when the plugin is loaded (before it initialized)
void set_plugin_path(const XSI::CString& input_plugin_path);
XSI::CString get_plugin_path();

// call find devices command when the plugin is initialized (nd render instance is created)
void find_devices();
XSI::CStringArray get_available_devices_names();

void read_config_ini();
InputConfig get_input_config();

void read_ocio_config();
OCIOConfig get_ocio_config();
