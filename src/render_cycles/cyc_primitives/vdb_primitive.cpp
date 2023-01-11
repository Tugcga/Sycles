#include "vdb_primitive.h"

#include <xsi_status.h>
#include <xsi_menu.h>
#include <xsi_customprimitive.h>
#include <xsi_factory.h>
#include <xsi_parameter.h>
#include <xsi_ppglayout.h>
#include <xsi_ppgeventcontext.h>
#include <xsi_customproperty.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // Needed for OpenGL on windows

#include <GL/gl.h>
#include <GL/glu.h>

#define kParamCaps  (XSI::siPersistable)

static VDBPrimitivesDataContainer vdb_cache;

SICALLBACK VDBPrimitiveObjectRemoved_OnEvent(const XSI::CRef& in_ref)
{
	XSI::Context ctxt(in_ref);
	XSI::CValueArray objects_removed = ctxt.GetAttribute("ObjectNames");
	for (ULONG i = 0; i < objects_removed.GetCount(); i++)
	{
		vdb_cache.remove(objects_removed[i].GetAsText());
	}
	return XSI::CStatus::False;
}

SICALLBACK VDBPrimitiveSceneOpen_OnEvent(const XSI::CRef& in_ref)
{
	vdb_cache.clear();
	return XSI::CStatus::False;
}

SICALLBACK VDBPrimitiveSceneClose_OnEvent(const XSI::CRef& in_ref)
{
	vdb_cache.clear();
	return XSI::CStatus::False;
}

SICALLBACK VDBPrimitiveMenu_Init(const XSI::CRef& in_ref)
{
	// retrieve the menu object to build
	XSI::Menu menu = XSI::Context(in_ref).GetSource();

	// set the menu caption
	menu.PutName("&VDBPrimitive Menu");

	// adds other menu items and attach a function callback
	XSI::MenuItem menu_item;
	menu.AddCallbackItem("VDB Primitive", "OnVDBPrimitiveItem", menu_item);

	return XSI::CStatus::OK;
}

SICALLBACK OnVDBPrimitiveItem(XSI::CRef& in_ref)
{
	XSI::Context ctxt(in_ref);
	XSI::MenuItem menuItem = ctxt.GetSource();

	XSI::Application app;
	XSI::CValue out_arg;
	XSI::CValueArray in_args;
	in_args.Add("VDBPrimitive");
	app.ExecuteCommand("GetPrim", in_args, out_arg);

	return XSI::CStatus::OK;
}

SICALLBACK VDBPrimitive_Define(const XSI::CRef& in_ref)
{
	XSI::Context in_ctxt(in_ref);
	XSI::CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		XSI::Factory fact = XSI::Application().GetFactory();
		XSI::CRef folder_def = fact.CreateParamDef("folder", XSI::CValue::siString, "");  // "[Project Path]/VDB_Grids/[Scene]");
		XSI::Parameter folder;
		in_prim.AddParameter(folder_def, folder);

		XSI::CRef file_def = fact.CreateParamDef("file", XSI::CValue::siString, "VDBGrids$F4");
		XSI::Parameter file;
		in_prim.AddParameter(file_def, file);

		XSI::CRef offset_def = fact.CreateParamDef("offset", XSI::CValue::siInt4, kParamCaps, "offset", "", 0, -INT_MAX, INT_MAX, -10, 10);
		XSI::Parameter offset;
		in_prim.AddParameter(offset_def, offset);

		XSI::CRef frame_def = fact.CreateParamDef("frame", XSI::CValue::siInt4, XSI::siAnimatable, "frame", "", 1, -INT_MAX, INT_MAX, 1, 100);
		XSI::Parameter frame;
		in_prim.AddParameter(frame_def, frame);

		XSI::CRef grid_index_def = fact.CreateParamDef("grid_index", XSI::CValue::siInt4, kParamCaps, "grid_index", "", 0, 0, INT_MAX, 0, INT_MAX);
		XSI::Parameter grid_index;
		in_prim.AddParameter(grid_index_def, grid_index);

		XSI::CRef visual_def = fact.CreateParamDef("visual", XSI::CValue::siBool, kParamCaps, "visual", "", false, 0, 1, 0, 1);
		XSI::Parameter visual;
		in_prim.AddParameter(visual_def, visual);

		// pass parameters
		XSI::CRef pass_id_def = fact.CreateParamDef("pass_id", XSI::CValue::siInt4, kParamCaps, "pass_id", "", 0, 0, INT_MAX, 0, 100);
		XSI::CRef is_shadow_catcher_def = fact.CreateParamDef("is_shadow_catcher", XSI::CValue::siBool, kParamCaps, "is_shadow_catcher", "", false, 0, 1, 0, 1);
		XSI::CRef lightgroup_def = fact.CreateParamDef("lightgroup", XSI::CValue::siString, kParamCaps, "lightgroup", "", "", "", "", "", "");

		XSI::Parameter pass_id;
		XSI::Parameter is_shadow_catcher;
		XSI::Parameter lightgroup;

		in_prim.AddParameter(pass_id_def, pass_id);
		in_prim.AddParameter(is_shadow_catcher_def, is_shadow_catcher);
		in_prim.AddParameter(lightgroup_def, lightgroup);

		// visibility
		XSI::CRef ray_camera_def = fact.CreateParamDef("ray_visibility_camera", XSI::CValue::siBool, kParamCaps, "ray_visibility_camera", "", true, 0, 1, 0, 1);
		XSI::CRef ray_diffuse_def = fact.CreateParamDef("ray_visibility_diffuse", XSI::CValue::siBool, kParamCaps, "ray_visibility_diffuse", "", true, 0, 1, 0, 1);
		XSI::CRef ray_glossy_def = fact.CreateParamDef("ray_visibility_glossy", XSI::CValue::siBool, kParamCaps, "ray_visibility_glossy", "", true, 0, 1, 0, 1);
		XSI::CRef ray_transmission_def = fact.CreateParamDef("ray_visibility_transmission", XSI::CValue::siBool, kParamCaps, "ray_visibility_transmission", "", true, 0, 1, 0, 1);
		XSI::CRef ray_scatter_def = fact.CreateParamDef("ray_visibility_volume_scatter", XSI::CValue::siBool, kParamCaps, "ray_visibility_volume_scatter", "", true, 0, 1, 0, 1);
		XSI::CRef ray_shadow_def = fact.CreateParamDef("ray_visibility_shadow", XSI::CValue::siBool, kParamCaps, "ray_visibility_shadow", "", true, 0, 1, 0, 1);

		XSI::Parameter ray_camera;
		XSI::Parameter ray_diffuse;
		XSI::Parameter ray_glossy;
		XSI::Parameter ray_transmission;
		XSI::Parameter ray_scatte;
		XSI::Parameter ray_shadow;

		in_prim.AddParameter(ray_camera_def, ray_camera);
		in_prim.AddParameter(ray_diffuse_def, ray_diffuse);
		in_prim.AddParameter(ray_glossy_def, ray_glossy);
		in_prim.AddParameter(ray_transmission_def, ray_transmission);
		in_prim.AddParameter(ray_scatter_def, ray_scatte);
		in_prim.AddParameter(ray_shadow_def, ray_shadow);
	}
	return XSI::CStatus::OK;
}

void set_grid_index(XSI::CustomPrimitive& in_prim, const bool is_visible)
{
	XSI::CParameterRefArray prop_array = in_prim.GetParameters();
	XSI::Parameter param = prop_array.GetItem("grid_index");
	param.PutCapabilityFlag(XSI::siNotInspectable, !is_visible);
}

void build_vdb_ui(XSI::PPGLayout& layout, XSI::CustomPrimitive& in_prim)
{
	layout.Clear();

	layout.AddTab("VDB Settings");
	layout.AddGroup("Parameters");
	XSI::PPGItem item_folder = layout.AddItem("folder", "Folder Path", XSI::siControlFolder);
	item_folder.PutAttribute(XSI::siUIInitialDir, L"project");
	layout.AddItem("file", "File Name");
	layout.AddItem("offset", "Frame Offset");
	layout.AddItem("frame", "Frame");

	layout.AddItem("visual", "Visualize in Viewport");

	VDBData data = vdb_cache.get(in_prim);

	if (data.is_valid)
	{
		XSI::CValueArray grids_combobox;
		for (ULONG i = 0; i < data.grids_count; i++)
		{
			grids_combobox.Add(data.grid_names[i]);
			grids_combobox.Add(i);
		}

		layout.AddEnumControl("grid_index", grids_combobox, "Grid", XSI::siControlCombo);

		// clamp grid index
		int index = in_prim.GetParameterValue("grid_index");
		if (index < 0)
		{
			index = 0;
		}
		if (index >= data.grids_count)
		{
			index = data.grids_count - 1;
		}

		in_prim.PutParameterValue("grid_index", (LONG)index);
		set_grid_index(in_prim, true);
		layout.EndGroup();
		// set static text
		layout.AddGroup("Statistics");
		std::vector<XSI::CString> text_content = data.get_description(index);
		for (ULONG i = 0; i < text_content.size(); i++)
		{
			layout.AddStaticText(text_content[i]);
		}
	}
	else
	{
		XSI::CValueArray grids_combobox(0);
		layout.AddEnumControl("grid_index", grids_combobox, "Grid", XSI::siControlCombo);
		set_grid_index(in_prim, false);
	}

	layout.EndGroup();

	layout.AddTab("Pass");
	layout.AddGroup("Properties");
	layout.AddItem("pass_id", "Pass ID");
	layout.AddItem("is_shadow_catcher", "Shadow Catcher");
	layout.AddGroup("Light Group");
	XSI::PPGItem item = layout.AddItem("lightgroup", "Light Group");
	item.PutAttribute(XSI::siUINoLabel, true);
	layout.EndGroup();
	layout.EndGroup();

	layout.AddTab("Visibility");
	layout.AddGroup("Ray Visibility");
	layout.AddItem("ray_visibility_camera", "Camera");
	layout.AddItem("ray_visibility_diffuse", "Diffuse");
	layout.AddItem("ray_visibility_glossy", "Glossy");
	layout.AddItem("ray_visibility_transmission", "Transmission");
	layout.AddItem("ray_visibility_volume_scatter", "Volume Scatter");
	layout.AddItem("ray_visibility_shadow", "Shadow");
	layout.EndGroup();
}

SICALLBACK VDBPrimitive_DefineLayout(XSI::CRef& in_ctxt)
{
	XSI::Context ctxt(in_ctxt);
	XSI::PPGLayout oLayout;
	XSI::PPGItem oItem;
	oLayout = ctxt.GetSource();

	XSI::Parameter changed = ctxt.GetSource();
	XSI::CustomPrimitive prim = changed.GetParent();

	build_vdb_ui(oLayout, prim);

	return XSI::CStatus::OK;
}

SICALLBACK VDBPrimitive_PPGEvent(const XSI::CRef& in_ctxt)
{
	XSI::PPGEventContext ctx(in_ctxt);
	XSI::PPGEventContext::PPGEvent event_id = ctx.GetEventID();

	if (event_id == XSI::PPGEventContext::siOnInit)
	{
		XSI::CustomPrimitive primitive = ctx.GetSource();
		XSI::PPGLayout layout = primitive.GetPPGLayout();
		build_vdb_ui(layout, primitive);

		ctx.PutAttribute("Refresh", true);
	}
	if (event_id == XSI::PPGEventContext::siParameterChange)
	{
		XSI::Parameter changed = ctx.GetSource();
		XSI::CustomPrimitive primitive = changed.GetParent();
		XSI::CString param_name = changed.GetScriptName();
		if (param_name == "folder" || param_name == "file" || param_name == "offset" || param_name == "frame" || param_name == "grid_index")
		{
			XSI::PPGLayout layout = primitive.GetPPGLayout();
			build_vdb_ui(layout, primitive);
			ctx.PutAttribute("Refresh", true);
		}
	}

	return XSI::CStatus::OK;
}

SICALLBACK VDBPrimitive_BoundingBox(const XSI::CRef& in_ref)
{
	XSI::Context in_ctxt(in_ref);
	XSI::CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return XSI::CStatus::Fail;
	}

	XSI::CParameterRefArray& params = in_prim.GetParameters();
	VDBData data = vdb_cache.get(in_prim);
	if (data.is_valid)
	{
		int grid_index = params.GetValue("grid_index");
		if (grid_index >= 0 && grid_index < data.grids_count)
		{
			double* bb = data.get_bb(grid_index);
			in_ctxt.PutAttribute("LowerBoundX", bb[0]);
			in_ctxt.PutAttribute("LowerBoundY", bb[1]);
			in_ctxt.PutAttribute("LowerBoundZ", bb[2]);
			in_ctxt.PutAttribute("UpperBoundX", bb[3]);
			in_ctxt.PutAttribute("UpperBoundY", bb[4]);
			in_ctxt.PutAttribute("UpperBoundZ", bb[5]);
		}
	}
	else
	{
		in_ctxt.PutAttribute("LowerBoundX", 0.0);
		in_ctxt.PutAttribute("LowerBoundY", 0.0);
		in_ctxt.PutAttribute("LowerBoundZ", 0.0);
		in_ctxt.PutAttribute("UpperBoundX", 0.0);
		in_ctxt.PutAttribute("UpperBoundY", 0.0);
		in_ctxt.PutAttribute("UpperBoundZ", 0.0);
	}

	return XSI::CStatus::OK;
}

SICALLBACK VDBPrimitive_Draw(const XSI::CRef& in_ref)
{
	XSI::Context in_ctxt(in_ref);
	XSI::CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return XSI::CStatus::Fail;
	}

	XSI::CParameterRefArray& params = in_prim.GetParameters();
	VDBData data = vdb_cache.get(in_prim);
	int grid_index = params.GetValue("grid_index");
	if (grid_index >= 0 && grid_index < data.grids_count)
	{
		double* bb = data.get_bb(grid_index);
		double boxMinPt[3];
		double boxMaxPt[3];

		boxMinPt[0] = bb[0];
		boxMinPt[1] = bb[1];
		boxMinPt[2] = bb[2];

		boxMaxPt[0] = bb[3];
		boxMaxPt[1] = bb[4];
		boxMaxPt[2] = bb[5];

		GLdouble l_Verts[8][3];

		l_Verts[0][0] = boxMinPt[0];
		l_Verts[0][1] = boxMinPt[1];
		l_Verts[0][2] = boxMinPt[2];

		l_Verts[1][0] = boxMaxPt[0];
		l_Verts[1][1] = boxMinPt[1];
		l_Verts[1][2] = boxMinPt[2];

		l_Verts[2][0] = boxMaxPt[0];
		l_Verts[2][1] = boxMaxPt[1];
		l_Verts[2][2] = boxMinPt[2];

		l_Verts[3][0] = boxMinPt[0];
		l_Verts[3][1] = boxMaxPt[1];
		l_Verts[3][2] = boxMinPt[2];

		l_Verts[4][0] = boxMinPt[0];
		l_Verts[4][1] = boxMaxPt[1];
		l_Verts[4][2] = boxMaxPt[2];

		l_Verts[5][0] = boxMaxPt[0];
		l_Verts[5][1] = boxMaxPt[1];
		l_Verts[5][2] = boxMaxPt[2];

		l_Verts[6][0] = boxMaxPt[0];
		l_Verts[6][1] = boxMinPt[1];
		l_Verts[6][2] = boxMaxPt[2];

		l_Verts[7][0] = boxMinPt[0];
		l_Verts[7][1] = boxMinPt[1];
		l_Verts[7][2] = boxMaxPt[2];

		::glBegin(GL_LINE_STRIP);
		::glVertex3dv(l_Verts[0]);
		::glVertex3dv(l_Verts[1]);
		::glVertex3dv(l_Verts[2]);
		::glVertex3dv(l_Verts[3]);
		::glVertex3dv(l_Verts[0]);

		::glVertex3dv(l_Verts[7]);
		::glVertex3dv(l_Verts[6]);
		::glVertex3dv(l_Verts[5]);
		::glVertex3dv(l_Verts[4]);
		::glVertex3dv(l_Verts[7]);
		::glEnd();

		::glBegin(GL_LINES);
		::glVertex3dv(l_Verts[3]);
		::glVertex3dv(l_Verts[4]);
		::glVertex3dv(l_Verts[2]);
		::glVertex3dv(l_Verts[5]);
		::glVertex3dv(l_Verts[1]);
		::glVertex3dv(l_Verts[6]);
		::glEnd();

		if (params.GetValue("visual"))
		{
			openvdb::GridBase::Ptr grid = data.grids[grid_index];
			ULONG voxels_count = grid->activeVoxelCount();
			// next for different data types
			if (grid->isType<openvdb::FloatGrid>())
			{// FLOAT
				openvdb::FloatGrid::Ptr float_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::FloatGrid::ValueOnCIter iter = float_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = float_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::DoubleGrid>())
			{// DOUBLE
				openvdb::DoubleGrid::Ptr double_grid = openvdb::gridPtrCast<openvdb::DoubleGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::DoubleGrid::ValueOnCIter iter = double_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = double_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::BoolGrid>())
			{// BOOLEAN
				openvdb::BoolGrid::Ptr bool_grid = openvdb::gridPtrCast<openvdb::BoolGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::BoolGrid::ValueOnCIter iter = bool_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = bool_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::Int32Grid>())
			{// INT32
				openvdb::Int32Grid::Ptr int32_grid = openvdb::gridPtrCast<openvdb::Int32Grid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::Int32Grid::ValueOnCIter iter = int32_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = int32_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::Int64Grid>())
			{// INT64
				openvdb::Int64Grid::Ptr int64_grid = openvdb::gridPtrCast<openvdb::Int64Grid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::Int64Grid::ValueOnCIter iter = int64_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = int64_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::Vec3IGrid>())
			{// VEC3I
				openvdb::Vec3IGrid::Ptr vec3i_grid = openvdb::gridPtrCast<openvdb::Vec3IGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::Vec3IGrid::ValueOnCIter iter = vec3i_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = vec3i_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::Vec3SGrid>())
			{// VEC3S
				openvdb::Vec3SGrid::Ptr vec3s_grid = openvdb::gridPtrCast<openvdb::Vec3SGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::Vec3SGrid::ValueOnCIter iter = vec3s_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = vec3s_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
			else if (grid->isType<openvdb::Vec3DGrid>())
			{// VEC3D
				openvdb::Vec3DGrid::Ptr vec3d_grid = openvdb::gridPtrCast<openvdb::Vec3DGrid>(grid);
				LLONG cnt = 0;
				::glBegin(GL_POINTS);
				for (openvdb::Vec3DGrid::ValueOnCIter iter = vec3d_grid->cbeginValueOn(); cnt < voxels_count; ++cnt)
				{
					openvdb::math::Vec3d vdbVec3 = vec3d_grid->indexToWorld(iter.getCoord());
					::glVertex3f(vdbVec3.x(), vdbVec3.y(), vdbVec3.z());
					++iter;
				};
				::glEnd();
			}
		}
	}

	return XSI::CStatus::OK;
}

VDBData get_vdb_data(XSI::CustomPrimitive& in_prim)
{
	return vdb_cache.get(in_prim);
}