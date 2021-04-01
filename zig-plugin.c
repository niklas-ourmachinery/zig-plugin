static struct tm_logger_api* tm_logger_api;
static struct tm_the_truth_api* tm_the_truth_api;
static struct tm_properties_view_api* tm_properties_view_api;
static struct tm_ui_api* tm_ui_api;
static struct tm_draw2d_api* tm_draw2d_api;

#include "zig-plugin.h"

#include <foundation/api_registry.h>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/macros.h>
#include <foundation/rect.inl>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>

#include <plugins/editor_views/asset_browser.h>
#include <plugins/editor_views/properties.h>
#include <plugins/ui/draw2d.h>
#include <plugins/ui/ui.h>

static const char* minimal_template
    = "static struct tm_logger_api* tm_logger_api;\n"
      "\n"
      "#include <foundation/api_registry.h>\n"
      "\n"
      "#include <foundation/log.h>\n"
      "\n"
      "TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api* reg, bool load)\n"
      "{\n"
      "    tm_logger_api = reg->get(TM_LOGGER_API_NAME);\n"
      "\n"
      "    TM_LOG(\"Minimal plugin %s.\\n\", load ? \"loaded\" : \"unloaded\");\n"
      "}\n"
      "";

static float properties_ui__text(struct tm_properties_ui_args_t* args, tm_rect_t item_rect,
    const char* name, const char* tooltip, tm_tt_id_t object, uint32_t indent, uint32_t property)
{
    tm_ui_buffers_t uib = tm_ui_api->buffers(args->ui);
    item_rect.y = tm_properties_view_api->ui_label(args, item_rect, name, tooltip);
    item_rect.y += 5;
    const char* text = tm_the_truth_api->get_string(args->tt, tm_tt_read(args->tt, object), property);
    const tm_rect_t r = tm_ui_api->wrapped_text(args->ui, args->uistyle, &(tm_ui_text_t){ .rect = item_rect, .text = text });
    tm_draw2d_style_t style[1] = { 0 };
    tm_ui_api->to_draw_style(args->ui, style, args->uistyle);
    style->color = uib.colors[TM_UI_COLOR_TEXT];
    const tm_rect_t stroke_r = tm_rect_pad(r, 5, 5);
    tm_draw2d_api->stroke_rect(uib.vbuffer, uib.ibuffers[0], style, stroke_r);
    return item_rect.y + r.h + 10;
}

static tm_tt_id_t asset_browser__create_asset__c_file(struct tm_asset_browser_create_asset_o* inst, tm_the_truth_o* tt, tm_tt_undo_scope_t undo_scope)
{
    return tm_the_truth_api->quick_create_object(tt, undo_scope, TM_TT_TYPE_HASH__C_FILE,
        TM_TT_PROP__C_FILE__TEXT, minimal_template, -1);
}

static tm_asset_browser_create_asset_i asset_browser__create_asset__c_file_i = {
    .menu_name = TM_LOCALIZE_LATER("New C File"),
    .asset_name = TM_LOCALIZE_LATER("minimal"),
    .create = asset_browser__create_asset__c_file,
};

static void truth__create_types(tm_the_truth_o* tt)
{
    const tm_the_truth_property_definition_t c_file_properties[] = {
        [TM_TT_PROP__C_FILE__TEXT] = { "text", TM_THE_TRUTH_PROPERTY_TYPE_STRING },
    };

    const tm_tt_type_t c_file = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__C_FILE, c_file_properties, TM_ARRAY_COUNT(c_file_properties));

    tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__FILE_EXTENSION, "c");
    tm_the_truth_api->set_property_aspect(tt, c_file, TM_TT_PROP__C_FILE__TEXT, TM_TT_PROP_ASPECT__PROPERTIES__CUSTOM_UI, properties_ui__text);

    // tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__PROPERTIES, &properties_aspect__c_file);
    // tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__ASSET_PREVIEW, preview_aspect__animation);
}

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api* reg, bool load)
{
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_ui_api = reg->get(TM_UI_API_NAME);
    tm_draw2d_api = reg->get(TM_DRAW2D_API_NAME);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, truth__create_types);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_CREATE_ASSET_INTERFACE_NAME, &asset_browser__create_asset__c_file_i);

    TM_LOG("Zig plugin %s.\n", load ? "loaded" : "unloaded");
}
