#include <xsi_x3dobject.h>

#include "../../../render_base/type_enums.h"
#include "cyc_geometry.h"

PointcloudType get_pointcloud_type(const XSI::X3DObject &xsi_object)
{
	if (is_pointcloud_strands(xsi_object))
	{
		return PointcloudType::PointcloudType_Strands;
	}
	else
	{
		return PointcloudType::PointcloudType_Unknown;
	}
}