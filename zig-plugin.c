static struct tm_logger_api* tm_logger_api;
static struct tm_the_truth_api* tm_the_truth_api;
static struct tm_properties_view_api* tm_properties_view_api;
static struct tm_ui_api* tm_ui_api;
static struct tm_draw2d_api* tm_draw2d_api;
static struct tm_localizer_api* tm_localizer_api;
static struct tm_asset_browser_add_asset_api* add_asset_api;
static struct tm_the_truth_assets_api* tm_the_truth_assets_api;
static struct tm_os_api* tm_os_api;
static struct tm_random_api* tm_random_api;
static struct tm_temp_allocator_api* tm_temp_allocator_api;

#include "zig-plugin.h"

#include <foundation/api_registry.h>
#include <foundation/carray.inl>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/macros.h>
#include <foundation/os.h>
#include <foundation/plugin_assets.h>
#include <foundation/random.h>
#include <foundation/rect.inl>
#include <foundation/temp_allocator.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>
#include <foundation/undo.h>

#include <plugins/editor_views/asset_browser.h>
#include <plugins/editor_views/properties.h>
#include <plugins/ui/draw2d.h>
#include <plugins/ui/ui.h>

#include <inttypes.h>
#include <string.h>

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
      "    TM_LOG(\"zig cc test plugin %s.\\n\", load ? \"loaded\" : \"unloaded\");\n"
      "}\n"
      "";

static void private__compile(tm_the_truth_o* tt, tm_tt_id_t c_file, tm_tt_undo_scope_t undo_scope, tm_ui_o* ui)
{
    TM_INIT_TEMP_ALLOCATOR(ta);

    const char* error = 0;
    const char* out_file_path = 0;

    // Compile the DLL
    do {
        const char* text = tm_the_truth_api->get_string(tt, tm_tt_read(tt, c_file), TM_TT_PROP__C_FILE__TEXT);

        const char* tmp = tm_os_api->file_system->temp_directory(ta);
        const uint64_t rnd = tm_random_api->next();
        const char* dir = tm_temp_allocator_api->printf(ta, "%szig-plugin-%" PRIX64, tmp, rnd);
        const bool made_dir = tm_os_api->file_system->make_directory(dir);
        if (!made_dir) {
            error = tm_temp_allocator_api->printf(ta, "Could not create temp directory %s", dir);
            break;
        }
        const char* c_file_path = tm_temp_allocator_api->printf(ta, "%s\\temp.c", dir);
        const tm_file_o f = tm_os_api->file_io->open_output(c_file_path);
        tm_os_api->file_io->write(f, text, strlen(text));
        tm_os_api->file_io->close(f);

        const char* HEADERS = "C:/Work/themachinery";
        const char* FLAGS_T = "-I %s -Wno-microsoft-anon-tag -fms-extensions -Werror";
        const char* FLAGS = tm_temp_allocator_api->printf(ta, FLAGS_T, HEADERS);
        out_file_path = tm_temp_allocator_api->printf(ta, "%s\\temp.dll", dir);
        const char* zig = tm_temp_allocator_api->printf(ta, "zig cc %s -shared -o %s %s", c_file_path, out_file_path, FLAGS);
        int exit_code;
        const char* result = tm_os_api->system->execute_stdout(zig, 0, ta, &exit_code);
        if (exit_code)
            error = tm_or(result, "Unknown error");
    } while (0);

    if (!error) {
        tm_tt_id_t dll = tm_the_truth_api->get_reference(tt, tm_tt_read(tt, c_file), TM_TT_PROP__C_FILE__DLL);
        if (!dll.u64) {
            const tm_tt_id_t c_file_asset = tm_the_truth_api->owner(tt, c_file);
            const char* c_file_name = tm_the_truth_api->get_string(tt, tm_tt_read(tt, c_file_asset), TM_TT_PROP__ASSET__NAME);

            dll = tm_the_truth_api->create_object_of_hash(tt, TM_TT_TYPE_HASH__PLUGIN, undo_scope);
            tm_the_truth_object_o* c_file_w = tm_the_truth_api->write(tt, c_file);
            tm_the_truth_api->set_reference(tt, c_file_w, TM_TT_PROP__C_FILE__DLL, dll);
            tm_the_truth_api->commit(tt, c_file_w, undo_scope);
            struct tm_asset_browser_add_asset_api* add = add_asset_api;
            add->add(add->inst, add->current_directory(add->inst, ui), dll, c_file_name, undo_scope, false, ui, 0, 0);
        }

        const tm_file_stat_t stat = tm_os_api->file_system->stat(out_file_path);
        if (stat.exists && stat.size > 0) {
            char* buffer = 0;
            tm_carray_temp_resize(buffer, stat.size, ta);
            tm_file_o f = tm_os_api->file_io->open_input(out_file_path);
            const int64_t read = tm_os_api->file_io->read(f, buffer, stat.size);
            tm_os_api->file_io->close(f);
            if (read != (int64_t)stat.size)
                error = "Couldn't read DLL output file.";
            if (!error) {
                tm_the_truth_object_o* dll_w = tm_the_truth_api->write(tt, dll);
                tm_the_truth_api->set_buffer_content(tt, dll_w, TM_TT_PROP__PLUGIN__WINDOWS_DLL, buffer, stat.size);
                tm_the_truth_api->commit(tt, dll_w, undo_scope);
            }
        } else
            error = "Couldn't read DLL output file.";
    }

    {
        tm_the_truth_object_o* c_file_w = tm_the_truth_api->write(tt, c_file);
        tm_the_truth_api->set_string(tt, c_file_w, TM_TT_PROP__C_FILE__ERROR, tm_or(error, ""));
        tm_the_truth_api->commit(tt, c_file_w, undo_scope);
    }

    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

static float properties__c_file__ui(struct tm_properties_ui_args_t* args, tm_rect_t item_rect, tm_tt_id_t object,
    uint32_t indent)
{
    tm_ui_o* ui = args->ui;
    tm_ui_style_t* uistyle = args->uistyle;
    tm_the_truth_o* tt = args->tt;

    if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = item_rect, .text = TM_LOCALIZE("Compile") })) {
        tm_tt_undo_scope_t undo_scope = tm_the_truth_api->create_undo_scope(tt, TM_LOCALIZE("Compile"));
        private__compile(tt, object, undo_scope, ui);
        args->undo_stack->add(args->undo_stack->inst, tt, undo_scope);
    }
    item_rect.y += item_rect.h + 5;

    item_rect.y = tm_properties_view_api->ui_property(args, item_rect, object, indent, TM_TT_PROP__C_FILE__DLL);

    const char* error = tm_the_truth_api->get_string(tt, tm_tt_read(tt, object), TM_TT_PROP__C_FILE__ERROR);
    if (error && *error) {
        tm_ui_buffers_t uib = tm_ui_api->buffers(ui);
        item_rect.y += 5;
        const tm_rect_t r = tm_ui_api->wrapped_text(ui, uistyle, &(tm_ui_text_t){ .rect = item_rect, .text = error, .color = uib.colors + TM_UI_COLOR_ERROR_TEXT });
        tm_draw2d_style_t style[1] = { 0 };
        tm_ui_api->to_draw_style(ui, style, uistyle);
        style->color = uib.colors[TM_UI_COLOR_ERROR_TEXT];
        const tm_rect_t stroke_r = tm_rect_pad(r, 5, 5);
        tm_draw2d_api->stroke_rect(uib.vbuffer, uib.ibuffers[0], style, stroke_r);
        item_rect.y = item_rect.y + r.h + 10;
    }

    // Show source code?
    if (true) {
        tm_ui_buffers_t uib = tm_ui_api->buffers(ui);
        item_rect.y += 5;
        const char* text = tm_the_truth_api->get_string(tt, tm_tt_read(tt, object), TM_TT_PROP__C_FILE__TEXT);
        const tm_rect_t r = tm_ui_api->wrapped_text(ui, uistyle, &(tm_ui_text_t){ .rect = item_rect, .text = text });
        tm_draw2d_style_t style[1] = { 0 };
        tm_ui_api->to_draw_style(ui, style, uistyle);
        style->color = uib.colors[TM_UI_COLOR_TEXT];
        const tm_rect_t stroke_r = tm_rect_pad(r, 5, 5);
        tm_draw2d_api->stroke_rect(uib.vbuffer, uib.ibuffers[0], style, stroke_r);
        item_rect.y = item_rect.y + r.h + 10;
    }

    return item_rect.y;
}

static struct tm_properties_aspect_i properties__c_file_i = {
    .custom_ui = properties__c_file__ui,
};

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
        [TM_TT_PROP__C_FILE__DLL] = { "dll", TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE, .type_hash = TM_TT_TYPE_HASH__PLUGIN },
        [TM_TT_PROP__C_FILE__ERROR] = { "error", TM_THE_TRUTH_PROPERTY_TYPE_STRING },
    };

    const tm_tt_type_t c_file = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__C_FILE, c_file_properties, TM_ARRAY_COUNT(c_file_properties));

    tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__FILE_EXTENSION, "c");
    tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__PROPERTIES, &properties__c_file_i);
}

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api* reg, bool load)
{
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_ui_api = reg->get(TM_UI_API_NAME);
    tm_draw2d_api = reg->get(TM_DRAW2D_API_NAME);
    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    add_asset_api = reg->get(TM_ASSET_BROWSER_ADD_ASSET_API_NAME);
    tm_the_truth_assets_api = reg->get(TM_THE_TRUTH_ASSETS_API_NAME);
    tm_os_api = reg->get(TM_OS_API_NAME);
    tm_random_api = reg->get(TM_RANDOM_API_NAME);
    tm_temp_allocator_api = reg->get(TM_TEMP_ALLOCATOR_API_NAME);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, truth__create_types);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_CREATE_ASSET_INTERFACE_NAME, &asset_browser__create_asset__c_file_i);
}