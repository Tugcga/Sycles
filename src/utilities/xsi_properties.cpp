#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_property.h>
#include <xsi_time.h>

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