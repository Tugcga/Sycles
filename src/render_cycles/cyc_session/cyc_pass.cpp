#include <set>

#include "scene/scene.h"
#include "scene/pass.h"

#include <xsi_application.h>

#include "../../utilities/logs.h"
#include "../../utilities/strings.h"
#include "../cyc_output/output_context.h"
#include "../../render_base/render_visual_buffer.h"
#include "cyc_baking.h"
#include "../cyc_output/series_context.h"

ccl::ustring noisy_combined_name()
{
    return ccl::ustring((XSI::CString("Noisy ") + pass_to_name(ccl::PASS_COMBINED)).GetAsciiString());
}

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
    if (channel_name == "Cycles Lightgroup")
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

ccl::Pass* pass_add(ccl::Scene* scene, ccl::PassType type, ccl::ustring name, ccl::PassMode mode)
{
	ccl::Pass* pass = scene->create_node<ccl::Pass>();

	pass->set_type(type);
	pass->set_name(name);
    pass->set_mode(mode);
	
    // lightgroups are always noisy (like in Blender)
    // each lightgroup has the name Combined_...
    if (type == ccl::PASS_COMBINED && is_start_from(name, ccl::ustring("Combined_")))
    {
        pass->set_mode(ccl::PassMode::NOISY);
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
    if (channel_name == "Cycles Combined" || channel_name == "Main") { return ccl::PASS_COMBINED; }
    else if (channel_name == "Cycles Depth") { return ccl::PASS_DEPTH; }
    else if (channel_name == "Cycles Roughness") { return ccl::PASS_ROUGHNESS; }
    else if (channel_name == "Cycles Position") { return ccl::PASS_POSITION; }
    else if (channel_name == "Cycles Normal") { return ccl::PASS_NORMAL; }
    else if (channel_name == "Cycles UV") { return ccl::PASS_UV; }
    else if (channel_name == "Cycles Object ID") { return ccl::PASS_OBJECT_ID; }
    else if (channel_name == "Cycles Material ID") { return ccl::PASS_MATERIAL_ID; }
    else if (channel_name == "Cycles Diffuse") { return ccl::PASS_DIFFUSE; }
    else if (channel_name == "Cycles Diffuse Color") { return ccl::PASS_DIFFUSE_COLOR; }
    else if (channel_name == "Cycles Glossy") { return ccl::PASS_GLOSSY; }
    else if (channel_name == "Cycles Glossy Color") { return ccl::PASS_GLOSSY_COLOR; }
    else if (channel_name == "Cycles Transmission") { return ccl::PASS_TRANSMISSION; }
    else if (channel_name == "Cycles Transmission Color") { return ccl::PASS_TRANSMISSION_COLOR; }
    else if (channel_name == "Cycles Diffuse Indirect") { return ccl::PASS_DIFFUSE_INDIRECT; }
    else if (channel_name == "Cycles Glossy Indirect") { return ccl::PASS_GLOSSY_INDIRECT; }
    else if (channel_name == "Cycles Transmission Indirect") { return ccl::PASS_TRANSMISSION_INDIRECT; }
    else if (channel_name == "Cycles Diffuse Direct") { return ccl::PASS_DIFFUSE_DIRECT; }
    else if (channel_name == "Cycles Glossy Direct") { return ccl::PASS_GLOSSY_DIRECT; }
    else if (channel_name == "Cycles Transmission Direct") { return ccl::PASS_TRANSMISSION_DIRECT; }
    else if (channel_name == "Cycles Emission") { return ccl::PASS_EMISSION; }
    else if (channel_name == "Cycles Background") { return ccl::PASS_BACKGROUND; }
    else if (channel_name == "Cycles AO") { return ccl::PASS_AO; }
    else if (channel_name == "Cycles Shadow Catcher") { return ccl::PASS_SHADOW_CATCHER; }
    else if (channel_name == "Cycles Motion") { return ccl::PASS_MOTION; }
    else if (channel_name == "Cycles Mist") { return ccl::PASS_MIST; }
    else if (channel_name == "Cycles Volume Direct") { return ccl::PASS_VOLUME_DIRECT; }
    else if (channel_name == "Cycles Volume Indirect") { return ccl::PASS_VOLUME_INDIRECT; }
    else if (channel_name == "Cycles Sample Count") { return ccl::PASS_SAMPLE_COUNT; }
    else if (channel_name == "Cycles AOV Color") { return ccl::PASS_AOV_COLOR; }
    else if (channel_name == "Cycles AOV Value") { return ccl::PASS_AOV_VALUE; }
    else if (channel_name == "Cycles Lightgroup") { return ccl::PASS_COMBINED; }

	// unknown channel, return None
	return ccl::PASS_NONE;
}

XSI::CString get_name_for_motion_display_channel()
{
    return "Cycles Motion";
}

XSI::CString get_name_for_motion_output_channel()
{
    return "Cycles_Motion";
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

ccl::PassType convert_baking_pass(ccl::PassType input_pass, BakingContext* baking_context)
{
    bool is_direct = baking_context->get_key_is_direct();
    bool is_indirect = baking_context->get_key_is_indirect();
    if (input_pass == ccl::PASS_DIFFUSE)
    {
        if (is_direct && is_indirect) { return ccl::PASS_DIFFUSE; }
        else if (is_direct) { return ccl::PASS_DIFFUSE_DIRECT; }
        else if (is_indirect) { return ccl::PASS_DIFFUSE_INDIRECT; }
        else { return ccl::PASS_DIFFUSE_COLOR; }
    }
    else if (input_pass == ccl::PASS_GLOSSY)
    {
        if (is_direct && is_indirect) { return ccl::PASS_GLOSSY; }
        else if (is_direct) { return ccl::PASS_GLOSSY_DIRECT; }
        else if (is_indirect) { return ccl::PASS_GLOSSY_INDIRECT; } else { return ccl::PASS_GLOSSY_COLOR; }
    }
    else if (input_pass == ccl::PASS_TRANSMISSION)
    {
        if (is_direct && is_indirect) { return ccl::PASS_TRANSMISSION; }
        else if (is_direct) { return ccl::PASS_TRANSMISSION_DIRECT; }
        else if (is_indirect) { return ccl::PASS_TRANSMISSION_INDIRECT; }
        else { return ccl::PASS_TRANSMISSION_COLOR; }
    }
    else
    {
        return input_pass;
    }
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

void check_visual_aov_lightgroup_name(RenderVisualBuffer* visual_buffer, const XSI::CStringArray& aov_color_names, const XSI::CStringArray& aov_value_names, const XSI::CStringArray& lightgroup_names)
{
    ccl::PassType visual_pass = visual_buffer->get_pass_type();
    if (visual_pass == ccl::PASS_COMBINED && visual_buffer->get_pass_name() != "Combined")
    {
        // visual is Lightgroup
        XSI::CString pass_name = remove_prefix_from_lightgroup_name(visual_buffer->get_pass_name().c_str());
        if (!is_lightgroup_is_correct(pass_name, lightgroup_names))
        {
            bool is_switch = lightgroup_names.GetCount() > 0;
            log_warning("Display channel is set to Light Group with " +
                XSI::CString(pass_name.Length() == 0 ? "empty name." : "name " + pass_name + ". There is no pass with this name.") +
                XSI::CString(is_switch ? " Switch to " + lightgroup_names[0] + "." : ""));

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
            log_warning("Display channel is set to AOV " + XSI::CString(is_color ? "Color" : "Value") +
                " with " +
                XSI::CString(pass_name.Length() == 0 ? "empty name." : "name " + pass_name + ". There is no pass with this name.") +
                XSI::CString(is_switch ? " Switch to " + switch_name  + "." : ""));

            if (is_switch)
            {
                visual_buffer->set_pass_name(add_prefix_to_aov_name(switch_name, is_color));
            }
        }
    }
}

void sync_passes(ccl::Scene* scene, UpdateContext* update_context, OutputContext* output_context, SeriesContext* series_context, BakingContext* baking_context, RenderVisualBuffer *visual_buffer)
{
    MotionSettingsType motion_type = update_context->get_motion_type();
    XSI::CStringArray lightgroups = update_context->get_lightgropus();
    XSI::CStringArray aov_color_names = update_context->get_color_aovs();
    XSI::CStringArray aov_value_names = update_context->get_value_aovs();

    // if visual pass is aov, then check that the name of the pass is correct
    check_visual_aov_lightgroup_name(visual_buffer, aov_color_names, aov_value_names, lightgroups);

    XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
    XSI::CTime eval_time = update_context->get_time();

    bool store_denoising = false;
    bool use_denoising = update_context->get_use_denoising();
    bool use_denoising_albedo = update_context->get_use_denoising_albedo();
    bool use_denoising_normal = update_context->get_use_denoising_normal();

    if (update_context->get_render_type() == RenderType::RenderType_Pass && (bool)render_parameters.GetValue("output_exr_combine_passes", eval_time))
    {
        store_denoising = render_parameters.GetValue("output_exr_denoising_data", eval_time);
    }
    
    if (store_denoising)
    {
        if (!use_denoising)
        {
            log_warning("You choose to include denoising passes in a single EXR, but denoising mode is disabled. Skip these passes.");
        }
        store_denoising = store_denoising & use_denoising;
    }

    // here we call the main method in output context
    // and setup all output passes, buffers, pixels and so on
    output_context->set_output_passes(baking_context, motion_type, store_denoising, use_denoising_albedo, use_denoising_normal, aov_color_names, aov_value_names, lightgroups);

    // sync passes
    bool use_shadow_catcher = false;
    // all default passes should be added at once
    // but some of them (for example, AOV) can be added several times with different names
    // in the update tile process all of them should be reded by names and stored into separate segement of the pixels buffer
    // always add combined pass
    std::set<ccl::ustring> exported_names;
    ccl::ustring combined_name = ccl::ustring(pass_to_name(ccl::PASS_COMBINED).GetAsciiString());
    exported_names.insert(combined_name);
    pass_add(scene, ccl::PASS_COMBINED, combined_name, ccl::PassMode::DENOISED);

    // next visual
    ccl::ustring visual_name = visual_buffer->get_pass_name();
    if (!exported_names.contains(visual_name))
    {
        // add visual pass
        exported_names.insert(visual_name);
        ccl::PassType visual_pass_type = visual_buffer->get_pass_type();
        pass_add(scene, visual_pass_type, visual_name, ccl::PassMode::DENOISED);

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

            ccl::Pass* pass = pass_add(scene, new_pass_type, output_name, ccl::PassMode::DENOISED);
            if (baking_context->get_is_valid())  // use valid baking context as identifier of the rendermap render process
            {
                pass->set_include_albedo(baking_context->get_key_is_color());
                baking_context->set_pass_type(new_pass_type);  // will use it later in sync integrator
            }
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

    // add passes for series rendering
    // only to the Cycles
    // combined already added with default name
    if (series_context->need_albedo())
    {
        ccl::PassType albedo_type = ccl::PASS_DIFFUSE_COLOR;
        ccl::ustring albedo_name = ccl::ustring(pass_to_name(albedo_type).GetAsciiString());
        if (!exported_names.contains(albedo_name))
        {
            exported_names.insert(albedo_name);
            pass_add(scene, albedo_type, albedo_name, ccl::PassMode::DENOISED);
        }
    }
    if (series_context->need_normal())
    {
        ccl::PassType normal_type = ccl::PASS_NORMAL;
        ccl::ustring normal_name = ccl::ustring(pass_to_name(normal_type).GetAsciiString());
        if (!exported_names.contains(normal_name))
        {
            exported_names.insert(normal_name);
            pass_add(scene, normal_type, normal_name, ccl::PassMode::DENOISED);
        }
    }

    scene->film->set_use_approximate_shadow_catcher(!use_shadow_catcher);
}