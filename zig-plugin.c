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
static struct tm_allocator_api* tm_allocator_api;
static struct tm_the_machinery_api* tm_the_machinery_api;

#include "zig-plugin.h"

#include <foundation/api_registry.h>
#include <foundation/carray.inl>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/macros.h>
#include <foundation/os.h>
#include <foundation/plugin_assets.h>
#include <foundation/plugin_callbacks.h>
#include <foundation/random.h>
#include <foundation/rect.inl>
#include <foundation/temp_allocator.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>
#include <foundation/undo.h>

#include <plugins/editor_views/asset_browser.h>
#include <plugins/editor_views/properties.h>
#include <plugins/the_machinery_shared/asset_aspects.h>
#include <plugins/ui/draw2d.h>
#include <plugins/ui/ui.h>

#include <the_machinery/the_machinery.h>

#include <inttypes.h>
#include <string.h>

struct monitored_file_t {
    tm_the_truth_o* tt;
    tm_tt_id_t c_file;
    tm_file_stat_t stat;
    char* path;
};

tm_allocator_i* allocator;
struct monitored_file_t* monitored_files;

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

static const char* custom_tab_template
    = "static struct tm_api_registry_api* tm_global_api_registry;\n"
      "\n"
      "static struct tm_draw2d_api* tm_draw2d_api;\n"
      "static struct tm_ui_api* tm_ui_api;\n"
      "\n"
      "#include <foundation/allocator.h>\n"
      "#include <foundation/api_registry.h>\n"
      "\n"
      "#include <plugins/ui/docking.h>\n"
      "#include <plugins/ui/draw2d.h>\n"
      "#include <plugins/ui/ui.h>\n"
      "#include <plugins/ui/ui_custom.h>\n"
      "\n"
      "#include <the_machinery/the_machinery_tab.h>\n"
      "\n"
      "#include <stdio.h>\n"
      "\n"
      "TM_DLL_EXPORT void load_custom_tab(struct tm_api_registry_api* reg, bool load);\n"
      "\n"
      "#define TM_CUSTOM_TAB_VT_NAME \"tm_custom_tab\"\n"
      "#define TM_CUSTOM_TAB_VT_NAME_HASH TM_STATIC_HASH(\"tm_custom_tab\", 0xbc4e3e47fbf1cdc1ULL)\n"
      "\n"
      "struct tm_tab_o {\n"
      "    tm_tab_i tm_tab_i;\n"
      "    tm_allocator_i* allocator;\n"
      "};\n"
      "\n"
      "static void tab__ui(tm_tab_o* tab, tm_ui_o* ui, const tm_ui_style_t* uistyle, tm_rect_t rect)\n"
      "{\n"
      "    tm_ui_buffers_t uib = tm_ui_api->buffers(ui);\n"
      "    tm_draw2d_style_t* style = &(tm_draw2d_style_t){ 0 };\n"
      "    tm_ui_api->to_draw_style(ui, style, uistyle);\n"
      "\n"
      "    style->color = (tm_color_srgb_t){ .a = 255, .r = 255 };\n"
      "    tm_draw2d_api->fill_rect(uib.vbuffer, *uib.ibuffers, style, rect);\n"
      "}\n"
      "\n"
      "static const char* tab__create_menu_name(void)\n"
      "{\n"
      "    return \"Custom Tab\";\n"
      "}\n"
      "\n"
      "static const char* tab__title(tm_tab_o* tab, struct tm_ui_o* ui)\n"
      "{\n"
      "    return \"Custom Tab\";\n"
      "}\n"
      "\n"
      "static bool tab__need_update(tm_tab_o* tab)\n"
      "{\n"
      "    return true;\n"
      "}\n"
      "\n"
      "static tm_tab_i* tab__create(tm_tab_create_context_t* context, tm_ui_o* ui)\n"
      "{\n"
      "    tm_allocator_i* allocator = context->allocator;\n"
      "    uint64_t* id = context->id;\n"
      "\n"
      "    static tm_the_machinery_tab_vt* vt = 0;\n"
      "    if (!vt)\n"
      "        vt = tm_global_api_registry->get(TM_CUSTOM_TAB_VT_NAME);\n"
      "\n"
      "    tm_tab_o* tab = tm_alloc(allocator, sizeof(tm_tab_o));\n"
      "    *tab = (tm_tab_o){\n"
      "        .tm_tab_i = {\n"
      "            .vt = (tm_tab_vt*)vt,\n"
      "            .inst = (tm_tab_o*)tab,\n"
      "            .root_id = *id,\n"
      "        },\n"
      "        .allocator = allocator,\n"
      "    };\n"
      "\n"
      "    *id += 1000000;\n"
      "    return &tab->tm_tab_i;\n"
      "}\n"
      "\n"
      "static void tab__destroy(tm_tab_o* tab)\n"
      "{\n"
      "    tm_free(tab->allocator, tab, sizeof(*tab));\n"
      "}\n"
      "\n"
      "static tm_the_machinery_tab_vt* custom_tab_vt = &(tm_the_machinery_tab_vt){\n"
      "    .name = TM_CUSTOM_TAB_VT_NAME,\n"
      "    .name_hash = TM_CUSTOM_TAB_VT_NAME_HASH,\n"
      "    .create_menu_name = tab__create_menu_name,\n"
      "    .create = tab__create,\n"
      "    .destroy = tab__destroy,\n"
      "    .title = tab__title,\n"
      "    .ui = tab__ui,\n"
      "    .need_update = tab__need_update,\n"
      "};\n"
      "\n"
      "TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api* reg, bool load)\n"
      "{\n"
      "    tm_global_api_registry = reg;\n"
      "\n"
      "    tm_draw2d_api = reg->get(TM_DRAW2D_API_NAME);\n"
      "    tm_ui_api = reg->get(TM_UI_API_NAME);\n"
      "\n"
      "    tm_set_or_remove_api(reg, load, TM_CUSTOM_TAB_VT_NAME, custom_tab_vt);\n"
      "    tm_add_or_remove_implementation(reg, load, TM_TAB_VT_INTERFACE_NAME, custom_tab_vt);\n"
      "}\n"
      "";

static const char* temp_dir(tm_temp_allocator_i* ta)
{
    const char* tmp = tm_os_api->file_system->temp_directory(ta);
    const uint64_t rnd = tm_random_api->next();
    const char* dir = tm_temp_allocator_api->printf(ta, "%szig-plugin-%" PRIX64, tmp, rnd);
    const bool made_dir = tm_os_api->file_system->make_directory(dir);
    return made_dir ? dir : 0;
}

static const char* write_c_file_to_temp_path(tm_the_truth_o* tt, tm_tt_id_t c_file, tm_temp_allocator_i* ta)
{
    const char* text = tm_the_truth_api->get_string(tt, tm_tt_read(tt, c_file), TM_TT_PROP__C_FILE__TEXT);
    const char* dir = temp_dir(ta);
    if (!dir)
        return 0;
    const char* c_file_path = tm_temp_allocator_api->printf(ta, "%s\\temp.c", dir);
    const tm_file_o f = tm_os_api->file_io->open_output(c_file_path);
    const bool success = tm_os_api->file_io->write(f, text, strlen(text));
    tm_os_api->file_io->close(f);
    return success ? c_file_path : 0;
}

static void read_c_file_from_path(tm_the_truth_o* tt, tm_tt_id_t c_file, const char* path, tm_tt_undo_scope_t undo_scope)
{
    TM_INIT_TEMP_ALLOCATOR(ta);

    const tm_file_stat_t stat = tm_os_api->file_system->stat(path);
    if (stat.exists && stat.size > 0) {
        char* buffer = 0;
        tm_carray_temp_resize(buffer, stat.size + 1, ta);
        tm_file_o f = tm_os_api->file_io->open_input(path);
        const int64_t read = tm_os_api->file_io->read(f, buffer, stat.size);
        tm_os_api->file_io->close(f);
        buffer[stat.size] = 0;
        if (read == (int64_t)stat.size) {
            tm_the_truth_object_o* c_file_w = tm_the_truth_api->write(tt, c_file);
            tm_the_truth_api->set_string(tt, c_file_w, TM_TT_PROP__C_FILE__TEXT, buffer);
            tm_the_truth_api->commit(tt, c_file_w, undo_scope);
        }
    }

    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

static void private__compile(tm_the_truth_o* tt, tm_tt_id_t c_file, tm_tt_undo_scope_t undo_scope, tm_ui_o* ui)
{
    TM_INIT_TEMP_ALLOCATOR(ta);

    const char* error = 0;
    const char* out_file_path = 0;

    // Compile the DLL
    do {
        const char* c_file_path = write_c_file_to_temp_path(tt, c_file, ta);
        if (!c_file_path) {
            error = "Couldn't write temp file";
            break;
        }

        out_file_path = tm_temp_allocator_api->printf(ta, "%s.dll", c_file_path);
        const char* HEADERS = "C:/Work/themachinery";
        const char* FLAGS_T = "-I %s -Wno-microsoft-anon-tag -fms-extensions -Werror";
        const char* FLAGS = tm_temp_allocator_api->printf(ta, FLAGS_T, HEADERS);

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
        TM_TT_PROP__C_FILE__TEXT, custom_tab_template, -1);
}

static tm_asset_browser_create_asset_i asset_browser__create_asset__c_file_i = {
    .menu_name = TM_LOCALIZE_LATER("New C File (Custom Tab)"),
    .asset_name = TM_LOCALIZE_LATER("custom_tab"),
    .create = asset_browser__create_asset__c_file,
};

static void asset__open__c_file(struct tm_application_o* app, struct tm_ui_o* ui, struct tm_tab_i* from_tab, tm_the_truth_o* tt, tm_tt_id_t asset, enum tm_asset_open_mode open_mode)
{
    TM_INIT_TEMP_ALLOCATOR(ta);

    tm_tt_id_t c_file = asset;
    if (TM_STRHASH_U64(tm_the_truth_api->type_name_hash(tt, tm_tt_type(c_file))) == TM_STRHASH_U64(TM_TT_TYPE_HASH__ASSET))
        c_file = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, c_file), TM_TT_PROP__ASSET__OBJECT);

    for (struct monitored_file_t* m = monitored_files; m != tm_carray_end(monitored_files); ++m) {
        if (m->tt == tt && m->c_file.u64 == c_file.u64) {
            tm_os_api->system->open_file(m->path);
            return;
        }
    }

    const char* path = write_c_file_to_temp_path(tt, c_file, ta);
    if (path)
        tm_os_api->system->open_file(path);

    struct monitored_file_t m = {
        .tt = tt,
        .c_file = c_file,
        .stat = tm_os_api->file_system->stat(path),
    };
    tm_carray_resize(m.path, strlen(path) + 1, allocator);
    memcpy(m.path, path, tm_carray_bytes(m.path));
    tm_carray_push(monitored_files, m, allocator);

    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

static struct tm_asset_open_aspect_i asset__open__c_file_i = {
    .open = asset__open__c_file,
};

static void private__monitor_files(tm_the_truth_o* tt)
{
    for (struct monitored_file_t* m = monitored_files; m != tm_carray_end(monitored_files); ++m) {
        if (m->tt != tt)
            continue;

        tm_file_stat_t stat = tm_os_api->file_system->stat(m->path);
        if (!stat.exists) {
            m->tt = 0;
            continue;
        }

        if (stat.size != m->stat.size || memcmp(&stat.last_modified_time, &m->stat.last_modified_time, sizeof(tm_file_time_o)) != 0) {
            tm_tt_undo_scope_t undo_scope = tm_the_truth_api->create_undo_scope(tt, TM_LOCALIZE("Read text from external file"));
            read_c_file_from_path(tt, m->c_file, m->path, undo_scope);
            private__compile(m->tt, m->c_file, undo_scope, 0);
            m->stat = stat;

            // TODO: Should push to global undo stack here.
        }
    }
}

static void plugin__tick(struct tm_plugin_o* inst, float dt)
{
    tm_the_truth_o* tt = tm_the_machinery_api->get_truth(tm_the_machinery_api->app);
    private__monitor_files(tt);
}

static struct tm_plugin_tick_i plugin_tick_i = {
    .tick = plugin__tick,
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
    tm_the_truth_api->set_aspect(tt, c_file, TM_TT_ASPECT__ASSET_OPEN, &asset__open__c_file_i);
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
    tm_allocator_api = reg->get(TM_ALLOCATOR_API_NAME);
    tm_the_machinery_api = reg->get(TM_THE_MACHINERY_API_NAME);

    allocator = tm_allocator_api->system;

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, truth__create_types);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_CREATE_ASSET_INTERFACE_NAME, &asset_browser__create_asset__c_file_i);
    tm_add_or_remove_implementation(reg, load, TM_PLUGIN_TICK_INTERFACE_NAME, &plugin_tick_i);
}