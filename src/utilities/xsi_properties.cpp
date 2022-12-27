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
#include <xsi_light.h>
#include <xsi_model.h>
#include <xsi_vector3.h>

#include <string>

#include "logs.h"
#include "math.h"

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

bool obtain_subsub_directions(const XSI::Shader &xsi_shader, float &sun_x, float &sun_y, float &sun_z, const XSI::CTime &eval_time)
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
				if (is_render_visible(c, eval_time))
				{
					bool use_native_sun = false;
					XSI::siClassID c_class = c.GetClassID();
					if (c_class == XSI::siClassID::siLightID)
					{
						XSI::Light c_light(c);
						int xsi_light_type = c_light.GetParameterValue("Type", eval_time);
						if (xsi_light_type == 1)
						{
							use_native_sun = true;
						}
					}

					XSI::CParameterRefArray params = c.GetParameters();
					XSI::Parameter lightType = params.GetItem("cycles_primitive_type");

					XSI::CValue light_type = lightType.GetValue();
					if (use_native_sun || !light_type.IsEmpty())
					{
						XSI::CString type_str(light_type);
						if (use_native_sun || type_str == "light_sun")
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
	}

	return false;
}

// TODO: this function should return the first model, contained input object
// if this model is root, then return the name of the object
std::string get_asset_name(const XSI::X3DObject& xsi_object)
{
	XSI::Model root = XSI::Application().GetActiveSceneRoot();
	if (xsi_object.IsEqualTo(root))
	{
		return xsi_object.GetName().GetAsciiString();
	}

	XSI::X3DObject current = xsi_object;
	while (!current.GetParent3DObject().IsEqualTo(root) && !current.GetParent3DObject().IsEqualTo(current))
	{
		current = current.GetParent3DObject();
	}
	return current.GetName().GetAsciiString();
}

XSI::MATH::CVector3 get_object_color(XSI::X3DObject& xsi_object, const XSI::CTime& eval_time)
{
	XSI::Property display_property;
	XSI::CStatus display_status = xsi_object.GetPropertyFromName("Display", display_property);
	if (display_status.Succeeded())
	{
		float display_r = display_property.GetParameterValue("wirecolorr", eval_time);
		float display_g = display_property.GetParameterValue("wirecolorg", eval_time);
		float display_b = display_property.GetParameterValue("wirecolorb", eval_time);
		return XSI::MATH::CVector3(srgb_to_linear(display_r), srgb_to_linear(display_g), srgb_to_linear(display_b));
	}
	else
	{
		return XSI::MATH::CVector3(0.0f, 0.0f, 0.0f);
	}
}