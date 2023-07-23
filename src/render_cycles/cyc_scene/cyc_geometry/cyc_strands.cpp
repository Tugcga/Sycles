#include "scene/scene.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_geometry.h>
#include <xsi_primitive.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_material.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "../../update_context.h"
#include "cyc_geometry.h"
#include "../cyc_scene.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/xsi_properties.h"

bool is_pointcloud_strands(const XSI::X3DObject& xsi_object)
{
	XSI::Geometry xsi_geometry = xsi_object.GetActivePrimitive().GetGeometry();
	XSI::ICEAttribute strand_count_attr = xsi_geometry.GetICEAttributeFromName("StrandCount");
	XSI::ICEAttribute strand_position_attr = xsi_geometry.GetICEAttributeFromName("StrandPosition");
	XSI::ICEAttribute point_position_attr = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::CICEAttributeDataArray2DVector3f strand_position_data;
	strand_position_attr.GetDataArray2D(strand_position_data);

	if (strand_position_data.GetCount() > 0 && strand_count_attr.GetElementCount() > 0 && strand_position_attr.GetElementCount() > 0 && point_position_attr.GetElementCount() > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

float calculate_strand_radius(const ULONG point_index,  // the index of the point in the pointcloud
	const XSI::CICEAttributeDataArrayFloat &point_size,  // array of point radiuses
	const ULONG point_size_length,  // the size of the array with point radiuses
	const ULONG strand_index,  // index of the strand knot in the current point, for initial point it equals to 0, for last knot it equals to strand_positions_count
	const ULONG strand_length,  // the length of the strand (the number of knots with the start one), so, for strand *---*---*, where the first * is point, strand_length = 3 (and strand_positions_count = 2)
	const XSI::CICEAttributeDataArrayFloat &strand_size,  // array of size values for a given strand
	const ULONG strand_size_length  // the number of elements of the size array for strand (it can be different from strand_length)
)
{
	// if strand_size_length = 0, then use point radius
	// laso return point radius in the case when the strand has zero knots (strand_length = konts count + 1)
	if (strand_size_length == 0 || strand_length <= 1)
	{
		// return either 0.0 (if the array with sizes is empty)
		// or last value of the array (if point index greater the the length)
		// or actual value in the array
		return point_size_length > 0 ? point_size[point_index < point_size_length ? point_index : point_size_length - 1] : 0.0;
	}

	// if there is only one value in strand sizes, then return it
	if (strand_size_length == 1)
	{
		return strand_size[0];
	}

	float p = (float)strand_index / (float)(strand_length - 1);  // p from 0.0 (for initial point) to 1.0 (for end tip of the strand)
	float l = 1.0f / (float)(strand_size_length - 1);  // the length of one segment in the sizes array (for *---*---*---* l = 1/3)
	int interval_index = (int)(p / l);  // index of the interval, where the knot is, so, we should return the lerp of end points of this interval

	// check is the interval index is correct
	if (interval_index <= 0) { return strand_size[0]; }
	if (interval_index + 1 >= strand_size_length) { return strand_size[strand_size_length - 1]; }

	float delta = p - (float)interval_index * l;  // delta between interval start and the point (*-delta-|-----*)
	float c = delta / l;  // lerp coefficient
	// clamp lerp coefficient
	if (c <= 0.0f) { c = 0.0f; }
	else if (c >= 1.0f) { c = 1.0f; }

	return (1.0f - c) * strand_size[interval_index] + c * strand_size[interval_index + 1];
}

void sync_strands_geom(ccl::Scene* scene, 
	ccl::Hair* strands_geom,
	UpdateContext* update_context, 
	const XSI::Primitive& xsi_primitive, 
	bool use_motion_blur, 
	// next three arrays comes with zero size
	ccl::vector<ccl::float4>& out_original_positions,  // total positions and sizes of current frame strands, one plain array, the split template into individual curves stores in the geometry object
	ccl::vector<size_t> &out_strand_points,  // store here point indices with non-empty strands (the length of strand positions is non-zero), all other points should be ignored
	ccl::vector<size_t> &out_strand_lengths  // store here strand sizes (the length of strand position attribute), consider only non-zero strands
)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);

	// get strands data from ICE
	XSI::ICEAttribute point_position_attr = xsi_geometry.GetICEAttributeFromName("PointPosition");
	XSI::ICEAttribute strand_position_attr = xsi_geometry.GetICEAttributeFromName("StrandPosition");
	XSI::ICEAttribute size_attr = xsi_geometry.GetICEAttributeFromName("Size");
	XSI::ICEAttribute strand_size_attr = xsi_geometry.GetICEAttributeFromName("StrandSize");  // this is the radius of the strand, NOT the number of points
	// strand_size_attr.IsDefined() can be false if the attribute is not defined in the ICE tree
	// strand_size_attr.GetElementCount() is equal to the number of points
	// context is siICENodeContextComponent0D (one element per point)
	// data type is siICENodeDataFloat
	// structure is siICENodeStructureArray (array of values), even if defined by only one value, it contains array with this one value, the length of the array may not be equal to the count of strand points

	// fill the data
	XSI::CICEAttributeDataArrayVector3f point_position_data;
	point_position_attr.GetDataArray(point_position_data);

	XSI::CICEAttributeDataArray2DVector3f strand_position_data;
	strand_position_attr.GetDataArray2D(strand_position_data);

	XSI::CICEAttributeDataArrayFloat size_data;
	// in trivial case (when we generate grid from two strands (1x2 or 2x1)) the data contains only one element, instead of 2
	// use this in prepare stage
	size_attr.GetDataArray(size_data);
	ULONG size_data_length = size_data.GetCount();

	XSI::CICEAttributeDataArray2DFloat strand_size_data;
	XSI::CICEAttributeDataArrayFloat one_strand_size_data;
	bool use_strand_size = strand_size_attr.IsDefined();
	if (use_strand_size)
	{
		strand_size_attr.GetDataArray2D(strand_size_data);
	}

	XSI::CICEAttributeDataArrayVector3f one_strand_data;

	// at first we should reserve the curves in the geometry
	// so, iterate throw points and calculate indices of non-trivail strands
	ULONG total_keys = 0;
	ULONG total_curves = 0;
	ULONG points_count = point_position_data.GetCount();
	for (size_t i = 0; i < points_count; i++)
	{
		// get the current strand length data
		strand_position_data.GetSubArray(i, one_strand_data);
		ULONG strand_length = one_strand_data.GetCount();
		if (strand_length > 0)
		{
			out_strand_points.push_back(i);
			out_strand_lengths.push_back(strand_length);

			total_keys += strand_length + 1;  // because each strand does not contains start point (point position)
			total_curves += 1;
		}
	}

	strands_geom->reserve_curves(total_curves, total_keys);
	ccl::Attribute* attr_intercept = NULL;
	ccl::Attribute* attr_random = NULL;
	ccl::Attribute* attr_length = NULL;
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_INTERCEPT))
	{
		attr_intercept = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_INTERCEPT);
	}
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_RANDOM))
	{
		attr_random = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_RANDOM);
	}
	if (strands_geom->need_attribute(scene, ccl::ATTR_STD_CURVE_LENGTH))
	{
		attr_length = strands_geom->attributes.add(ccl::ATTR_STD_CURVE_LENGTH);
	}
	ULONG original_index = 0;
	
	if (use_motion_blur)
	{
		out_original_positions.resize(total_keys);
	}

	ULONG strand_sizes_length = 0;
	ULONG first_key = 0;
	for (size_t curve_index = 0; curve_index < total_curves; curve_index++)
	{
		// get actual point index for this curve
		size_t point_index = out_strand_points[curve_index];  // we should get data for this point and strand

		// get strand positions array
		strand_position_data.GetSubArray(point_index, one_strand_data);
		ULONG strand_knots_length = one_strand_data.GetCount();
		// one strand data contains non-zero values
		// also extract strand sizes, if it exists
		if (use_strand_size)
		{
			strand_size_data.GetSubArray(point_index, one_strand_size_data);
			// and save the array length
			// we will use it later to check is the array is too short or not
			strand_sizes_length = one_strand_size_data.GetCount();
		}
		else
		{
			strand_sizes_length = 0;
		}

		// set start point of the strand - point position
		XSI::MATH::CVector3f point_position = point_position_data[point_index];
		// for initial strand point use point size
		// if points size aray is empty, use zero value
		// if the point index outside of the array, use the last value of the array
		float point_radius = calculate_strand_radius(point_index, size_data, size_data_length, 0, strand_knots_length + 1, one_strand_size_data, strand_sizes_length);
		strands_geom->add_curve_key(vector3_to_float3(point_position), point_radius);

		// if we need motion blur, fill original positions array
		if (use_motion_blur)
		{
			out_original_positions[original_index] = ccl::make_float4(point_position.GetX(), point_position.GetY(), point_position.GetZ(), point_radius);
			original_index++;
		}

		// start strand, so the intercept attribute is 0.0
		if (attr_intercept)
		{
			attr_intercept->add(0.0);
		}

		float strand_sparse_length = 0.0f;
		float total_time = strand_knots_length + 1;
		for (ULONG k_index = 0; k_index < strand_knots_length; k_index++)
		{
			float radius = calculate_strand_radius(point_index, size_data, size_data_length, k_index + 1, strand_knots_length + 1, one_strand_size_data, strand_sizes_length);
			
			XSI::MATH::CVector3f knot_position = one_strand_data[k_index];
			strands_geom->add_curve_key(vector3_to_float3(knot_position), radius);
			if (k_index > 0)
			{
				XSI::MATH::CVector3f prev_knot_position = one_strand_data[k_index - 1];
				float dx = knot_position.GetX() - prev_knot_position.GetX();
				float dy = knot_position.GetY() - prev_knot_position.GetY();
				float dz = knot_position.GetZ() - prev_knot_position.GetZ();
				strand_sparse_length += sqrtf(dx * dx + dy * dy + dz * dz);
			}

			if (use_motion_blur)
			{
				out_original_positions[original_index] = ccl::make_float4(knot_position.GetX(), knot_position.GetY(), knot_position.GetZ(), radius);
				original_index++;
			}

			if (attr_intercept)
			{
				float time = static_cast<float>(k_index + 1) / total_time;

				attr_intercept->add(time);
			}
		}
		if (attr_random != NULL)
		{
			attr_random->add(ccl::hash_uint2_to_float(curve_index, 0));
		}
		if (attr_length != NULL)
		{
			attr_length->add(strand_sparse_length);
		}
		strands_geom->add_curve(first_key, 0);
		first_key += strand_knots_length + 1;
	}

	ccl::Attribute* attr_generated = strands_geom->attributes.add(ccl::ATTR_STD_GENERATED);
	ccl::float3* generated = attr_generated->data_float3();
	for (size_t i = 0; i < strands_geom->num_curves(); i++)
	{
		ccl::float3 co = strands_geom->get_curve_keys()[strands_geom->get_curve(i).first_key];
		generated[i] = co;
	}

	// add ICE attributes
	// each attribute is per curve
	XSI::CRefArray xsi_ice_attributes = xsi_geometry.GetICEAttributes();

	for (LONG i = 0; i < xsi_ice_attributes.GetCount(); i++)
	{
		XSI::ICEAttribute xsi_attribute(xsi_ice_attributes[i]);
		XSI::CString xsi_attr_name = xsi_attribute.GetName();
		ccl::ustring attr_name = ccl::ustring(xsi_attr_name.GetAsciiString());

		if (strands_geom->need_attribute(scene, attr_name))
		{
			XSI::siICENodeContextType attr_context = xsi_attribute.GetContextType();
			XSI::siICENodeStructureType attr_structure = xsi_attribute.GetStructureType();

			if (attr_context == XSI::siICENodeContextComponent0D && attr_structure == XSI::siICENodeStructureSingle)
			{
				// supports only one element per point
				// like color and something similar
				XSI::siICENodeDataType attr_data_type = xsi_attribute.GetDataType();

				if (attr_data_type == XSI::siICENodeDataVector3)
				{
					XSI::CICEAttributeDataArrayVector3f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeVector, ccl::ATTR_ELEMENT_CURVE);

					ccl::float3* cyc_attr_data = cycles_attribute->data_float3();
					for (size_t v_index = 0; v_index < total_curves; v_index++)
					{
						*cyc_attr_data = vector3_to_float3(attr_data[out_strand_points[v_index]]);
						cyc_attr_data++;
					}
				}
				else if (attr_data_type == XSI::siICENodeDataColor4)
				{
					XSI::CICEAttributeDataArrayColor4f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeColor, ccl::ATTR_ELEMENT_CURVE);

					ccl::float4* cyc_attr_data = cycles_attribute->data_float4();
					for (size_t v_index = 0; v_index < total_curves; v_index++)
					{
						*cyc_attr_data = color4_to_float4(attr_data[out_strand_points[v_index]]);
						cyc_attr_data++;
					}
				}
				else if (attr_data_type == XSI::siICENodeDataFloat)
				{
					XSI::CICEAttributeDataArrayFloat attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = strands_geom->attributes.add(attr_name, ccl::TypeFloat, ccl::ATTR_ELEMENT_CURVE);

					float* cyc_attr_data = cycles_attribute->data_float();
					for (size_t v_index = 0; v_index < total_curves; v_index++)
					{
						*cyc_attr_data = attr_data[out_strand_points[v_index]];
						cyc_attr_data++;
					}
				}
			}
		}
	}
}

void sync_strands_deform(ccl::Hair* hair, 
	UpdateContext* update_context,
	const XSI::X3DObject &xsi_object, 
	const ccl::vector<ccl::float4>& original_positions,
	const ccl::vector<size_t> &strand_points,
	const ccl::vector<size_t>& strand_length)
{
	size_t motion_steps = update_context->get_motion_steps();
	hair->set_motion_steps(motion_steps);
	hair->set_use_motion_blur(true);

	size_t attribute_index = 0;
	ccl::Attribute* attr_m_positions = hair->attributes.add(ccl::ATTR_STD_MOTION_VERTEX_POSITION, ccl::ustring("std_motion_strand_position"));
	ccl::float4* motion_positions = attr_m_positions->data_float4();
	MotionSettingsPosition motion_position = update_context->get_motion_position();

	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		size_t time_motion_step = mi;
		if (motion_position == MotionSettingsPosition::MotionPosition_Start)
		{
			time_motion_step++;
		}
		else if (motion_position == MotionSettingsPosition::MotionPosition_Center)
		{// center
			if (mi >= motion_steps / 2)
			{
				time_motion_step++;
			}
		}

		float time = update_context->get_motion_time(time_motion_step);
		XSI::Geometry time_xsi_geometry = xsi_object.GetActivePrimitive(time).GetGeometry();

		XSI::ICEAttribute time_pos = time_xsi_geometry.GetICEAttributeFromName("PointPosition");
		XSI::ICEAttribute time_strand = time_xsi_geometry.GetICEAttributeFromName("StrandPosition");
		XSI::ICEAttribute time_size = time_xsi_geometry.GetICEAttributeFromName("Size");
		XSI::ICEAttribute time_strand_size = time_xsi_geometry.GetICEAttributeFromName("StrandSize");
		bool use_strand_size = time_strand_size.IsDefined();
		if (time_pos.GetElementCount() > 0 && time_strand.GetElementCount() > 0 && time_size.GetElementCount() > 0)
		{// attributes exists, get the data
			// prepare data buffers
			// make it every time step, because there are some problems if we done it once at very beginning
			XSI::CICEAttributeDataArrayVector3f time_pos_data;
			XSI::CICEAttributeDataArray2DVector3f time_strand_data;
			XSI::CICEAttributeDataArrayFloat time_size_data;
			XSI::CICEAttributeDataArray2DFloat time_strand_size_data;

			time_pos.GetDataArray(time_pos_data);
			time_strand.GetDataArray2D(time_strand_data);
			time_size.GetDataArray(time_size_data);

			ULONG time_points_count = time_pos_data.GetCount();
			ULONG time_sizes_count = time_size_data.GetCount();
			ULONG time_strands_count = time_strand_data.GetCount();
			ULONG time_strand_size_count = 0;
			if (use_strand_size)
			{
				time_strand_size.GetDataArray2D(time_strand_size_data);
				time_strand_size_count = time_strand_size_data.GetCount();
			}

			// iterate throw strands
			size_t total_curves = strand_points.size();
			size_t original_positions_iterator = 0;
			// we should iterate throw curves in the original frame
			for (size_t curve_index = 0; curve_index < total_curves; curve_index++)
			{
				size_t point_index = strand_points[curve_index];
				size_t strand_knots_count = strand_length[curve_index];
				// check that in this time the point exists
				bool is_fail = true;
				if (point_index < time_points_count && point_index < time_sizes_count && point_index < time_strands_count)
				{
					// create buffer for every strand independently
					// in onether case the render is crashed
					XSI::CICEAttributeDataArrayVector3f time_one_strand_positions;
					time_strand_data.GetSubArray(point_index, time_one_strand_positions);
					XSI::CICEAttributeDataArrayFloat strand_sizes;
					ULONG strand_sizes_length = 0;
					if (use_strand_size && point_index < time_strand_size_count)
					{
						time_strand_size_data.GetSubArray(point_index, strand_sizes);
						strand_sizes_length = strand_sizes.GetCount();
					}
					size_t time_strand_knots_count = time_one_strand_positions.GetCount();  // the number of knots in the current strand at current time
					float strand_prop_length = time_strand_knots_count + 1;
					if (time_strand_knots_count == strand_knots_count)
					{
						// the strand length at the time equal to the strand length at original frame
						is_fail = false;

						XSI::MATH::CVector3f p = time_pos_data[point_index];
						float point_radius = calculate_strand_radius(point_index, time_size_data, time_sizes_count, 0, time_strand_knots_count + 1, strand_sizes, strand_sizes_length);
						motion_positions[attribute_index++] = ccl::make_float4(p.GetX(), p.GetY(), p.GetZ(), point_radius);
						// next keys for strand positions
						for (ULONG k_index = 0; k_index < time_strand_knots_count; k_index++)
						{
							float s = calculate_strand_radius(point_index, time_size_data, time_sizes_count, k_index + 1, time_strand_knots_count + 1, strand_sizes, strand_sizes_length);
							p = time_one_strand_positions[k_index];
							motion_positions[attribute_index++] = ccl::make_float4(p.GetX(), p.GetY(), p.GetZ(), s);
						}
					}
					else
					{
						// the strand length is different with original one
						// in this case simply copy original positions
						is_fail = true;
					}
				}
				if (is_fail)
				{
					// at present time the pointcloud does not contains data for a given point
					// or the strand length is different with original one
					// in this case we should copy strand positions from original positions (only for this strand)
					for (size_t k = 0; k < strand_knots_count + 1; k++)
					{
						motion_positions[attribute_index++] = original_positions[original_positions_iterator + k];
					}
				}
				original_positions_iterator += strand_knots_count + 1;
			}
		}
		else
		{// invald attribute at the time, set default positions
			for (size_t k_index = 0; k_index < original_positions.size(); k_index++)
			{
				motion_positions[attribute_index++] = original_positions[k_index];
			}
		}
	}
}

// in this function we actually create geometry
// and also define motion deform, if we need it
void sync_strands_geom_process(ccl::Scene* scene, ccl::Hair* strands_geom, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, bool motion_deform)
{
	strands_geom->name = combine_geometry_name(xsi_object, xsi_primitive).GetAsciiString();

	ccl::vector<ccl::float4> original_positions;
	ccl::vector<size_t> strand_points;
	ccl::vector<size_t> strand_lengths;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	sync_strands_geom(scene, strands_geom, update_context, xsi_primitive, use_motion_blur, original_positions, strand_points, strand_lengths);

	if (use_motion_blur)
	{
		sync_strands_deform(strands_geom, update_context, xsi_object, original_positions, strand_points, strand_lengths);
	}
	else
	{
		strands_geom->set_use_motion_blur(false);
	}

	original_positions.clear();
	original_positions.shrink_to_fit();
}

// here we setup object, create or assign geometry, setup transform (with object motion)
ccl::Hair* sync_strands_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive(xsi_object.GetActivePrimitive(eval_time));
	ULONG xsi_primitive_id = xsi_primitive.GetObjectID();
	if (update_context->is_geometry_exists(xsi_primitive_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_primitive_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::HAIR)
		{
			return static_cast<ccl::Hair*>(scene->geometry[geo_index]);
		}
	}

	ccl::Hair* hair_geom = scene->create_node<ccl::Hair>();

	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	hair_geom->set_used_shaders(used_shaders);

	sync_strands_geom_process(scene, hair_geom, update_context, xsi_primitive, xsi_object, motion_deform);

	update_context->add_geometry_index(xsi_primitive_id, scene->geometry.size() - 1);

	return hair_geom;
}

XSI::CStatus update_strands(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::HAIR)
			{
				ccl::Hair* strands_geom = static_cast<ccl::Hair*>(geometry);
				strands_geom->clear(true);

				sync_strands_geom_process(scene, strands_geom, update_context, xsi_prim, xsi_object, motion_deform);

				bool rebuild = strands_geom->curve_keys_is_modified() || strands_geom->curve_radius_is_modified();
				strands_geom->tag_update(scene, rebuild);
			}
			else
			{
				return XSI::CStatus::Abort;
			}
		}
		else
		{
			return XSI::CStatus::Abort;
		}
	}
	else
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}