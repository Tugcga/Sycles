#pragma once
#include "scene/scene.h"

#include <xsi_renderercontext.h>
#include <xsi_camera.h>

#include "../../render_cycles/update_context.h"
#include "../../render_base/type_enums.h"

void sync_scene(ccl::Scene* scene, UpdateContext* update_context);

// cyc_camera
void sync_camera(ccl::Scene* scene, UpdateContext* update_context);

// cyc_light
void sync_xsi_lights(ccl::Scene* scene, const std::vector<XSI::Light>& xsi_lights, UpdateContext* update_context);