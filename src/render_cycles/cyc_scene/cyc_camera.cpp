#include <xsi_kinematics.h>

#include "scene/scene.h"
#include "scene/camera.h"
#include <xsi_camera.h>

#include "../../render_cycles/update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

static ccl::Transform tweak_camera_matrix(const ccl::Transform& tfm, const ccl::CameraType type, const ccl::PanoramaType panorama_type)
{
	ccl::Transform result;

	if (type == ccl::CAMERA_PANORAMA)
	{
		if (panorama_type == ccl::PANORAMA_MIRRORBALL)
		{
			result = tfm * ccl::make_transform(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, -1.0f, 0.0f, 0.0f);
		}
		else
		{
			result = tfm * ccl::make_transform(
				0.0f, -1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				1.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	else
	{
		result = tfm * ccl::transform_scale(1.0f, 1.0f, 1.0f);
	}

	return transform_clear_scale(result);
}

std::vector<float> extract_camera_tfm(const XSI::Camera &xsi_camera)
{
	XSI::MATH::CMatrix4 camera_tfm_matrix = xsi_camera.GetKinematics().GetGlobal().GetTransform().GetMatrix4();
	std::vector<float> xsi_camera_tfm(16);
	xsi_matrix_to_cycles_array(xsi_camera_tfm, camera_tfm_matrix, true);

	return xsi_camera_tfm;
}

void sync_camera_motion(ccl::Scene* scene, UpdateContext* update_context, float fov_prev, float fov_next)
{
	ccl::Camera* camera = scene->camera;
	if (update_context->get_need_motion())
	{
		ccl::array<ccl::Transform> motions;
		motions.resize(update_context->get_motion_steps(), camera->get_matrix());

		// setup camera motion properties
		MotionPosition motion_position = update_context->get_motion_position();
		float shutter_time = update_context->get_motion_shutter_time();
		camera->set_motion_position(motion_position == MotionPosition_Start ? ccl::MOTION_POSITION_START : (motion_position == MotionPosition_Center ? ccl::MOTION_POSITION_CENTER : ccl::MOTION_POSITION_END));
		camera->set_shuttertime(shutter_time);
		camera->set_rolling_shutter_type(update_context->get_motion_rolling() ? ccl::Camera::RollingShutterType::ROLLING_SHUTTER_TOP : ccl::Camera::RollingShutterType::ROLLING_SHUTTER_NONE);
		camera->set_rolling_shutter_duration(update_context->get_motion_rolling_duration());

		XSI::Camera xsi_camera = update_context->get_camera();
		XSI::KinematicState camera_kinematic = xsi_camera.GetKinematics().GetGlobal();
		std::vector<float> time_array(16, 0.0f);
		for (size_t i = 0; i < update_context->get_motion_steps(); i++)
		{
			float current_time = update_context->get_motion_time(i);
			xsi_matrix_to_cycles_array(time_array, camera_kinematic.GetTransform(current_time).GetMatrix4(), true);
			ccl::Transform time_tfm = tweak_camera_matrix(get_transform(time_array), scene->camera->get_camera_type(), scene->camera->get_panorama_type());
			motions[i] = time_tfm;
		}
		time_array.clear();
		time_array.shrink_to_fit();

		scene->camera->set_motion(motions);

		scene->camera->set_use_perspective_motion(false);
		if (scene->camera->get_camera_type() == ccl::CAMERA_PERSPECTIVE)
		{
			// set previous and next fov
			scene->camera->set_fov_pre(fov_prev);
			scene->camera->set_fov_post(fov_next);
			if (scene->camera->get_fov_pre() != scene->camera->get_fov() || scene->camera->get_fov_post() != scene->camera->get_fov())
			{
				scene->camera->set_use_perspective_motion(true);
			}
		}
	}
	else
	{
		ccl::array<ccl::Transform> motions;
		motions.resize(1, scene->camera->get_matrix());
		scene->camera->set_motion(motions);

		scene->camera->set_use_perspective_motion(false);
		scene->camera->set_fov_pre(scene->camera->get_fov());
		scene->camera->set_fov_post(scene->camera->get_fov());
	}
}

void sync_camera(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::Camera xsi_camera = update_context->get_camera();
	XSI::CTime eval_time = update_context->get_time();

	ccl::Camera* camera = scene->camera;

	camera->set_full_width(update_context->get_full_width());
	camera->set_full_height(update_context->get_full_height());

	// now our camera is perspective/orthographic, so it can not be panoramic
	// but for panoramic camera setup is slightly different
	 
	float camera_aspect = float(xsi_camera.GetParameterValue("aspect", eval_time));
	float fov_grad = float(xsi_camera.GetParameterValue("fov", eval_time));
	float fov_prev_grad = float(xsi_camera.GetParameterValue("fov", update_context->get_motion_fisrt_time()));
	float fov_next_grad = float(xsi_camera.GetParameterValue("fov", update_context->get_motion_last_time()));
	float near_clip = xsi_camera.GetParameterValue("near", eval_time);
	float far_clip = xsi_camera.GetParameterValue("far", eval_time);
	bool is_ortho = xsi_camera.GetParameterValue("proj") == 0;

	// check is camera orthographic
	if (is_ortho)
	{// orthographic
		float ortho_aspect = float(xsi_camera.GetParameterValue("orthoheight", eval_time)) * camera_aspect / 2;
		camera->viewplane.left = -1 * ortho_aspect;
		camera->viewplane.right = ortho_aspect;
		camera->viewplane.bottom = -1 * ortho_aspect / camera_aspect;
		camera->viewplane.top = ortho_aspect / camera_aspect;
	}
	else
	{// perspective
		camera->compute_auto_viewplane();

		bool is_horizontal = xsi_camera.GetParameterValue("fovtype", eval_time) == 1;
		if ((is_horizontal || camera_aspect <= 1) && (!is_horizontal || camera_aspect >= 1))
		{
			if (camera_aspect <= 1)
			{
				fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) * camera_aspect));
				fov_prev_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_prev_grad) / 2.0) * camera_aspect));
				fov_next_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_next_grad) / 2.0) * camera_aspect));
			}
			else
			{
				fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) / camera_aspect));
				fov_prev_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_prev_grad) / 2.0) / camera_aspect));
				fov_next_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_next_grad) / 2.0) / camera_aspect));
			}
		}
	}
	float fov_rad = DEG2RADF(fov_grad);
	float fov_prev_rad = DEG2RADF(fov_prev_grad);
	float fov_next_rad = DEG2RADF(fov_next_grad);

	// clipping distances, for perspective/orthographic only
	camera->set_nearclip(near_clip);
	camera->set_farclip(far_clip);

	// camera type
	camera->set_camera_type(is_ortho ? ccl::CAMERA_ORTHOGRAPHIC : ccl::CAMERA_PERSPECTIVE);

	// transforms
	std::vector<float> camera_tfm = extract_camera_tfm(xsi_camera);
	camera->set_matrix(tweak_camera_matrix(get_transform(camera_tfm), scene->camera->get_camera_type(), scene->camera->get_panorama_type()));

	// fov
	camera->set_fov(fov_rad);

	// set curent camera as dicing camera
	*scene->dicing_camera = *camera;

	sync_camera_motion(scene, update_context, fov_prev_rad, fov_next_rad);

	camera->tag_full_width_modified();
	camera->tag_full_height_modified();
	camera->tag_matrix_modified();
	camera->tag_fov_modified();

	camera->update(scene);
}