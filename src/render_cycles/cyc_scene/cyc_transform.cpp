#include "scene/scene.h"
#include "scene/object.h"
#include "util/transform.h"

#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_matrix4.h>

#include "../update_context.h"
#include "../../utilities/math.h"

void sync_transform(ccl::Object* object, UpdateContext* update_context, const XSI::KinematicState& xsi_kine)
{
	// get curent transform matrix
	XSI::MATH::CMatrix4 xsi_matrix = xsi_kine.GetTransform(update_context->get_time()).GetMatrix4();
	ccl::Transform tfm = xsi_matrix_to_transform(xsi_matrix);

	object->set_tfm(tfm);

	// TODO: create motion
	// but it will be better at first implement plygon meshes
}

XSI::CStatus sync_geometry_transform(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject &xsi_object)
{
	ULONG xsi_id = xsi_object.GetObjectID();
	XSI::KinematicState xsi_kine = xsi_object.GetKinematics().GetGlobal();

	if (update_context->is_object_exists(xsi_id))
	{
		std::vector<size_t> indexes = update_context->get_object_cycles_indexes(xsi_id);
		for(size_t i = 0; i < indexes.size(); i++)
		{
			size_t index = indexes[i];
			ccl::Object* object = scene->objects[index];
			sync_transform(object, update_context, xsi_kine);
		}
	}
	else
	{
		//return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}