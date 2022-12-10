#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_property.h>
#include <xsi_time.h>
#include <xsi_status.h>

bool is_render_visible(XSI::X3DObject& xsi_object, const XSI::CTime &eval_time)
{
	XSI::Property visibility_prop;
	xsi_object.GetPropertyFromName("visibility", visibility_prop);
	if (visibility_prop.IsValid())
	{
		return visibility_prop.GetParameterValue("rendvis", eval_time);
	}
	return false;
}

bool get_xsi_object_property(XSI::X3DObject &xsi_object, const XSI::CString &property_name, XSI::Property &out_property)
{
	XSI::CStatus is_get = xsi_object.GetPropertyFromName(property_name, out_property);
	if (out_property.IsValid())
	{
		return true;
	}
	else
	{
		return false;
	}
}