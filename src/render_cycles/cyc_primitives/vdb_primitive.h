#pragma once
#include <xsi_application.h>
#include <xsi_customprimitive.h>
#include <xsi_time.h>

#include <openvdb/openvdb.h>

#include "../../utilities/logs.h"
#include "../../utilities/strings.h"

struct VDBData
{
	VDBData()
	{
		reset();
	}

	void reset()
	{
		is_valid = false;
		grids_count = 0;
		grids.resize(0);
		grids.shrink_to_fit();
		grid_names.resize(0);
		grid_names.shrink_to_fit();
	}

	void init(XSI::CString& file_path)
	{
		reset();

		if (FILE* test_file = fopen(file_path.GetAsciiString(), "r"))
		{// if file exists
			fclose(test_file);
			openvdb::io::File file(file_path.GetAsciiString());

			try
			{
				file.open();
				for (openvdb::io::File::NameIterator nameIter = file.beginName(); nameIter != file.endName(); ++nameIter)
				{
					openvdb::Name grid_name = nameIter.gridName();
					openvdb::GridBase::Ptr grid = file.readGrid(grid_name);

					grids.push_back(grid);
					grid_names.push_back(XSI::CString(grid_name.c_str()));

					grids_count++;
				}
				is_valid = true;
				source_path = file_path;
				file.close();
			}
			catch (openvdb::Exception& e)
			{
				log_message(XSI::CString("[VDB Primitive]: ") + XSI::CString(e.what()), XSI::siWarningMsg);
			}
		}
	}

	double* get_bb(ULONG index)
	{
		openvdb::CoordBBox bb = grids[index]->evalActiveVoxelBoundingBox();
		openvdb::Vec3d minp = grids[index]->indexToWorld(bb.min());
		openvdb::Vec3d maxp = grids[index]->indexToWorld(bb.max());

		return new double[6]{ minp.x(), minp.y(), minp.z(), maxp.x(), maxp.y(), maxp.z() };
	}

	std::vector<XSI::CString> get_description(ULONG index)
	{
		std::vector<XSI::CString> to_return(0);
		if (index >= grids_count)
		{
			to_return.push_back(XSI::CString("Invalid grid index"));
			return to_return;
		}
		else
		{
			XSI::CString str;
			str += XSI::CString("Voxels count = ");
			str += XSI::CString(grids[index]->activeVoxelCount());
			to_return.push_back(str);

			str = XSI::CString("Voxel size = ");
			openvdb::Vec3d size = grids[index]->transform().voxelSize();
			str += XSI::CString("(") + XSI::CString(size.x()) + ", " + XSI::CString(size.y()) + ", " + XSI::CString(size.z()) + ")";
			to_return.push_back(str);

			str = XSI::CString("Data type = ");
			str += XSI::CString(grids[index]->valueType().c_str());
			to_return.push_back(str);

			str = XSI::CString("\nMetadata:");
			to_return.push_back(str);
			bool is_metadata = false;
			for (openvdb::MetaMap::MetaIterator iter = grids[index]->beginMeta(); iter != grids[index]->endMeta(); ++iter)
			{
				is_metadata = true;
				const std::string& name = iter->first;
				openvdb::Metadata::Ptr value = iter->second;
				std::string valueAsString = value->str();
				openvdb::Name type_name = value->typeName();
				str = XSI::CString(name.c_str()) + XSI::CString(" = ") + XSI::CString(valueAsString.c_str());
				to_return.push_back(str);
			}
			if (!is_metadata)
			{
				str = XSI::CString("No metadata in the file");
				to_return.push_back(str);
			}
			return to_return;
		}
	}

	int get_grid_index(const XSI::CString& name) const
	{
		for (ULONG i = 0; i < grids_count; i++)
		{
			if (grid_names[i] == name)
			{
				return i;
			}
		}
		return -1;
	}

	bool is_valid;
	XSI::CString source_path;
	ULONG grids_count;
	std::vector<openvdb::GridBase::Ptr> grids;
	std::vector<XSI::CString> grid_names;
};

struct VDBIdentifier
{
	XSI::CString full_path;  // distinct different datas in the cache by path to the vdb-file
	XSI::CString obj_name;
	ULONG obj_id;
};

struct VDBPrimitivesDataContainer
{
	bool find_index_by_path(VDBIdentifier& id, ULONG& output)
	{
		for (ULONG i = 0; i < vdb_ids.size(); i++)
		{
			if (vdb_ids[i].full_path == id.full_path)
			{
				output = i;
				return true;
			}
		}
		return false;
	}

	void find_indexes_by_name(const XSI::CString& name, ccl::array<ULONG>& output)
	{
		for (ULONG i = 0; i < vdb_ids.size(); i++)
		{
			if (vdb_ids[i].obj_name == name)
			{
				output.push_back_slow(i);
			}
		}
	}

	VDBData get(XSI::CustomPrimitive& in_prim)
	{
		ULONG prim_id = in_prim.GetObjectID();
		XSI::CParameterRefArray& params = in_prim.GetParameters();
		XSI::CString file_path = vdbprimitive_inputs_to_path(params, XSI::CTime());
		if (file_path.Length() > 0)
		{
			// try to find the vdb
			ULONG index;
			VDBIdentifier id;
			id.full_path = file_path;
			id.obj_id = prim_id;
			id.obj_name = in_prim.GetName();
			if (find_index_by_path(id, index))
			{
				return vdb_datas[index];
			}
			else
			{//the primitive is new, try to add it
				VDBData data;
				data.init(file_path);
				if (data.is_valid)
				{
					vdb_ids.push_back(id);
					vdb_datas.push_back(data);
				}
				return data;
			}
		}
		else
		{
			VDBData data;
			data.is_valid = false;
			return data;
		}
	}

	//call when we delete object from the scene
	//name is the name of deleted object
	//we should delete all datas with given name
	void remove(const XSI::CString& name)
	{
		ULONG index = 0;
		ccl::array<ULONG> indexes;
		find_indexes_by_name(name, indexes);
		if (indexes.size() > 0)
		{
			for (int i = indexes.size() - 1; i >= 0; i--)
			{// iterate from the end to start
				ULONG index = indexes[i];

				vdb_ids.erase(vdb_ids.begin() + index);
				// clear vdb data
				// reset each grid
				for (ULONG i = 0; i < vdb_datas[index].grids.size(); i++)
				{
					vdb_datas[index].grids[i].reset();
				}
				// then clear lists
				vdb_datas[index].grids.clear();
				vdb_datas[index].grids.shrink_to_fit();
				vdb_datas[index].grid_names.clear();
				vdb_datas[index].grid_names.shrink_to_fit();
				// delete also data from the list
				vdb_datas.erase(vdb_datas.begin() + index);
			}
		}
	}

	// call when reload a scene, here we should clear data about all vdbs
	void clear()
	{
		for (ULONG i = 0; i < vdb_datas.size(); i++)
		{
			for (ULONG j = 0; j < vdb_datas[i].grids.size(); j++)
			{
				vdb_datas[i].grids[j].reset();
			}
			vdb_datas[i].grids.clear();
			vdb_datas[i].grids.shrink_to_fit();
			vdb_datas[i].grid_names.clear();
			vdb_datas[i].grid_names.shrink_to_fit();
		}
		vdb_ids.clear();
		vdb_ids.shrink_to_fit();
		vdb_datas.clear();
		vdb_datas.shrink_to_fit();
	}

	std::vector<VDBIdentifier> vdb_ids;
	std::vector<VDBData> vdb_datas;
};

VDBData get_vdb_data(XSI::CustomPrimitive& in_prim);