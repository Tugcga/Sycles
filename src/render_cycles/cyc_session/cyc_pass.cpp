#include <set>

#include "scene/scene.h"
#include "scene/pass.h"

#include <xsi_application.h>

#include "../../utilities/logs.h"
#include "../cyc_output/output_context.h"

ccl::Pass* pass_add(ccl::Scene* scene, ccl::PassType type, ccl::ustring name, ccl::PassMode mode = ccl::PassMode::DENOISED)
{
	ccl::Pass* pass = scene->create_node<ccl::Pass>();

	pass->set_type(type);
	pass->set_name(name);
	pass->set_mode(mode);

	return pass;
}

int get_pass_components(ccl::PassType pass_type)
{
	ccl::PassInfo pass_info = ccl::Pass::get_info(pass_type);
	return pass_info.num_components;
}

ccl::PassType channel_to_pass_type(const XSI::CString &channel_name)
{
	if (channel_name == "Sycles Combined" || channel_name == "Main") { return ccl::PASS_COMBINED; }
	else if (channel_name == "Sycles Depth") { return ccl::PASS_DEPTH; }
	else if (channel_name == "Sycles Position") { return ccl::PASS_POSITION; }
	else if (channel_name == "Sycles Normal") { return ccl::PASS_NORMAL; }
	else if (channel_name == "Sycles UV") { return ccl::PASS_UV; }
	else if (channel_name == "Sycles Object ID") { return ccl::PASS_OBJECT_ID; }
	else if (channel_name == "Sycles Material ID") { return ccl::PASS_MATERIAL_ID; }
	else if (channel_name == "Sycles Diffuse Color") { return ccl::PASS_DIFFUSE_COLOR; }
	else if (channel_name == "Sycles Glossy Color") { return ccl::PASS_GLOSSY_COLOR; }
	else if (channel_name == "Sycles Transmission Color") { return ccl::PASS_TRANSMISSION_COLOR; }
	else if (channel_name == "Sycles Diffuse Indirect") { return ccl::PASS_DIFFUSE_INDIRECT; }
	else if (channel_name == "Sycles Glossy Indirect") { return ccl::PASS_GLOSSY_INDIRECT; }
	else if (channel_name == "Sycles Transmission Indirect") { return ccl::PASS_TRANSMISSION_INDIRECT; }
	else if (channel_name == "Sycles Diffuse Direct") { return ccl::PASS_DIFFUSE_DIRECT; }
	else if (channel_name == "Sycles Glossy Direct") { return ccl::PASS_GLOSSY_DIRECT; }
	else if (channel_name == "Sycles Transmission Direct") { return ccl::PASS_TRANSMISSION_DIRECT; }
	else if (channel_name == "Sycles Emission") { return ccl::PASS_EMISSION; }
	else if (channel_name == "Sycles Background") { return ccl::PASS_BACKGROUND; }
	else if (channel_name == "Sycles AO") { return ccl::PASS_AO; }
	else if (channel_name == "Sycles Shadow Catcher") { return ccl::PASS_SHADOW_CATCHER; }
	else if (channel_name == "Sycles Shadow") { return ccl::PASS_SHADOW; }
	else if (channel_name == "Sycles Motion") { return ccl::PASS_MOTION; }
	else if (channel_name == "Sycles Motion Weight") { return ccl::PASS_MOTION_WEIGHT; }
	else if (channel_name == "Sycles Mist") { return ccl::PASS_MIST; }
	else if (channel_name == "Sycles Volume Direct") { return ccl::PASS_VOLUME_DIRECT; }
	else if (channel_name == "Sycles Volume Indirect") { return ccl::PASS_VOLUME_INDIRECT; }
	else if (channel_name == "Sycles Sample Count") { return ccl::PASS_SAMPLE_COUNT; }
	else if (channel_name == "Sycles AOV Color") { return ccl::PASS_AOV_COLOR; }
	else if (channel_name == "Sycles AOV Value") { return ccl::PASS_AOV_VALUE; }
    // TODO: recognize lightgroup pass

	// unknown channel, return None
	return ccl::PASS_NONE;
}

XSI::CString pass_to_name(ccl::PassType pass_type)
{
    switch (pass_type) {
    case ccl::PASS_NONE:
        return "None";
    case ccl::PASS_COMBINED:
        return "Combined";
    case ccl::PASS_DEPTH:
        return "Depth";
    case ccl::PASS_MIST:
        return "Mist";
    case ccl::PASS_POSITION:
        return "Position";
    case ccl::PASS_NORMAL:
        return "Normal";
    case ccl::PASS_ROUGHNESS:
        return "Roughness";
    case ccl::PASS_UV:
        return "UV";
    case ccl::PASS_MOTION:
        return "Motion";
    case ccl::PASS_MOTION_WEIGHT:
        return "Motion Wright";
    case ccl::PASS_OBJECT_ID:
        return "Object ID";
    case ccl::PASS_MATERIAL_ID:
        return "Material ID";
    case ccl::PASS_EMISSION:
        return "Emission";
    case ccl::PASS_BACKGROUND:
        return "Background";
    case ccl::PASS_AO:
        return "Ambient Occlusion";
    case ccl::PASS_SHADOW:
        return "Shadow";
    case ccl::PASS_DIFFUSE_COLOR:
        return "Diffuse Color";
    case ccl::PASS_GLOSSY_COLOR:
        return "Glossy COlor";
    case ccl::PASS_TRANSMISSION_COLOR:
        return "Transmission Color";
    case ccl::PASS_DIFFUSE:
        return "Diffuse";
    case ccl::PASS_DIFFUSE_DIRECT:
        return "Diffuse Direct";
    case ccl::PASS_DIFFUSE_INDIRECT:
        return "Diffuse Indirect";
    case ccl::PASS_GLOSSY:
        return "Glossy";
    case ccl::PASS_GLOSSY_DIRECT:
        return "Glossy Direct";
    case ccl::PASS_GLOSSY_INDIRECT:
        return "Glossy Indirect";
    case ccl::PASS_TRANSMISSION:
        return "Transmission";
    case ccl::PASS_TRANSMISSION_DIRECT:
        return "Transmission Direct";
    case ccl::PASS_TRANSMISSION_INDIRECT:
        return "Transmission Indirect";
    case ccl::PASS_VOLUME:
        return "Volume";
    case ccl::PASS_VOLUME_DIRECT:
        return "Volume Direct";
    case ccl::PASS_VOLUME_INDIRECT:
        return "Volume Indirect";
    case ccl::PASS_CRYPTOMATTE:
        return "Cryptomatte";
    case ccl::PASS_DENOISING_NORMAL:
        return "Denoising Normal";
    case ccl::PASS_DENOISING_ALBEDO:
        return "Denoising Albedo";
    case ccl::PASS_DENOISING_DEPTH:
        return "Denoising Depth";
    case ccl::PASS_DENOISING_PREVIOUS:
        return "Denoising Previous";
    case ccl::PASS_SHADOW_CATCHER:
        return "Shadow Catcher";
    case ccl::PASS_SHADOW_CATCHER_SAMPLE_COUNT:
        return "Shadow Catcher Sample Count";
    case ccl::PASS_SHADOW_CATCHER_MATTE:
        return "Shadow Catcher Matte";
    case ccl::PASS_ADAPTIVE_AUX_BUFFER:
        return "Adaptive AUX Buffer";
    case ccl::PASS_SAMPLE_COUNT:
        return "Sample Count";
    case ccl::PASS_AOV_COLOR:
        return "AOV Color";
    case ccl::PASS_AOV_VALUE:
        return "AOV Value";
    case ccl::PASS_BAKE_PRIMITIVE:
        return "Bake Primitive";
    case ccl::PASS_BAKE_DIFFERENTIAL:
        return "Bake Differential";
    case ccl::PASS_CATEGORY_LIGHT_END:
        return "Light End";
    case ccl::PASS_CATEGORY_DATA_END:
        return "Data End";
    case ccl::PASS_CATEGORY_BAKE_END:
        return "Bake End";
    case ccl::PASS_NUM:
        return "Num";
    case ccl::PASS_GUIDING_COLOR:
        return "Guiding Color";
    case ccl::PASS_GUIDING_PROBABILITY:
        return "Guiding Probability";
    case ccl::PASS_GUIDING_AVG_ROUGHNESS:
        return "Guiding AVG Roughness";
    }

    return "Unknown";
}

void sync_passes(ccl::Scene* scene, OutputContext* output_context)
{
    // sync passes
    // all default passes should be added at once
    // but some of them (for example, AOV) can be added several times with different names
    // in the update tile process all of them should be reded by names and stored into separate segement of the pixels buffer
    // always add combined pass
    std::set<ccl::ustring> exported_names;
    ccl::ustring combined_name = ccl::ustring(pass_to_name(ccl::PASS_COMBINED).GetAsciiString());
    exported_names.insert(combined_name);
    pass_add(scene, ccl::PASS_COMBINED, combined_name);

    // next visual
    ccl::ustring visual_name = output_context->get_visual_pass_name();
    if (!exported_names.contains(visual_name))
    {
        // add visual pass
        exported_names.insert(visual_name);
        pass_add(scene, output_context->get_visal_pass_type(), visual_name);
    }

    // next for all render output
    for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
    {
        ccl::ustring output_name = output_context->get_output_pass_name(i);
        if (!exported_names.contains(output_name))
        {
            exported_names.insert(output_name);
            pass_add(scene, output_context->get_output_pass_type(i), output_name);
        }
    }
}