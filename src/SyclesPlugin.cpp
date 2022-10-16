#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_pluginregistrar.h>
#include <xsi_status.h>

#include "device/device.h"

using namespace XSI; 

SICALLBACK XSILoadPlugin(PluginRegistrar& in_reg)
{
	in_reg.PutAuthor("Shekn");
	in_reg.PutName("SyclesPlugin");
	in_reg.PutVersion(1, 0);
	//RegistrationInsertionPoint - do not remove this line

	ccl::vector<ccl::DeviceInfo> cpu_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_CPU);
	for (int i = 0; i < cpu_devices.size(); i++) {
		Application().LogMessage("cpu: " + CString(cpu_devices[i].description.c_str()));
	}

	return CStatus::OK;
}

SICALLBACK XSIUnloadPlugin(const PluginRegistrar& in_reg)
{
	CString strPluginName;
	strPluginName = in_reg.GetName();
	Application().LogMessage(strPluginName + " has been unloaded.", siVerboseMsg);
	return CStatus::OK;
}