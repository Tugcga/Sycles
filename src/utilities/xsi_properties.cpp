#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_property.h>
#include <xsi_time.h>
#include <xsi_status.h>
#include <xsi_shader.h>
#include <xsi_material.h>
#include <xsi_arrayparameter.h>
#include <xsi_parameter.h>
#include <xsi_kinematics.h>

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

bool obtain_subsub_directions(const XSI::Shader &xsi_shader, float &sun_x, float &sun_y, float &sun_z)
{
	XSI::CRef parent = xsi_shader.GetParent();
	XSI::Material parent_material(parent);
	XSI::CRefArray obj_used_shader = parent_material.GetUsedBy();
	if (obj_used_shader.GetCount() > 0)
	{
		for (size_t obj_index = 0; obj_index < obj_used_shader.GetCount(); obj_index++)
		{
			XSI::X3DObject xsi_object(obj_used_shader[obj_index]);
			XSI::CRefArray children = xsi_object.GetChildren();
			for (size_t i = 0; i < children.GetCount(); i++)
			{
				XSI::X3DObject c = children.GetItem(i);
				XSI::CParameterRefArray params = c.GetParameters();
				XSI::Parameter lightType = params.GetItem("cycles_primitive_type");
				XSI::CValue light_type = lightType.GetValue();
				if (!light_type.IsEmpty())
				{
					XSI::CString type_str(light_type);
					if (type_str == "light_sun")
					{
						XSI::MATH::CMatrix4 light_transform = c.GetKinematics().GetGlobal().GetTransform().GetMatrix4();
						sun_x = light_transform.GetValue(2, 0);
						sun_y = light_transform.GetValue(2, 1);
						sun_z = light_transform.GetValue(2, 2);
						return true;
					}
				}
			}
		}
	}

	return false;
}