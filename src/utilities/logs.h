#pragma once
#include "util/array.h"
#include "util/types.h"

#include <xsi_application.h>
#include <xsi_string.h>
#include <xsi_color.h>
#include <xsi_color4f.h>
#include <xsi_doublearray.h>
#include <xsi_iceattributedataarray.h>

#include <vector>
#include <string>
#include <set>

#include "../render_base/image_buffer.h"

// output the message to the console
void log_message(const XSI::CString &message, XSI::siSeverityType level = XSI::siSeverityType::siInfoMsg);
void log_warning(const XSI::CString& message);

// convert data to string
XSI::CString to_string(const XSI::CFloatArray& array);
XSI::CString to_string(const XSI::CLongArray& array);
XSI::CString to_string(const XSI::CDoubleArray& array);
XSI::CString to_string(const XSI::CICEAttributeDataArrayVector3f& array);
XSI::CString to_string(const XSI::CICEAttributeDataArrayFloat& array);
XSI::CString to_string(const std::vector<std::string>& array);
XSI::CString to_string(const std::vector<ULONG> &array);
XSI::CString to_string(const std::vector<LONG>& array);
XSI::CString to_string(const std::set<ULONG>& array);
XSI::CString to_string(const std::vector<unsigned int> &array);
XSI::CString to_string(const std::vector<int>& array);
XSI::CString to_string(const std::vector<float> &array);
XSI::CString to_string(const std::vector<unsigned short>& array);
XSI::CString to_string(const std::vector<double>& array);
XSI::CString to_string(const XSI::CColor &color);
XSI::CString to_string(const XSI::MATH::CColor4f &color);
XSI::CString to_string(const XSI::MATH::CVector3 &vector);
XSI::CString to_string(const XSI::CStringArray &array);
XSI::CString to_string(const std::vector<size_t>& array);
XSI::CString to_string(const XSI::MATH::CMatrix4& matrix);
XSI::CString to_string(const ccl::array<ccl::float2> &array);
XSI::CString to_string(const ccl::array<ccl::float3>& array);
XSI::CString to_string(const ccl::array<ccl::float4>& array);
XSI::CString to_string(const ccl::vector<ccl::float3>& array);
XSI::CString to_string(const ccl::vector<ccl::float4>& array);
XSI::CString to_string_float4(const ccl::float4 &value);  // to_string name exists in ccl namespace
XSI::CString to_string(const ccl::array<int>& array);
XSI::CString to_string(const ccl::vector<size_t>& array);
XSI::CString to_string(const ImageRectangle& rect);
XSI::CString to_string(const std::vector<XSI::CStringArray>& array);
XSI::CString to_string_int2(const ccl::int2 &value);
XSI::CString to_string_float3(const ccl::float3& value);

XSI::CString bitmask_to_string(uint64_t mask);