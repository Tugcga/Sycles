#pragma once

enum RenderType
{
	RenderType_Pass,
	RenderType_Shaderball,
	RenderType_Region,
	RenderType_Rendermap,
	RenderType_Export,
	RenderType_Unknown
};

enum UpdateType
{
	UpdateType_Undefined,
	UpdateType_Camera,
	UpdateType_Transform,
	UpdateType_Material,
	UpdateType_Mesh,
	UpdateType_Pointcloud,
	UpdateType_Hair,
	UpdateType_XsiLight,
	UpdateType_Visibility,
	UpdateType_Subdivision,
	UpdateType_Render,
	UpdateType_GlobalAmbient,
	UpdateType_Pass
};

enum CameraType
{
	CameraType_General,
	CameraType_Panoramic
};

enum ShaderballType
{
	ShaderballType_Material,
	ShaderballType_Shader,
	ShaderballType_Texture,
	ShaderballType_Unknown
};