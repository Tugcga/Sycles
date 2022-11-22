#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_pluginregistrar.h>
#include <xsi_status.h>

#include "version.h"
#include "input/input.h"

SICALLBACK XSILoadPlugin(XSI::PluginRegistrar& in_reg)
{
	//add plugin directory to the PATH, because some apps require it for loading libraries
#ifdef _WINDOWS
	// get plugin_path and remove trailing slash
	XSI::CString plugin_path = in_reg.GetOriginPath();
	plugin_path.TrimRight("\\");

	// save plugin path into global variable to use it later
	set_plugin_path(plugin_path);

	// get PATH env
	char* pValue;
	size_t envBufSize;
	int err = _dupenv_s(&pValue, &envBufSize, "PATH");
	if (err)
	{
		XSI::Application().LogMessage("Failed to retrieve PATH environment.", XSI::siErrorMsg);
	}
	else
	{
		const XSI::CString currentPath = pValue;
		free(pValue);

		// check so that plugin_path isn't already in PATH
		if (currentPath.FindString(plugin_path) == UINT_MAX)
		{
			// add plugin_path to beginning of PATH
			XSI::CString envPath = plugin_path + ";" + currentPath;

			// set the new path
			err = _putenv_s("PATH", envPath.GetAsciiString());
			if (err)
			{
				XSI::Application().LogMessage("Failed to add Cycles Render path to PATH environment.", XSI::siErrorMsg);
			}
		}
	}
#endif
	in_reg.PutAuthor("Shekn");
	in_reg.PutName("Cycles Render");
	in_reg.PutVersion(get_major_version(), get_minor_version());
	//RegistrationInsertionPoint - do not remove this line
	in_reg.RegisterRenderer("Cycles Render");           // The renderer
	in_reg.RegisterProperty("Cycles Render Options");   // Render options

	in_reg.RegisterEvent("Cyc_OnObjectAdded", XSI::siOnObjectAdded);
	in_reg.RegisterEvent("Cyc_OnObjectRemoved", XSI::siOnObjectRemoved);

	return XSI::CStatus::OK;
}

SICALLBACK XSIUnloadPlugin(const XSI::PluginRegistrar& in_reg)
{
	XSI::CString strPluginName;
	strPluginName = in_reg.GetName();
	XSI::Application().LogMessage(strPluginName + " has been unloaded.", XSI::siVerboseMsg);

	return XSI::CStatus::OK;
}
