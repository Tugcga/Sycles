#pragma once
#include <xsi_application.h>

#include "../../render_base/type_enums.h"

#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <map>

// measure time
using m_time = std::optional<std::chrono::steady_clock::time_point>;

enum SyncType
{
	Unknown,
	BakePreprocess,
	ScenePreprocess,
	ScenePostprocess,
	Material,
	Camera,
	Light,
	Volume,
	Polymesh,
	Curve,
	Surface,
	Hair,
	Strands,
	Points,
	VDB,
	PoincloudInstances
};

class ProfilerContext {
public:
	ProfilerContext();
	~ProfilerContext();

	void reset();

	void prebake_start(); void prebake_finish();
	void prescene_start(); void prescene_finish();
	void postscene_start(); void postscene_finish();
	void scene_item_start(SyncType sync_type, ULONG xsi_id, const XSI::CString&  xsi_name); void scene_item_finish(ULONG xsi_id);

	XSI::CString message();

private:
	std::vector<ULONG> buffer;  // used for iutput ids

	m_time prebake_start_time; m_time prebake_finish_time;
	m_time prescene_start_time; m_time prescene_finish_time;
	m_time postscene_start_time; m_time postscene_finish_time;

	std::map<ULONG, XSI::CString> id_to_name;  // store object names by their ids
	std::map<ULONG, SyncType> id_to_type;
	std::map<ULONG, m_time> id_to_start;
	std::map<ULONG, m_time> id_to_finish;

	std::vector<ULONG> geather_ids_for_type(SyncType sync_type);
	std::string build_output_str(const std::string &title, const std::vector<ULONG> &ids, const std::string &tab_title, const std::string& tab_content);
};

SyncType update_type_to_sync_type(UpdateType update_type);
long long to_milliseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point finish);
long long to_microseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point finish);
std::string time_to_string(m_time start, m_time finish);