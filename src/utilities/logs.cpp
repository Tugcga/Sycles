#include "util/array.h"
#include "util/types.h"

#include <xsi_string.h>
#include <xsi_application.h>
#include <xsi_color.h>
#include <xsi_color4f.h>
#include <xsi_vector3.h>
#include <xsi_floatarray.h>
#include <xsi_longarray.h>
#include <xsi_doublearray.h>
#include <xsi_iceattributedataarray.h>

#include <vector>
#include <string>
#include <set>

#include "../render_base/image_buffer.h"

void log_message(const XSI::CString &message, XSI::siSeverityType level)
{
	XSI::Application().LogMessage("[Cycles Render] " + message, level);
}

void log_warning(const XSI::CString& message) {
	XSI::Application().LogMessage("[Cycles Warning] " + message, XSI::siSeverityType::siWarningMsg);
}

XSI::CString to_string(const XSI::CFloatArray& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::CLongArray& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::CDoubleArray& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::CICEAttributeDataArrayVector3f& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = XSI::CString(array.GetCount()) + "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::CICEAttributeDataArrayFloat& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = XSI::CString(array.GetCount()) + "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<std::string>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0].c_str());
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString(array[i].c_str());
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<ULONG> &array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<LONG>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::set<ULONG>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[";
	for (auto it = array.begin(); it != array.end(); ++it)
	{
		to_return += XSI::CString(*it) + ", ";
	}
	to_return = to_return.GetSubString(0, to_return.Length() - 2);
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<unsigned int> &array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString((int)array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString((int)array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<int>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString((int)array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString((int)array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<float> &array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString((float)array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString((float)array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<unsigned short>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString((int)array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString((int)array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<double>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString((float)array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString((float)array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::CColor &color)
{
	return "(" + XSI::CString(color.r) + ", " + XSI::CString(color.g) + ", " + XSI::CString(color.b) + ", " + XSI::CString(color.a) + ")";
}

XSI::CString to_string(const XSI::MATH::CVector3 &vector)
{
	return "(" + XSI::CString(vector.GetX()) + ", " + XSI::CString(vector.GetY()) + ", " + XSI::CString(vector.GetZ()) + ")";
}

XSI::CString to_string(const XSI::MATH::CColor4f &color)
{
	return "(" + XSI::CString(color.GetR()) + ", " + XSI::CString(color.GetG()) + ", " + XSI::CString(color.GetB()) + ", " + XSI::CString(color.GetA()) + ")";
}

XSI::CString to_string(const XSI::CStringArray& array)
{
	if (array.GetCount() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + array[0];
	for (ULONG i = 1; i < array.GetCount(); i++)
	{
		to_return += ", " + array[i];
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const std::vector<size_t>& array)
{
	if (array.size() == 0)
	{
		return "[]";
	}

	XSI::CString to_return = "[" + XSI::CString(array[0]);
	for (ULONG i = 1; i < array.size(); i++)
	{
		to_return += ", " + XSI::CString(array[i]);
	}
	to_return += "]";
	return to_return;
}

XSI::CString to_string(const XSI::MATH::CMatrix4& matrix)
{
	XSI::CString to_return = "";
	for (ULONG i = 0; i < 4; i++)
	{
		for (ULONG j = 0; j < 4; j++)
		{
			to_return += XSI::CString(matrix.GetValue(i, j));
		}
		to_return += "\n";
	}

	return to_return;
}

XSI::CString to_string(const ccl::array<ccl::float2>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += "(" + XSI::CString(array[i].x) + ", " + XSI::CString(array[i].y) + ")" + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ccl::array<ccl::float3>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += "(" + XSI::CString(array[i].x) + ", " + XSI::CString(array[i].y) + ", " + XSI::CString(array[i].z) + ")" + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ccl::array<ccl::float4>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += "(" + XSI::CString(array[i].x) + ", " + XSI::CString(array[i].y) + ", " + XSI::CString(array[i].z) + ", " + XSI::CString(array[i].w) + ")" + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ccl::vector<ccl::float3>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += "(" + XSI::CString(array[i].x) + ", " + XSI::CString(array[i].y) + ", " + XSI::CString(array[i].z) + ")" + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ccl::vector<ccl::float4>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += "(" + XSI::CString(array[i].x) + ", " + XSI::CString(array[i].y) + ", " + XSI::CString(array[i].z) + "; " + XSI::CString(array[i].w) + ")" + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string_float4(const ccl::float4& value)
{
	return "(" + XSI::CString(value.x) + ", " + XSI::CString(value.y) + ", " + XSI::CString(value.z) + ", " + XSI::CString(value.w) + ")";
}

XSI::CString to_string(const ccl::array<int>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += XSI::CString(array[i]) + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ccl::vector<size_t>& array)
{
	XSI::CString to_return = "[";
	for (ULONG i = 0; i < array.size(); i++)
	{
		to_return += XSI::CString(array[i]) + ((i == array.size() - 1) ? "" : ", ");
	}

	to_return += "]";

	return to_return;
}

XSI::CString to_string(const ImageRectangle& rect) {
	return XSI::CString("(") + XSI::CString(rect.get_width()) + ", " + XSI::CString(rect.get_height()) + "; [" + XSI::CString(rect.get_x_start()) + ", " + XSI::CString(rect.get_y_start()) + "] - [" + XSI::CString(rect.get_x_end()) + ", " + XSI::CString(rect.get_y_end()) + "])";
}

XSI::CString to_string(const std::vector<XSI::CStringArray>& array) {
	XSI::CString to_return = "(";
	for (size_t i = 0; i < array.size(); i++) {
		XSI::CStringArray xsi_array = array[i];
		for (size_t j = 0; j < xsi_array.GetCount(); j++) {
			XSI::CString s = xsi_array[j];
			to_return += ", " + s;
		}
		to_return += (i == array.size() - 1) ? "" : "|";
	}

	to_return += ")";

	return to_return;
}

XSI::CString to_string_int2(const ccl::int2& value)
{
	return "(" + XSI::CString(value.x) + ", " + XSI::CString(value.y) + ")";
}

XSI::CString to_string_float3(const ccl::float3& value) {
	return "(" + XSI::CString(value.x) + ", " + XSI::CString(value.y) + ", " + XSI::CString(value.z) + ")";
}

XSI::CString bitmask_to_string(uint64_t mask)
{
	XSI::CString str = "";
	uint64_t value = mask;
	for (size_t i = 0; i < 64; i++)
	{
		str += XSI::CString(value % 2);
		value = value / 2;
	}

	// reverse output
	XSI::CString to_return = "";
	for (size_t i = 0; i < str.Length(); i++)
	{
		to_return += str[str.Length() - 1 - i];
	}

	return to_return;
}