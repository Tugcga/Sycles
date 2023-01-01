#include <xsi_x3dobject.h>

#include "../../../render_base/type_enums.h"
#include "cyc_geometry.h"

PointcloudType get_pointcloud_type(XSI::X3DObject &xsi_object, const XSI::CTime& eval_time)
{
	if (is_pointcloud_points(xsi_object, eval_time))
	{
		return PointcloudType::PointcloudType_Points;
	}
	else if (is_pointcloud_strands(xsi_object))
	{
		return PointcloudType::PointcloudType_Strands;
	}
	else
	{
		return PointcloudType::PointcloudType_Unknown;
	}
}