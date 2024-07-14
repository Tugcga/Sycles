#pragma once
#include <unordered_map>

#include <xsi_ref.h>

void extend_map(std::unordered_map<std::string, std::vector<ULONG>>& map, std::string key, ULONG value);
void extend_map(std::unordered_map<ULONG, std::vector<ULONG>>& map, ULONG key, ULONG value);

void extend_map_by_group_members(std::unordered_map<ULONG, std::vector<ULONG>>& map, const XSI::CRefArray& group_members, const std::vector<ULONG>& light_ids);