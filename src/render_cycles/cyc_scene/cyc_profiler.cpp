#include "cyc_profiler.h"

ProfilerContext::ProfilerContext() {
	reset();
}

ProfilerContext::~ProfilerContext() {
	reset();
}

void ProfilerContext::reset() {
	buffer.clear();

	prebake_start_time.reset();
	prebake_finish_time.reset();
	prescene_start_time.reset();
	prescene_finish_time.reset();
	postscene_start_time.reset();
	postscene_finish_time.reset();

	id_to_name.clear();
	id_to_type.clear();
	id_to_start.clear();
	id_to_finish.clear();
}

void ProfilerContext::prebake_start() {
	prebake_start_time = std::chrono::steady_clock::now();
}

void ProfilerContext::prebake_finish() {
	prebake_finish_time = std::chrono::steady_clock::now();
}

void ProfilerContext::prescene_start() {
	prescene_start_time = std::chrono::steady_clock::now();
}

void ProfilerContext::prescene_finish() {
	prescene_finish_time = std::chrono::steady_clock::now();
}

void ProfilerContext::postscene_start() {
	postscene_start_time = std::chrono::steady_clock::now();
}

void ProfilerContext::postscene_finish() {
	postscene_finish_time = std::chrono::steady_clock::now();
}

void ProfilerContext::scene_item_start(SyncType sync_type, ULONG xsi_id, const XSI::CString& xsi_name) {
	id_to_name[xsi_id] = xsi_name;
	id_to_type[xsi_id] = sync_type;

	id_to_start[xsi_id] = std::chrono::steady_clock::now();
}

void ProfilerContext::scene_item_finish(ULONG xsi_id) {
	id_to_finish[xsi_id] = std::chrono::steady_clock::now();
}

std::vector<ULONG> ProfilerContext::geather_ids_for_type(SyncType sync_type) {
	buffer.clear();

	for (const auto& [xsi_id, type] : id_to_type) {
		if (type == sync_type && id_to_start.contains(xsi_id) && id_to_finish.contains(xsi_id) && id_to_name.contains(xsi_id)) {
			buffer.push_back(xsi_id);
		}
	}

	return buffer;
}

std::string ProfilerContext::build_output_str(const std::string& title, const std::vector<ULONG>& ids, const std::string& tab_title, const std::string& tab_content) {
	std::string to_return = "";
	to_return += tab_title + title;

	for (size_t i = 0; i < ids.size(); i++) {
		ULONG xsi_id = ids[i];
		XSI::CString name = id_to_name[xsi_id];
		m_time start = id_to_start[xsi_id];
		m_time finish = id_to_finish[xsi_id];

		to_return += tab_content + name.GetAsciiString() + ": " + time_to_string(start, finish);
	}

	return to_return;
}

XSI::CString ProfilerContext::message() {
	std::string tab = "\n" + std::string(2, ' ');
	std::string tab_2 = "\n" + std::string(4, ' ');
	std::string tab_3 = "\n" + std::string(6, ' ');

	std::string out_string = "Scene synchronisation times:";
	if (prebake_start_time.has_value() && prebake_finish_time.has_value()) {
		out_string += tab + "Bake: " + time_to_string(prebake_start_time, prebake_finish_time);
	}
	if (prescene_start_time.has_value() && prescene_finish_time.has_value()) {
		out_string += tab + "Before scene sync: " + time_to_string(prescene_start_time, prescene_finish_time);
	}

	buffer = geather_ids_for_type(SyncType::Camera); if (buffer.size() > 0) { out_string += build_output_str("Cameras:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Material); if (buffer.size() > 0) { out_string += build_output_str("Materials:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Light); if (buffer.size() > 0) { out_string += build_output_str("Lights:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Volume); if (buffer.size() > 0) { out_string += build_output_str("Volumes:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Curve); if (buffer.size() > 0) { out_string += build_output_str("Curves:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Surface); if (buffer.size() > 0) { out_string += build_output_str("Surfaces:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Hair); if (buffer.size() > 0) { out_string += build_output_str("Hairs:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Strands); if (buffer.size() > 0) { out_string += build_output_str("Strands:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Polymesh); if (buffer.size() > 0) { out_string += build_output_str("Polymeshes:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::Points); if (buffer.size() > 0) { out_string += build_output_str("ICE Points:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::VDB); if (buffer.size() > 0) { out_string += build_output_str("VDBs:", buffer, tab, tab_2); }
	buffer = geather_ids_for_type(SyncType::PoincloudInstances); if (buffer.size() > 0) { out_string += build_output_str("Pointcloud Instances:", buffer, tab, tab_2); }

	if (postscene_start_time.has_value() && postscene_finish_time.has_value()) {
		out_string += tab + "After scene sync: " + time_to_string(postscene_start_time, postscene_finish_time);
	}
	
	return out_string.c_str();
}

SyncType update_type_to_sync_type(UpdateType update_type) {
	// WARNING:this map should be synced with render_engine_cyc.cpp
	// XSI::CStatus RenderEngineCyc::update_scene(XSI::X3DObject& xsi_object, const UpdateType update_type)
	if (update_type == UpdateType_Camera) { return SyncType::Camera; }
	else if (update_type == UpdateType_Material) { return SyncType::Material; }
	else if (update_type == UpdateType_Mesh) { return SyncType::Polymesh; }
	else if (update_type == UpdateType_Hair) { return SyncType::Hair; }
	else if (update_type == UpdateType_XsiLight) { return SyncType::Light; }
	else if (update_type == UpdateType_GlobalAmbient) { return SyncType::Light; }
	else if (update_type == UpdateType_LightPrimitive) { return SyncType::Light; }
	else if (update_type == UpdateType_Curve) { return SyncType::Curve; }
	else if (update_type == UpdateType_Surface) { return SyncType::Surface; }
	else if (update_type == UpdateType_VDBPrimitive) { return SyncType::VDB; }
	else if (update_type == UpdateType_MeshProperty) { return SyncType::Polymesh; }
	else if (update_type == UpdateType_HairProperty) { return SyncType::Hair; }
	else if (update_type == UpdateType_CurveProperty) { return SyncType::Curve; }
	else if (update_type == UpdateType_SurfaceProperty) { return SyncType::Surface; }
	else if (update_type == UpdateType_VolumeProperty) { return SyncType::Volume; }
	// TODO: here it's not convinient to define the actual type of updated pointcloud object
	// it can be strands or points or instances
	// so, skip profiling when update it

	return SyncType::Unknown;
}

long long to_milliseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point finish) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
}

long long to_microseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point finish) {
	return std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
}

// coded by DeepSeek 2026-07-27
std::string time_to_string(m_time start, m_time finish) {
	if (!start.has_value() || !finish.has_value()) {
		return "unknown";
	}

	std::chrono::nanoseconds diff = std::chrono::duration_cast<std::chrono::nanoseconds>(*finish - *start);
	double ms = diff.count() / 1e6;
	std::string s = std::format("{:.3f}", ms);

	while (!s.empty() && (s.back() == '0' || s.back() == '.')) {
		if (s.back() == '.') { s.pop_back(); break; }
		s.pop_back();
	}

	size_t pos = s.find('.');
	std::string int_part = (pos == std::string::npos) ? s : s.substr(0, pos);
	std::string frac_part = (pos == std::string::npos) ? "" : s.substr(pos);

	std::string result;
	int count = 0;
	for (auto it = int_part.rbegin(); it != int_part.rend(); ++it) {
		if (count && count % 3 == 0) result.push_back(' ');
		result.push_back(*it);
		++count;
	}
	std::reverse(result.begin(), result.end());

	return result + frac_part + " ms";
}