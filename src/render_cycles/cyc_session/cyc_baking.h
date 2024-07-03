#pragma once
#include <xsi_x3dobject.h>

#include "util/types.h"
#include "scene/pass.h"
#include "scene/scene.h"

#include <vector>

#include "../update_context.h"
#include "../../render_base/image_buffer.h"

class BakingContext
{
public:
	BakingContext();
	~BakingContext();

	void reset();
	void setup(ULONG in_width, ULONG in_height);
	void set(int x, int y, int seed, int primitive_id, ccl::float2 uv, float du_dx, float du_dy, float dv_dx, float dv_dy);

	ImageBuffer* get_buffer_primitive_id();
	ImageBuffer* get_buffer_differencial();
	ImageBuffer* get_buffer_seed();

	void make_invalid();
	void make_valid();
	bool get_is_valid();
	void set_keys(bool is_direct, bool is_indirect, bool is_color, bool is_diffuse, bool is_glossy, bool is_transmission, bool is_emit);
	bool get_key_is_direct();
	bool get_key_is_indirect();
	bool get_key_is_color();
	bool get_key_is_diffuse();
	bool get_key_is_glossy();
	bool get_key_is_transmission();
	bool get_key_is_emit();
	void set_pass_type(ccl::PassType new_pass_type);
	ccl::PassType get_pass_type();
	void set_use_camera(bool value);
	bool get_use_camera();

	ULONG get_width();
	ULONG get_height();

private:
	ULONG width, height;

	ImageBuffer* buffer_primitive_id;
	ImageBuffer* buffer_differencial;
	ImageBuffer* buffer_seed;

	bool is_valid;
	ccl::PassType pass_type;

	bool key_is_direct;
	bool key_is_indirect;
	bool key_is_color;
	bool key_is_diffuse;
	bool key_is_glossy;
	bool key_is_transmission;
	bool key_is_emit;
	bool use_camera;
};

// outclass functions
void sync_baking(ccl::Scene* scene, UpdateContext* update_context, BakingContext* baking_context, XSI::X3DObject &bake_object, const XSI::CString &baking_uv_name, ULONG bake_width, ULONG bake_height);