#include "scene/scene.h"
#include "scene/object.h"
#include "util/transform.h"

#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_matrix4.h>

#include "../update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

void sync_transform(ccl::Object* object, UpdateContext* update_context, const XSI::KinematicState& xsi_kine)
{
	// get curent transform matrix
	XSI::MATH::CMatrix4 xsi_matrix = xsi_kine.GetTransform(update_context->get_time()).GetMatrix4();
	ccl::Transform tfm = xsi_matrix_to_transform(xsi_matrix);

	object->set_tfm(tfm);
	object->tag_tfm_modified();

	if (update_context->get_need_motion())
	{
		size_t motion_steps = update_context->get_motion_steps();
		ccl::array<ccl::Transform> motion_tfms;
		motion_tfms.resize(motion_steps, ccl::transform_empty());

		for (size_t i = 0; i < update_context->get_motion_steps(); i++)
		{
			float current_time = update_context->get_motion_time(i);
			ccl::Transform time_tfm = xsi_matrix_to_transform(xsi_kine.GetTransform(current_time).GetMatrix4());
			motion_tfms[i] = time_tfm;
		}
		
		object->set_motion(motion_tfms);
	}
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
			object->tag_update(scene);
		}
	}
	else
	{
		//return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}