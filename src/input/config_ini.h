#pragma once

// these strucs store paramters from config ini-file
struct ConfigShaderball
{
	ULONG samples;
	ULONG max_bounces;
	ULONG diffuse_bounces;
	ULONG glossy_bounces;
	ULONG transmission_bounces;
	ULONG transparent_bounces;
	ULONG volume_bounces;
	bool use_ocl;
	float clamp_direct;
	float clamp_indirect;
};

struct ConfigRender
{
	ULONG devices;
};

struct ConfigSeries
{
	bool save_intermediate;
	bool save_albedo;
	XSI::CString albedo_prefix;
	bool save_normal;
	XSI::CString normal_prefix;
	bool save_beauty;
	XSI::CString beauty_prefix;
	ULONG sampling_step;
	ULONG sampling_size;
	XSI::CString sampling_start_separator;
	XSI::CString sampling_middle_separator;
	XSI::CString sampling_postfix;
};

// this struct store all parameters from input ini-file as separate structs
struct InputConfig
{
	bool is_init;
	ConfigShaderball shaderball;
	ConfigRender render;
	ConfigSeries series;
};