#include <xsi_kinematics.h>

#include "scene/scene.h"
#include "scene/camera.h"
#include <xsi_camera.h>

#include "../../render_cycles/update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

std::vector<float> extract_camera_tfm(const XSI::Camera &xsi_camera)
{
	XSI::MATH::CMatrix4 camera_tfm_matrix = xsi_camera.GetKinematics().GetGlobal().GetTransform().GetMatrix4();
	std::vector<float> xsi_camera_tfm(16);
	xsi_camera_tfm[0] = camera_tfm_matrix.GetValue(0, 0);
	xsi_camera_tfm[1] = camera_tfm_matrix.GetValue(0, 1);
	xsi_camera_tfm[2] = camera_tfm_matrix.GetValue(0, 2);
	xsi_camera_tfm[3] = camera_tfm_matrix.GetValue(0, 3);

	xsi_camera_tfm[4] = camera_tfm_matrix.GetValue(1, 0);
	xsi_camera_tfm[5] = camera_tfm_matrix.GetValue(1, 1);
	xsi_camera_tfm[6] = camera_tfm_matrix.GetValue(1, 2);
	xsi_camera_tfm[7] = camera_tfm_matrix.GetValue(1, 3);

	xsi_camera_tfm[8] = -1 * camera_tfm_matrix.GetValue(2, 0);
	xsi_camera_tfm[9] = -1 * camera_tfm_matrix.GetValue(2, 1);
	xsi_camera_tfm[10] = -1 * camera_tfm_matrix.GetValue(2, 2);
	xsi_camera_tfm[11] = -1 * camera_tfm_matrix.GetValue(2, 3);

	xsi_camera_tfm[12] = camera_tfm_matrix.GetValue(3, 0);
	xsi_camera_tfm[13] = camera_tfm_matrix.GetValue(3, 1);
	xsi_camera_tfm[14] = camera_tfm_matrix.GetValue(3, 2);
	xsi_camera_tfm[15] = camera_tfm_matrix.GetValue(3, 3);

	return xsi_camera_tfm;
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
			}
			else
			{
				fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) / camera_aspect));
			}
		}
	}
	float fov_rad = DEG2RADF(fov_grad);

	// clipping distances, for perspective/orthographic only
	camera->set_nearclip(near_clip);
	camera->set_farclip(far_clip);

	// camera type
	camera->set_camera_type(is_ortho ? ccl::CAMERA_ORTHOGRAPHIC : ccl::CAMERA_PERSPECTIVE);

	// transforms
	std::vector<float> camera_tfm = extract_camera_tfm(xsi_camera);
	camera->set_matrix(get_transform(camera_tfm));

	// fov
	camera->set_fov(fov_rad);

	// set curent camera as dicing camera
	*scene->dicing_camera = *camera;

	camera->tag_full_width_modified();
	camera->tag_full_height_modified();
	camera->tag_matrix_modified();
	camera->tag_fov_modified();

	camera->update(scene);
}