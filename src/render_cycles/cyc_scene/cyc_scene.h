#pragma once
#include "scene/scene.h"

#include <xsi_renderercontext.h>
#include <xsi_camera.h>

#include "../../render_cycles/update_context.h"

void sync_scene(ccl::Scene* scene, UpdateContext* update_context);

// cyc_camera
void sync_camera(ccl::Scene* scene, UpdateContext* update_context);