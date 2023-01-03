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

		for (size_t i = 0; i < motion_steps; i++)
		{
			float current_time = update_context->get_motion_time(i);
			ccl::Transform time_tfm = xsi_matrix_to_transform(xsi_kine.GetTransform(current_time).GetMatrix4());
			motion_tfms[i] = time_tfm;
		}
		
		object->set_motion(motion_tfms);
		object->tag_motion_modified();
	}
}

void sync_transforms(ccl::Object* object, const std::vector<XSI::MATH::CTransformation> &xsi_tfms_array, size_t main_motion_step)
{
	ccl::Transform tfm = xsi_matrix_to_transform(xsi_tfms_array[main_motion_step].GetMatrix4());
	
	if (xsi_tfms_array.size() > 1)
	{// motion transfroms
		size_t motion_steps = xsi_tfms_array.size();
		ccl::array<ccl::Transform> motion_tfms;
		motion_tfms.resize(motion_steps, ccl::transform_empty());

		for (size_t i = 0; i < motion_steps; i++)
		{
			ccl::Transform time_tfm = xsi_matrix_to_transform(xsi_tfms_array[i].GetMatrix4());
			motion_tfms[i] = time_tfm;
		}

		object->set_motion(motion_tfms);
		object->tag_motion_modified();
	}

	object->set_tfm(tfm);
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
		
	}

	return XSI::CStatus::OK;
}

std::vector<XSI::MATH::CTransformation> build_transforms_array(const XSI::KinematicState &xsi_kine, bool need_motion, const std::vector<float> &motion_times, const XSI::CTime &eval_time)
{
	if (need_motion)
	{
		std::vector<XSI::MATH::CTransformation> to_return(motion_times.size(), XSI::MATH::CTransformation());
		for (size_t i = 0; i < motion_times.size(); i++)
		{
			to_return[i] = xsi_kine.GetTransform(motion_times[i]);
		}
		return to_return;
	}
	else
	{
		return { xsi_kine.GetTransform(eval_time) };
	}
}