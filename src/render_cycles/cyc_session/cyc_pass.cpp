#include <set>

#include "scene/scene.h"
#include "scene/pass.h"

#include <xsi_application.h>

#include "../../utilities/logs.h"
#include "../cyc_output/output_context.h"
#include "../../render_base/render_visual_buffer.h"

XSI::CString add_prefix_to_aov_name(const XSI::CString &name, bool is_color)
{
    if (is_color)
    {
        return "aovcolor_" + name;
    }
    else
    {
        return "aovvalue_" + name;
    }
}

XSI::CString add_prefix_to_lightgroup_name(const XSI::CString& name)
{
    return "Combined_" + name;
}

XSI::CString remove_prefix_from_aov_name(const XSI::CString &name)
{
    if (name.Length() >= 9)
    {
        return name.GetSubString(9);
    }
    else
    {
        return name;
    }
}

XSI::CString remove_prefix_from_lightgroup_name(const XSI::CString &name)
{
    if (name.Length() >= 9)
    {
        return name.GetSubString(9);
    }
    else
    {
        return name;
    }
}

// this function called for define name for visual buffer only
// in pre render callback
// channel_name is a name of preview channel (m_display_channel_name)
// so, this name without _
XSI::CString channel_name_to_pass_name(const XSI::CParameterRefArray& render_parameters, const XSI::CString& channel_name, const XSI::CTime& eval_time)
{
    ccl::PassType local_pass_type = channel_to_pass_type(channel_name);
    if (local_pass_type == ccl::PASS_NONE)
    {
        // all unkonwn channels interpretate as combined
        local_pass_type = ccl::PASS_COMBINED;
    }
    XSI::CString local_pass_name = pass_to_name(local_pass_type);
    if (channel_name == "Sycles Lightgroup")
    {
        // we define local_pass_type as Combined, but we should change actual name to obtain not general combined pass, but lightgroup
        local_pass_name = add_prefix_to_lightgroup_name(render_parameters.GetValue("output_pass_preview_name", eval_time));
    }
    else if (local_pass_type == ccl::PASS_AOV_COLOR || local_pass_type == ccl::PASS_AOV_VALUE)
    {// change the name of the pass into name from render parameters
        // at present time we does not know is this name correct or not
        // so, does not change it to correct name, but later show warning message, if the name is incorrect
        local_pass_name = add_prefix_to_aov_name(render_parameters.GetValue("output_pass_preview_name", eval_time), local_pass_type == ccl::PASS_AOV_COLOR);
    }

    return local_pass_name;
}

ccl::Pass* pass_add(ccl::Scene* scene, ccl::PassType type, ccl::ustring name, ccl::PassMode mode = ccl::PassMode::DENOISED)
{
	ccl::Pass* pass = scene->create_node<ccl::Pass>();

	pass->set_type(type);
	pass->set_name(name);
	pass->set_mode(mode);

    if (type == ccl::PASS_COMBINED && name.size() >= 9)
    {
        pass->set_lightgroup(ccl::ustring(remove_prefix_from_lightgroup_name(XSI::CString(name.c_str())).GetAsciiString()));
    }

	return pass;
}

int get_pass_components(ccl::PassType pass_type, bool is_lightgroup)
{
	ccl::PassInfo pass_info = ccl::Pass::get_info(pass_type, false, is_lightgroup);
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
    else if (channel_name == "Sycles Mist") { return ccl::PASS_MIST; }
    else if (channel_name == "Sycles Volume Direct") { return ccl::PASS_VOLUME_DIRECT; }
    else if (channel_name == "Sycles Volume Indirect") { return ccl::PASS_VOLUME_INDIRECT; }
    else if (channel_name == "Sycles Sample Count") { return ccl::PASS_SAMPLE_COUNT; }
    else if (channel_name == "Sycles AOV Color") { return ccl::PASS_AOV_COLOR; }
    else if (channel_name == "Sycles AOV Value") { return ccl::PASS_AOV_VALUE; }
    else if (channel_name == "Sycles Lightgroup") { return ccl::PASS_COMBINED; }

	// unknown channel, return None
	return ccl::PASS_NONE;
}

XSI::CString get_name_for_motion_display_channel()
{
    return "Sycles Motion";
}

XSI::CString get_name_for_motion_output_channel()
{
    return "Sycles_Motion";
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

bool is_aov_name_correct(const XSI::CString& pass_name, const XSI::CStringArray& aovs)
{
    for (size_t i = 0; i < aovs.GetCount(); i++)
    {
        XSI::CString name_with_prefix = aovs[i];
        if (name_with_prefix == pass_name)
        {
            return true;
        }
    }

    return false;
}

bool is_lightgroup_is_correct(const XSI::CString& pass_name, const XSI::CStringArray& lightgroups)
{
    for (size_t i = 0; i < lightgroups.GetCount(); i++)
    {
        XSI::CString lg_name = lightgroups[i];
        if (lg_name == pass_name)
        {
            return true;
        }
    }

    return false;
}

void check_visual_aov_name(RenderVisualBuffer* visual_buffer, const XSI::CStringArray& aov_color_names, const XSI::CStringArray& aov_value_names, const XSI::CStringArray& lightgroup_names)
{
    ccl::PassType visual_pass = visual_buffer->get_pass_type();
    if (visual_pass == ccl::PASS_COMBINED && visual_buffer->get_pass_name() != "Combined")
    {
        // visual is Lightgroup
        XSI::CString pass_name = remove_prefix_from_lightgroup_name(visual_buffer->get_pass_name().c_str());
        if (!is_lightgroup_is_correct(pass_name, lightgroup_names))
        {
            bool is_switch = lightgroup_names.GetCount() > 0;
            log_message("Display channel is set to Light Group with " +
                XSI::CString(pass_name.Length() == 0 ? "empty name." : "name " + pass_name + ". There is no pass with this name.") +
                XSI::CString(is_switch ? " Switch to " + lightgroup_names[0] + "." : ""), XSI::siWarningMsg);

            if (is_switch)
            {
                visual_buffer->set_pass_name(add_prefix_to_lightgroup_name(lightgroup_names[0]));
            }
        }
    }
    else if (visual_pass == ccl::PASS_AOV_COLOR || visual_pass == ccl::PASS_AOV_VALUE)
    {
        bool is_color = visual_pass == ccl::PASS_AOV_COLOR;
        XSI::CString pass_name = remove_prefix_from_aov_name(visual_buffer->get_pass_name().c_str());  // this is original name
        if (!is_aov_name_correct(pass_name, is_color ? aov_color_names : aov_value_names))
        {// name is incorrect
            bool is_switch = is_color ? aov_color_names.GetCount() > 0 : aov_value_names.GetCount() > 0;
            XSI::CString switch_name = "";
            if (is_switch)
            {
                switch_name = is_color ? aov_color_names[0] : aov_value_names[0];
            }

            // display warning message
            log_message("Display channel is set to AOV " + XSI::CString(is_color ? "Color" : "Value") +
                " with " +
                XSI::CString(pass_name.Length() == 0 ? "empty name." : "name " + pass_name + ". There is no pass with this name.") +
                XSI::CString(is_switch ? " Switch to " + switch_name  + "." : ""), XSI::siWarningMsg);

            if (is_switch)
            {
                visual_buffer->set_pass_name(add_prefix_to_aov_name(switch_name, is_color));
            }
        }
    }
}

void sync_passes(ccl::Scene* scene, OutputContext* output_context, RenderVisualBuffer *visual_buffer, MotionType motion_type, const XSI::CStringArray &lightgroups)
{
    // for test only we add to the scene three aovs:
    // 1. color sphere_color_aov
    // 2. value sphere_value_aov
    // 3. value plane_value_aov

    // TODO: make shure that all names are unique, does not use two equal names (even it existst in different nodes)
    XSI::CStringArray aov_color_names;
    aov_color_names.Add("sphere_color_aov");
    XSI::CStringArray aov_value_names;
    aov_value_names.Add("sphere_value_aov");
    aov_value_names.Add("plane_value_aov");

    // if visual pass is aov, then check that the name of the pass is correct
    check_visual_aov_name(visual_buffer, aov_color_names, aov_value_names, lightgroups);

    // here we call the main method in output contex
    // and setup all output passes, buffers, pixels and so on
    output_context->set_output_passes(motion_type, aov_color_names, aov_value_names, lightgroups);

    // sync passes
    bool use_shadow_catcher = false;
    // all default passes should be added at once
    // but some of them (for example, AOV) can be added several times with different names
    // in the update tile process all of them should be reded by names and stored into separate segement of the pixels buffer
    // always add combined pass
    std::set<ccl::ustring> exported_names;
    ccl::ustring combined_name = ccl::ustring(pass_to_name(ccl::PASS_COMBINED).GetAsciiString());
    exported_names.insert(combined_name);
    pass_add(scene, ccl::PASS_COMBINED, combined_name);

    // next visual
    ccl::ustring visual_name = visual_buffer->get_pass_name();
    if (!exported_names.contains(visual_name))
    {
        // add visual pass
        exported_names.insert(visual_name);
        ccl::PassType visual_pass_type = visual_buffer->get_pass_type();
        pass_add(scene, visual_pass_type, visual_name);
        if (visual_pass_type == ccl::PASS_SHADOW_CATCHER)
        {
            use_shadow_catcher = true;
        }
    }

    // next for all render output
    for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
    {
        ccl::ustring output_name = output_context->get_output_pass_name(i);
        if (!exported_names.contains(output_name))
        {
            exported_names.insert(output_name);
            ccl::PassType new_pass_type = output_context->get_output_pass_type(i);
            pass_add(scene, new_pass_type, output_name);
            if (new_pass_type == ccl::PASS_SHADOW_CATCHER)
            {
                use_shadow_catcher = true;
            }
        }
    }

    // next for cryptomatte
    if (output_context->get_is_cryptomatte())
    {
        scene->film->set_cryptomatte_depth(output_context->get_ctypto_depth());
        scene->film->set_cryptomatte_passes(output_context->get_crypto_passes());

        // keys and values for metadata was clear when we setup output passes
        if (scene->film->get_cryptomatte_passes() & ccl::CRYPT_OBJECT)
        {
            output_context->add_cryptomatte_metadata("CryptoObject", scene->object_manager->get_cryptomatte_objects(scene));
        }
        if (scene->film->get_cryptomatte_passes() & ccl::CRYPT_MATERIAL)
        {
            output_context->add_cryptomatte_metadata("CryptoMaterial", scene->shader_manager->get_cryptomatte_materials(scene));
        }
        if (scene->film->get_cryptomatte_passes() & ccl::CRYPT_ASSET)
        {
            output_context->add_cryptomatte_metadata("CryptoAsset", scene->object_manager->get_cryptomatte_assets(scene));
        }
    }

    scene->film->set_use_approximate_shadow_catcher(!use_shadow_catcher);
}