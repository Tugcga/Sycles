#include <unordered_map>
#include <vector>
#include <string>

#include <xsi_x3dobject.h>

void extend_map(std::unordered_map<std::string, std::vector<ULONG>>& map, std::string key, ULONG value)
{
	if (map.contains(key))
	{
		map[key].push_back(value);
	}
	else
	{
		map[key] = { value };
	}
}

void extend_map(std::unordered_map<ULONG, std::vector<ULONG>>& map, ULONG key, ULONG value)
{
	if (map.contains(key))
	{
		map[key].push_back(value);
	}
	else
	{
		map[key] = { value };
	}
}

void extend_map_by_group_members(std::unordered_map<ULONG, std::vector<ULONG>>& map, const XSI::CRefArray& group_members, const std::vector<ULONG>& light_ids)
{
	LONG members_count = group_members.GetCount();
	for (size_t j = 0; j < members_count; j++)
	{
		XSI::X3DObject member(group_members[j]);
		if (member.IsValid())
		{
			ULONG member_id = member.GetObjectID();
			for (size_t i = 0; i < light_ids.size(); i++)
			{
				extend_map(map, member_id, light_ids[i]);
			}
		}
	}
}