#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // Needed for OpenGL on windows
#include <algorithm> 

#include <GL/gl.h>
#include <GL/glu.h>
#include <cfloat>
#include <math.h> 

#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_pluginregistrar.h>
#include <xsi_status.h>
#include <xsi_menu.h>
#include <xsi_customprimitive.h>
#include <xsi_factory.h>
#include <xsi_parameter.h>
#include <xsi_ppglayout.h>
#include <xsi_color4f.h>
#include <valarray>

using namespace XSI;
using namespace std;

extern GLUquadric* g_quadric;
#define kParamCaps	(siAnimatable | siPersistable | siKeyable)
#define PI 3.14159265

void add_standard_parameters(CustomPrimitive& in_prim, CString primitiveTypeName)
{
	Factory fact = Application().GetFactory();
	CRef type_def = fact.CreateParamDef("cycles_primitive_type", CValue::siString, primitiveTypeName);

	CRef max_bounces_def = fact.CreateParamDef("max_bounces", CValue::siInt4, kParamCaps, "max_bounces", "", 1024, 0, INT_MAX, 512, 2048);
	CRef cast_shadow_def = fact.CreateParamDef("cast_shadow", CValue::siBool, kParamCaps, "cast_shadow", "", true, 0, 1, 0, 1);
	CRef mis_def = fact.CreateParamDef("multiple_importance", CValue::siBool, kParamCaps, "multiple_importance", "", true, 0, 1, 0, 1);
	CRef use_camera_def = fact.CreateParamDef("use_camera", CValue::siBool, kParamCaps, "use_camera", "", false, 0, 1, 0, 1);
	CRef use_diffuse_def = fact.CreateParamDef("use_diffuse", CValue::siBool, kParamCaps, "use_diffuse", "", true, 0, 1, 0, 1);
	CRef use_glossy_def = fact.CreateParamDef("use_glossy", CValue::siBool, kParamCaps, "use_glossy", "", true, 0, 1, 0, 1);
	CRef use_transmission_def = fact.CreateParamDef("use_transmission", CValue::siBool, kParamCaps, "use_transmission", "", true, 0, 1, 0, 1);
	CRef use_scatter_def = fact.CreateParamDef("use_scatter", CValue::siBool, kParamCaps, "use_scatter", "", true, 0, 1, 0, 1);
	CRef shadow_caustics_def = fact.CreateParamDef("shadow_caustics", CValue::siBool, kParamCaps, "shadow_caustics", "", false, 0, 1, 0, 1);

	CRef is_shadow_catcher_def = fact.CreateParamDef("is_shadow_catcher", CValue::siBool, kParamCaps, "is_shadow_catcher", "", true, 0, 1, 0, 1);

	CRef lightgroup_def = fact.CreateParamDef("lightgroup", CValue::siString, kParamCaps, "lightgroup", "", "", "", "", "", "");

	Parameter type;
	Parameter max_bounces;
	Parameter cast_shadow;
	Parameter mis;
	Parameter use_camera;
	Parameter use_diffuse;
	Parameter use_glossy;
	Parameter use_transmission;
	Parameter use_scatter;
	Parameter shadow_caustics;

	Parameter is_shadow_catcher;
	Parameter lightgroup;

	in_prim.AddParameter(type_def, type);

	in_prim.AddParameter(max_bounces_def, max_bounces);
	in_prim.AddParameter(cast_shadow_def, cast_shadow);
	in_prim.AddParameter(mis_def, mis);
	in_prim.AddParameter(use_camera_def, use_camera);
	in_prim.AddParameter(use_diffuse_def, use_diffuse);
	in_prim.AddParameter(use_glossy_def, use_glossy);
	in_prim.AddParameter(use_transmission_def, use_transmission);
	in_prim.AddParameter(use_scatter_def, use_scatter);
	in_prim.AddParameter(shadow_caustics_def, shadow_caustics);

	in_prim.AddParameter(is_shadow_catcher_def, is_shadow_catcher);
	in_prim.AddParameter(lightgroup_def, lightgroup);
}

SICALLBACK cyclesPoint_Define(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		Factory fact = Application().GetFactory();
		CRef sizeDef = fact.CreateParamDef("size", CValue::siDouble, kParamCaps, "size", "", 0.1, 0.000001, DBL_MAX, 0.05, 1.0);
		Parameter size;
		in_prim.AddParameter(sizeDef, size);

		CRef powerDef = fact.CreateParamDef("power", CValue::siDouble, kParamCaps, "power", "", 500.0, 0.0, DBL_MAX, 0.0, 1500.0);
		Parameter power;
		in_prim.AddParameter(powerDef, power);

		//Visible settings
		CRef rayLengthDef = fact.CreateParamDef("ray_length", CValue::siDouble, kParamCaps, "ray_length", "", 1.5, 0, DBL_MAX, 0.5, 2.0);
		Parameter rayLength;
		in_prim.AddParameter(rayLengthDef, rayLength);

		add_standard_parameters(in_prim, "light_point");

	}
	return CStatus::OK;
}

SICALLBACK cyclesPoint_DefineLayout(CRef& in_ctxt)
{
	Context ctxt(in_ctxt);
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

	oLayout.AddGroup("Null Visible Settings");
	oLayout.AddItem("ray_length", "Ray Length");
	oLayout.EndGroup();

	oLayout.AddGroup("Point Light Settings");
	oLayout.AddItem("power", "Power");
	oLayout.AddItem("size", "Size");
	oLayout.EndGroup();

	oLayout.AddGroup("Generic Settings");
	oLayout.AddItem("max_bounces", "Max Bounces");
	oLayout.AddItem("cast_shadow", "Cast Shadow");
	oLayout.AddItem("multiple_importance", "Multiple Importance");
	oLayout.AddItem("shadow_caustics", "Shadow Caustics");
	oLayout.AddItem("lightgroup", "Light Group");
	oLayout.EndGroup();

	oLayout.AddGroup("Mask");
	oLayout.AddItem("is_shadow_catcher", "Shadow Catcher");
	oLayout.EndGroup();

	oLayout.AddGroup("Ray Visibility");
	oLayout.AddItem("use_camera", "Camera");
	oLayout.AddItem("use_diffuse", "Diffuse");
	oLayout.AddItem("use_glossy", "Glossy");
	oLayout.AddItem("use_transmission", "Transmission");
	oLayout.AddItem("use_scatter", "Volume Scatter");
	oLayout.EndGroup();

	return CStatus::OK;
}

SICALLBACK cyclesPoint_Draw(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}
	CParameterRefArray& params = in_prim.GetParameters();

	double size = params.GetValue("size");
	double ray_length = params.GetValue("ray_length");
	if (g_quadric)
	{
		double vStart = 0.75 * size / sqrt(3.0);
		double vEnd = (0.75 + ray_length) * size / sqrt(3.0);
		GLdouble vertices[16][3];
		vertices[0][0] = vStart;
		vertices[0][1] = vStart;
		vertices[0][2] = vStart;
		vertices[1][0] = vEnd;
		vertices[1][1] = vEnd;
		vertices[1][2] = vEnd;

		vertices[2][0] = vStart;
		vertices[2][1] = vStart;
		vertices[2][2] = -1 * vStart;
		vertices[3][0] = vEnd;
		vertices[3][1] = vEnd;
		vertices[3][2] = -1 * vEnd;

		vertices[4][0] = vStart;
		vertices[4][1] = -1 * vStart;
		vertices[4][2] = vStart;
		vertices[5][0] = vEnd;
		vertices[5][1] = -1 * vEnd;
		vertices[5][2] = vEnd;

		vertices[6][0] = -1 * vStart;
		vertices[6][1] = vStart;
		vertices[6][2] = vStart;
		vertices[7][0] = -1 * vEnd;
		vertices[7][1] = vEnd;
		vertices[7][2] = vEnd;

		vertices[8][0] = -1 * vStart;
		vertices[8][1] = -1 * vStart;
		vertices[8][2] = vStart;
		vertices[9][0] = -1 * vEnd;
		vertices[9][1] = -1 * vEnd;
		vertices[9][2] = vEnd;

		vertices[10][0] = -1 * vStart;
		vertices[10][1] = vStart;
		vertices[10][2] = -1 * vStart;
		vertices[11][0] = -1 * vEnd;
		vertices[11][1] = vEnd;
		vertices[11][2] = -1 * vEnd;

		vertices[12][0] = vStart;
		vertices[12][1] = -1 * vStart;
		vertices[12][2] = -1 * vStart;
		vertices[13][0] = vEnd;
		vertices[13][1] = -1 * vEnd;
		vertices[13][2] = -1 * vEnd;

		vertices[14][0] = -1 * vStart;
		vertices[14][1] = -1 * vStart;
		vertices[14][2] = -1 * vStart;
		vertices[15][0] = -1 * vEnd;
		vertices[15][1] = -1 * vEnd;
		vertices[15][2] = -1 * vEnd;

		::glMatrixMode(GL_MODELVIEW);
		::glPushMatrix();

		::gluDisk(g_quadric, 0, size, 12, 1);
		::glRotated(90.0, 1.0, 0.0, 0.0);
		::gluDisk(g_quadric, 0, size, 12, 1);
		::glRotated(90.0, 0.0, 1.0, 0.0);
		::gluDisk(g_quadric, 0, size, 12, 1);

		if (ray_length > 0.0001)
		{
			::glBegin(GL_LINES);
			::glVertex3dv(vertices[0]);
			::glVertex3dv(vertices[1]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[2]);
			::glVertex3dv(vertices[3]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[4]);
			::glVertex3dv(vertices[5]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[6]);
			::glVertex3dv(vertices[7]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[8]);
			::glVertex3dv(vertices[9]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[10]);
			::glVertex3dv(vertices[11]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[12]);
			::glVertex3dv(vertices[13]);
			::glEnd();

			::glBegin(GL_LINES);
			::glVertex3dv(vertices[14]);
			::glVertex3dv(vertices[15]);
			::glEnd();
		}
		::glPopMatrix();
	}

	return CStatus::OK;
}

SICALLBACK cyclesPoint_BoundingBox(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}

	CParameterRefArray& params = in_prim.GetParameters();
	double size = params.GetValue("size");

	in_ctxt.PutAttribute("LowerBoundX", -size);
	in_ctxt.PutAttribute("LowerBoundY", -size);
	in_ctxt.PutAttribute("LowerBoundZ", -size);
	in_ctxt.PutAttribute("UpperBoundX", size);
	in_ctxt.PutAttribute("UpperBoundY", size);
	in_ctxt.PutAttribute("UpperBoundZ", size);

	return CStatus::OK;
}

//--------------------------
//---------Sun Light--------
//--------------------------

SICALLBACK cyclesSun_Define(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		Factory fact = Application().GetFactory();
		CRef sizeDef = fact.CreateParamDef("angle", CValue::siDouble, kParamCaps, "angle", "", 4.8, 0.0, 180.0, 0.0, 180.0);
		Parameter size;
		in_prim.AddParameter(sizeDef, size);

		CRef powerDef = fact.CreateParamDef("power", CValue::siDouble, kParamCaps, "power", "", 5.0, 0.0, DBL_MAX, 0.0, 15.0);
		Parameter power;
		in_prim.AddParameter(powerDef, power);

		CRef arrowSizeDef = fact.CreateParamDef("arrow_size", CValue::siDouble, kParamCaps, "arrow_size", "", 0.25, 0, DBL_MAX, 0, 2.0);
		CRef arrowLengthDef = fact.CreateParamDef("arrow_length", CValue::siDouble, kParamCaps, "arrow_length", "", 5.0, 0, DBL_MAX, 4.0, 16.0);
		Parameter arrowSize;
		Parameter arrowLength;
		in_prim.AddParameter(arrowSizeDef, arrowSize);
		in_prim.AddParameter(arrowLengthDef, arrowLength);

		add_standard_parameters(in_prim, "light_sun");
	}
	return CStatus::OK;
}

SICALLBACK cyclesSun_DefineLayout(CRef& in_ctxt)
{
	Context ctxt(in_ctxt);
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

	oLayout.AddGroup("Null Visible Settings");
	oLayout.AddItem("arrow_length", "Arrow Length");
	oLayout.AddItem("arrow_size", "Arrow Size");
	oLayout.EndGroup();

	oLayout.AddGroup("Sun Light Settings");
	oLayout.AddItem("power", "Strength");
	oLayout.AddItem("angle", "Angle");
	oLayout.EndGroup();

	oLayout.AddGroup("Generic Settings");
	oLayout.AddItem("max_bounces", "Max Bounces");
	oLayout.AddItem("cast_shadow", "Cast Shadow");
	oLayout.AddItem("multiple_importance", "Multiple Importance");
	oLayout.AddItem("shadow_caustics", "Shadow Caustics");
	oLayout.AddItem("lightgroup", "Light Group");
	oLayout.EndGroup();

	oLayout.AddGroup("Mask");
	oLayout.AddItem("is_shadow_catcher", "Shadow Catcher");
	oLayout.EndGroup();

	oLayout.AddGroup("Ray Visibility");
	oLayout.AddItem("use_camera", "Camera");
	oLayout.AddItem("use_diffuse", "Diffuse");
	oLayout.AddItem("use_glossy", "Glossy");
	oLayout.AddItem("use_transmission", "Transmission");
	oLayout.AddItem("use_scatter", "Volume Scatter");
	oLayout.EndGroup();

	return CStatus::OK;
}

void DrawArrow(double size, double arrowSize, double arrowLength)
{
	GLdouble bigArrow[7][3];
	double aLength = 2 * arrowSize * size;
	double aWidth = arrowSize * size;
	double arrowLineLength = arrowLength * size;
	bigArrow[0][0] = 0;
	bigArrow[0][1] = 0;
	bigArrow[0][2] = 0;

	bigArrow[1][0] = 0;
	bigArrow[1][1] = 0;
	bigArrow[1][2] = -arrowLineLength;

	bigArrow[2][0] = aWidth;
	bigArrow[2][1] = 0;
	bigArrow[2][2] = -arrowLineLength + aLength;

	bigArrow[3][0] = -aWidth;
	bigArrow[3][1] = 0;
	bigArrow[3][2] = -arrowLineLength + aLength;

	bigArrow[4][0] = 0;
	bigArrow[4][1] = aWidth;
	bigArrow[4][2] = -arrowLineLength + aLength;

	bigArrow[5][0] = 0;
	bigArrow[5][1] = -aWidth;
	bigArrow[5][2] = -arrowLineLength + aLength;

	bigArrow[6][0] = 0;
	bigArrow[6][1] = 0;
	bigArrow[6][2] = -arrowLineLength + aLength / 2;

	::glBegin(GL_LINES);
	::glVertex3dv(bigArrow[0]);
	::glVertex3dv(bigArrow[1]);
	::glEnd();

	::glBegin(GL_LINE_STRIP);
	::glVertex3dv(bigArrow[6]);
	::glVertex3dv(bigArrow[4]);
	::glVertex3dv(bigArrow[1]);
	::glVertex3dv(bigArrow[5]);
	::glVertex3dv(bigArrow[6]);
	::glVertex3dv(bigArrow[3]);
	::glVertex3dv(bigArrow[1]);
	::glVertex3dv(bigArrow[2]);
	::glVertex3dv(bigArrow[6]);
	::glEnd();
}

SICALLBACK cyclesSun_Draw(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}
	CParameterRefArray& params = in_prim.GetParameters();

	double angle = params.GetValue("angle");
	double size = tan(angle * PI / 360.0);
	double arrowSize = params.GetValue("arrow_size");
	double arrowLength = params.GetValue("arrow_length");
	if (g_quadric)
	{
		DrawArrow(1.0, arrowSize, arrowLength);

		::glMatrixMode(GL_MODELVIEW);
		::glPushMatrix();

		::gluDisk(g_quadric, 0, size, 12, 1);

		::glPopMatrix();
	}

	return CStatus::OK;
}

SICALLBACK cyclesSun_BoundingBox(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}

	CParameterRefArray& params = in_prim.GetParameters();
	double angle = params.GetValue("angle");
	double size = tan(angle * PI / 360.0);
	double arrowLength = params.GetValue("arrow_length");

	in_ctxt.PutAttribute("LowerBoundX", -size);
	in_ctxt.PutAttribute("LowerBoundY", -size);
	in_ctxt.PutAttribute("LowerBoundZ", -arrowLength);
	in_ctxt.PutAttribute("UpperBoundX", size);
	in_ctxt.PutAttribute("UpperBoundY", size);
	in_ctxt.PutAttribute("UpperBoundZ", 0);

	return CStatus::OK;
}

//--------------------------
//---------Spot Light-------
//--------------------------

SICALLBACK cyclesSpot_Define(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		Factory fact = Application().GetFactory();
		CRef sizeDef = fact.CreateParamDef("size", CValue::siDouble, kParamCaps, "size", "", 0.1, 0.0, DBL_MAX, 0.0, 1.0);
		CRef spotAngleDef = fact.CreateParamDef("spot_angle", CValue::siDouble, kParamCaps, "spot_angle", "", 75.0, 0.0, DBL_MAX, 0.0, 180.0);
		CRef spotSmoothDef = fact.CreateParamDef("spot_smooth", CValue::siDouble, kParamCaps, "spot_smooth", "", 0.15, 0.0, DBL_MAX, 0.0, 1.0);
		Parameter size;
		Parameter spotAngle;
		Parameter spotSmooth;
		in_prim.AddParameter(sizeDef, size);
		in_prim.AddParameter(spotAngleDef, spotAngle);
		in_prim.AddParameter(spotSmoothDef, spotSmooth);

		CRef powerDef = fact.CreateParamDef("power", CValue::siDouble, kParamCaps, "power", "", 500.0, 0.0, DBL_MAX, 0.0, 1500.0);
		Parameter power;
		in_prim.AddParameter(powerDef, power);

		CRef arrowSizeDef = fact.CreateParamDef("arrow_size", CValue::siDouble, kParamCaps, "arrow_size", "", 0.25, 0, DBL_MAX, 0, 2.0);
		CRef arrowLengthDef = fact.CreateParamDef("arrow_length", CValue::siDouble, kParamCaps, "arrow_length", "", 8, 0, DBL_MAX, 4, 16);
		CRef coneLengthDef = fact.CreateParamDef("cone_length", CValue::siDouble, kParamCaps, "cone_length", "", 4, 0, DBL_MAX, 2, 12);
		Parameter arrowSize;
		Parameter arrowLength;
		Parameter coneLength;
		in_prim.AddParameter(arrowSizeDef, arrowSize);
		in_prim.AddParameter(arrowLengthDef, arrowLength);
		in_prim.AddParameter(coneLengthDef, coneLength);

		add_standard_parameters(in_prim, "light_spot");
	}
	return CStatus::OK;
}

SICALLBACK cyclesSpot_DefineLayout(CRef& in_ctxt)
{
	Context ctxt(in_ctxt);
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

	oLayout.AddGroup("Null Visible Settings");
	oLayout.AddItem("arrow_length", "Arrow Length");
	oLayout.AddItem("arrow_size", "Arrow Size");
	oLayout.AddItem("cone_length", "Cone Length");
	oLayout.EndGroup();

	oLayout.AddGroup("Spot Light Settings");
	oLayout.AddItem("power", "Power");
	oLayout.AddItem("size", "Size");
	oLayout.AddItem("spot_angle", "Spot Angle");
	oLayout.AddItem("spot_smooth", "Spot Smooth");
	oLayout.EndGroup();

	oLayout.AddGroup("Generic Settings");
	oLayout.AddItem("max_bounces", "Max Bounces");
	oLayout.AddItem("cast_shadow", "Cast Shadow");
	oLayout.AddItem("multiple_importance", "Multiple Importance");
	oLayout.AddItem("shadow_caustics", "Shadow Caustics");
	oLayout.AddItem("lightgroup", "Light Group");
	oLayout.EndGroup();

	oLayout.AddGroup("Mask");
	oLayout.AddItem("is_shadow_catcher", "Shadow Catcher");
	oLayout.EndGroup();

	oLayout.AddGroup("Ray Visibility");
	oLayout.AddItem("use_camera", "Camera");
	oLayout.AddItem("use_diffuse", "Diffuse");
	oLayout.AddItem("use_glossy", "Glossy");
	oLayout.AddItem("use_transmission", "Transmission");
	oLayout.AddItem("use_scatter", "Volume Scatter");
	oLayout.EndGroup();

	return CStatus::OK;
}

SICALLBACK cyclesSpot_Draw(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}
	CParameterRefArray& params = in_prim.GetParameters();

	double size = params.GetValue("size");
	double angle = params.GetValue("spot_angle");
	double coneLength = params.GetValue("cone_length");
	double arrowSize = params.GetValue("arrow_size");
	double arrowLength = params.GetValue("arrow_length");
	if (g_quadric)
	{
		DrawArrow(1.0, arrowSize, arrowLength);

		double zShift = -coneLength;
		double endRadius = /*size + */(-1) * zShift * tan(angle * PI / 360.0);
		GLdouble vertices[8][3];
		vertices[0][0] = size;
		vertices[0][1] = 0;
		vertices[0][2] = 0;
		vertices[1][0] = endRadius;
		vertices[1][1] = 0;
		vertices[1][2] = zShift;

		vertices[2][0] = 0;
		vertices[2][1] = size;
		vertices[2][2] = 0;
		vertices[3][0] = 0;
		vertices[3][1] = endRadius;
		vertices[3][2] = zShift;

		vertices[4][0] = -size;
		vertices[4][1] = 0;
		vertices[4][2] = 0;
		vertices[5][0] = -endRadius;
		vertices[5][1] = 0;
		vertices[5][2] = zShift;

		vertices[6][0] = 0;
		vertices[6][1] = -size;
		vertices[6][2] = 0;
		vertices[7][0] = 0;
		vertices[7][1] = -endRadius;
		vertices[7][2] = zShift;


		::glMatrixMode(GL_MODELVIEW);
		::glPushMatrix();

		::glBegin(GL_LINES);
		::glVertex3dv(vertices[0]);
		::glVertex3dv(vertices[1]);
		::glEnd();
		::glBegin(GL_LINES);
		::glVertex3dv(vertices[2]);
		::glVertex3dv(vertices[3]);
		::glEnd();
		::glBegin(GL_LINES);
		::glVertex3dv(vertices[4]);
		::glVertex3dv(vertices[5]);
		::glEnd();
		::glBegin(GL_LINES);
		::glVertex3dv(vertices[6]);
		::glVertex3dv(vertices[7]);
		::glEnd();

		::gluDisk(g_quadric, 0, size, 12, 1);
		::glTranslated(0, 0, zShift);
		::gluDisk(g_quadric, 0, endRadius, 12, 1);

		::glPopMatrix();
	}

	return CStatus::OK;
}

SICALLBACK cyclesSpot_BoundingBox(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}

	CParameterRefArray& params = in_prim.GetParameters();
	double size = params.GetValue("size");
	double angle = params.GetValue("spot_angle");
	double coneLength = params.GetValue("cone_length");
	double zShift = -coneLength;
	double endRadius = (-1) * zShift * tan(angle * PI / 360.0);
	double arrowLength = params.GetValue("arrow_length");

	in_ctxt.PutAttribute("LowerBoundX", -endRadius);
	in_ctxt.PutAttribute("LowerBoundY", -endRadius);
	in_ctxt.PutAttribute("LowerBoundZ", std::min(-arrowLength, zShift));
	in_ctxt.PutAttribute("UpperBoundX", endRadius);
	in_ctxt.PutAttribute("UpperBoundY", endRadius);
	in_ctxt.PutAttribute("UpperBoundZ", 0);

	return CStatus::OK;
}

//--------------------------
//---------Area Light-------
//--------------------------

SICALLBACK cyclesArea_Define(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		Factory fact = Application().GetFactory();
		CRef sizeUDef = fact.CreateParamDef("sizeU", CValue::siDouble, kParamCaps, "sizeU", "", 1.0, 0.000001, DBL_MAX, 0.05, 4.0);
		CRef sizeVDef = fact.CreateParamDef("sizeV", CValue::siDouble, kParamCaps, "sizeV", "", 1.0, 0.000001, DBL_MAX, 0.05, 4.0);
		CRef isPortalDef = fact.CreateParamDef("is_portal", CValue::siBool, kParamCaps, "is_portal", "", false, 0, 1, 0, 1);
		Parameter sizeU;
		Parameter sizeV;
		Parameter isPortal;
		in_prim.AddParameter(sizeUDef, sizeU);
		in_prim.AddParameter(sizeVDef, sizeV);
		in_prim.AddParameter(isPortalDef, isPortal);

		CRef powerDef = fact.CreateParamDef("power", CValue::siDouble, kParamCaps, "power", "", 250.0, 0.0, DBL_MAX, 0.0, 750.0);
		Parameter power;
		in_prim.AddParameter(powerDef, power);
		CRef shapeDef = fact.CreateParamDef("shape", CValue::siDouble, kParamCaps, "shape", "", 0, 0, 1, 0, 1);
		Parameter shape;
		in_prim.AddParameter(shapeDef, shape);

		CRef spreadDef = fact.CreateParamDef("spread", CValue::siDouble, kParamCaps, "spread", "", 180.0, 1.0, 180.0, 1.0, 180.0);
		Parameter spread;
		in_prim.AddParameter(spreadDef, spread);


		CRef arrowSizeDef = fact.CreateParamDef("arrow_size", CValue::siDouble, kParamCaps, "arrow_size", "", 0.05, 0, DBL_MAX, 0, 0.1);
		CRef arrowLengthDef = fact.CreateParamDef("arrow_length", CValue::siDouble, kParamCaps, "arrow_length", "", 1, 0, DBL_MAX, 0, 2.0);
		Parameter arrowSize;
		Parameter arrowLength;
		in_prim.AddParameter(arrowSizeDef, arrowSize);
		in_prim.AddParameter(arrowLengthDef, arrowLength);

		add_standard_parameters(in_prim, "light_area");
	}
	return CStatus::OK;
}

SICALLBACK cyclesArea_DefineLayout(CRef& in_ctxt)
{
	Context ctxt(in_ctxt);
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

	oLayout.AddGroup("Null Visible Settings");
	oLayout.AddItem("arrow_length", "Arrow Length");
	oLayout.AddItem("arrow_size", "Arrow Size");
	oLayout.EndGroup();

	oLayout.AddGroup("Area Light Settings");
	oLayout.AddItem("power", "Power");
	CValueArray shape_combo(4);
	shape_combo[0] = "Rectangle"; shape_combo[1] = 0;
	shape_combo[2] = "Ellipse"; shape_combo[3] = 1;
	oLayout.AddEnumControl("shape", shape_combo, "Shape");
	oLayout.AddItem("sizeU", "Size U");
	oLayout.AddItem("sizeV", "Size V");
	oLayout.AddItem("spread", "Spread");
	oLayout.AddItem("is_portal", "Portal");
	oLayout.EndGroup();

	oLayout.AddGroup("Generic Settings");
	oLayout.AddItem("max_bounces", "Max Bounces");
	oLayout.AddItem("cast_shadow", "Cast Shadow");
	oLayout.AddItem("multiple_importance", "Multiple Importance");
	oLayout.AddItem("shadow_caustics", "Shadow Caustics");
	oLayout.AddItem("lightgroup", "Light Group");
	oLayout.EndGroup();

	oLayout.AddGroup("Mask");
	oLayout.AddItem("is_shadow_catcher", "Shadow Catcher");
	oLayout.EndGroup();

	oLayout.AddGroup("Ray Visibility");
	oLayout.AddItem("use_camera", "Camera");
	oLayout.AddItem("use_diffuse", "Diffuse");
	oLayout.AddItem("use_glossy", "Glossy");
	oLayout.AddItem("use_transmission", "Transmission");
	oLayout.AddItem("use_scatter", "Volume Scatter");
	oLayout.EndGroup();

	return CStatus::OK;
}

SICALLBACK cyclesArea_Draw(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}
	CParameterRefArray& params = in_prim.GetParameters();

	double sizeU = params.GetValue("sizeU");
	sizeU = sizeU / 2;
	double sizeV = params.GetValue("sizeV");
	sizeV = sizeV / 2;
	double arrowSize = params.GetValue("arrow_size");
	double arrowLength = params.GetValue("arrow_length");
	int area_shape = params.GetValue("shape");
	if (g_quadric)
	{
		DrawArrow(sizeU + sizeV, arrowSize, arrowLength);
		if (area_shape == 0)
		{// rectangle
			GLdouble shape_verts[4][3];
			shape_verts[0][0] = sizeU;
			shape_verts[0][1] = sizeV;
			shape_verts[0][2] = 0;
			shape_verts[1][0] = -sizeU;
			shape_verts[1][1] = sizeV;
			shape_verts[1][2] = 0;
			shape_verts[2][0] = -sizeU;
			shape_verts[2][1] = -sizeV;
			shape_verts[2][2] = 0;
			shape_verts[3][0] = sizeU;
			shape_verts[3][1] = -sizeV;
			shape_verts[3][2] = 0;

			::glBegin(GL_LINE_STRIP);
			for (size_t i = 0; i < 4; i++)
			{
				::glVertex3dv(shape_verts[i]);
			}
			::glVertex3dv(shape_verts[0]);
			::glEnd();
		}
		else
		{// ellipse
			GLdouble shape_verts[36][3];
			double step = 2 * PI / 36;
			for (size_t i = 0; i < 36; i++)
			{
				shape_verts[i][0] = sizeU * cos(step * i);
				shape_verts[i][1] = sizeV * sin(step * i);
				shape_verts[i][2] = 0;
			}
			::glBegin(GL_LINE_STRIP);
			for (size_t i = 0; i < 36; i++)
			{
				::glVertex3dv(shape_verts[i]);
			}
			::glVertex3dv(shape_verts[0]);
			::glEnd();
		}
	}

	return CStatus::OK;
}

SICALLBACK cyclesArea_BoundingBox(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}

	CParameterRefArray& params = in_prim.GetParameters();
	double sizeU = params.GetValue("sizeU");
	double sizeV = params.GetValue("sizeV");
	double arrowLength = params.GetValue("arrow_length");

	in_ctxt.PutAttribute("LowerBoundX", -sizeU / 2);
	in_ctxt.PutAttribute("LowerBoundY", -sizeV / 2);
	in_ctxt.PutAttribute("LowerBoundZ", -arrowLength * (sizeU / 2 + sizeV / 2));
	in_ctxt.PutAttribute("UpperBoundX", sizeU / 2);
	in_ctxt.PutAttribute("UpperBoundY", sizeV / 2);
	in_ctxt.PutAttribute("UpperBoundZ", 0);

	return CStatus::OK;
}

//--------------------------
//------Background Light----
//--------------------------

SICALLBACK cyclesBackground_Define(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (in_prim.IsValid())
	{
		Factory fact = Application().GetFactory();
		CRef typeDef = fact.CreateParamDef("cycles_primitive_type", CValue::siString, "light_background");
		CRef sizeVisibleDef = fact.CreateParamDef("size_visible", CValue::siDouble, kParamCaps, "size_visible", "", 5.0, 0.000001, DBL_MAX, 0, 10.0);

		Parameter type;
		Parameter sizeVisible;

		in_prim.AddParameter(typeDef, type);
		in_prim.AddParameter(sizeVisibleDef, sizeVisible);
	}
	return CStatus::OK;
}

SICALLBACK cyclesBackground_DefineLayout(CRef& in_ctxt)
{
	Context ctxt(in_ctxt);
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

	oLayout.AddGroup("Null Visible Settings");
	oLayout.AddItem("size_visible", "Size");
	oLayout.EndGroup();

	return CStatus::OK;
}

SICALLBACK cyclesBackground_Draw(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}
	CParameterRefArray& params = in_prim.GetParameters();

	double size = params.GetValue("size_visible");
	if (g_quadric)
	{
		GLdouble arcsVerts[18][3];
		for (int i = 0; i < 9; i++)
		{
			arcsVerts[i][0] = size * cos(i * PI / 8);
			arcsVerts[i][1] = size * sin(i * PI / 8);
			arcsVerts[i][2] = 0;
		}
		for (int i = 0; i < 9; i++)
		{
			arcsVerts[9 + i][0] = 0;
			arcsVerts[9 + i][1] = size * sin(i * PI / 8);
			arcsVerts[9 + i][2] = size * cos(i * PI / 8);
		}

		::glBegin(GL_LINE_STRIP);
		::glVertex3dv(arcsVerts[0]);
		::glVertex3dv(arcsVerts[1]);
		::glVertex3dv(arcsVerts[2]);
		::glVertex3dv(arcsVerts[3]);
		::glVertex3dv(arcsVerts[4]);
		::glVertex3dv(arcsVerts[5]);
		::glVertex3dv(arcsVerts[6]);
		::glVertex3dv(arcsVerts[7]);
		::glVertex3dv(arcsVerts[8]);
		::glEnd();

		::glBegin(GL_LINE_STRIP);
		::glVertex3dv(arcsVerts[9]);
		::glVertex3dv(arcsVerts[10]);
		::glVertex3dv(arcsVerts[11]);
		::glVertex3dv(arcsVerts[12]);
		::glVertex3dv(arcsVerts[13]);
		::glVertex3dv(arcsVerts[14]);
		::glVertex3dv(arcsVerts[15]);
		::glVertex3dv(arcsVerts[16]);
		::glVertex3dv(arcsVerts[17]);
		::glEnd();

		::glMatrixMode(GL_MODELVIEW);
		::glPushMatrix();
		::glRotated(90.0, 1.0, 0.0, 0.0);
		::gluDisk(g_quadric, 0, size, 16, 1);
		::glPopMatrix();
	}

	return CStatus::OK;
}

SICALLBACK cyclesBackground_BoundingBox(const CRef& in_ref)
{
	Context in_ctxt(in_ref);
	CustomPrimitive in_prim(in_ctxt.GetSource());
	if (!in_prim.IsValid())
	{
		return CStatus::Fail;
	}

	CParameterRefArray& params = in_prim.GetParameters();
	double size = params.GetValue("size_visible");

	in_ctxt.PutAttribute("LowerBoundX", -size);
	in_ctxt.PutAttribute("LowerBoundY", -size);
	in_ctxt.PutAttribute("LowerBoundZ", -size);
	in_ctxt.PutAttribute("UpperBoundX", size);
	in_ctxt.PutAttribute("UpperBoundY", size);
	in_ctxt.PutAttribute("UpperBoundZ", size);

	return CStatus::OK;
}