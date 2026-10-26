# -*- coding: utf-8 -*-
path = r'D:\EMULADORES\Git\AlencarSoftTVBOX\AlencarSoftTVBOX-Clang\src\main.c'
with open(path, 'rb') as f:
    data = f.read()

text = data.decode('utf-8')

vfd_func = """static void app_update_vfd_channel(app_context_t *app, int ch_idx) {
    if (!app || ch_idx < 0 || ch_idx >= app->catalog.total_channels) return;
    iptv_channel_t *ch = &app->catalog.channels[ch_idx];

    char cat_initial = 'G';
    int channel_num = ch_idx + 1;

    /* Procura na categoria atualmente selecionada */
    if (app->current_category_idx >= 0 && app->current_category_idx < app->catalog.total_categories) {
        iptv_category_t *cat = &app->catalog.categories[app->current_category_idx];
        for (int r = 0; r < cat->count; r++) {
            if (cat->channel_indices[r] == ch_idx) {
                for (const char *p = cat->name; *p; p++) {
                    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                        cat_initial = (char)toupper((unsigned char)*p);
                        break;
                    }
                }
                channel_num = r + 1;
                goto cat_done;
            }
        }
    }

    /* Fallback na lista de categorias */
    for (int c = 0; c < app->catalog.total_categories; c++) {
        iptv_category_t *cat = &app->catalog.categories[c];
        if (strcmp(cat->name, "Todos os Canais") == 0) continue;
        for (int r = 0; r < cat->count; r++) {
            if (cat->channel_indices[r] == ch_idx) {
                for (const char *p = cat->name; *p; p++) {
                    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                        cat_initial = (char)toupper((unsigned char)*p);
                        break;
                    }
                }
                channel_num = r + 1;
                goto cat_done;
            }
        }
    }
cat_done:

    char clean_name[64];
    format_clean_channel_name(ch->name, clean_name, sizeof(clean_name));
    for (char *p = clean_name; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }

    /* Formato exato do visor: Exemplo "N01 TV CULTURA" */
    char vfd_buf[64];
    snprintf(vfd_buf, sizeof(vfd_buf), "%c%02d %s", cat_initial, channel_num, clean_name);
    vfd_set_text(vfd_buf);
    printf("[%s] [VFD] '%s'\\n", APP_NAME, vfd_buf);
    fflush(stdout);
}
"""

marker = 'void app_render(app_context_t *app) {'
if marker in text:
    text = text.replace(marker, vfd_func + '\n' + marker, 1)
    print('Inserted vfd_func')

# Substituicoes com suporte a CRLF ou LF
replacements = [
    (
        'app_select_channel(app, index);\n    vfd_set_text(ch->name);',
        'app_select_channel(app, index);\n    app_update_vfd_channel(app, index);'
    ),
    (
        'app->view = VIEW_PLAYING;\n                    vfd_set_text(ch->name);',
        'app->view = VIEW_PLAYING;\n                    app_update_vfd_channel(app, ch_idx);'
    ),
    (
        '''        case KEY_NAV_UP:
            if (app->current_channel_idx > 0) {
                app->current_channel_idx--;
                if (app->current_channel_idx < app->scroll_offset) {
                    app->scroll_offset = app->current_channel_idx;
                }
            }
            break;''',
        '''        case KEY_NAV_UP:
            if (app->current_channel_idx > 0) {
                app->current_channel_idx--;
                if (app->current_channel_idx < app->scroll_offset) {
                    app->scroll_offset = app->current_channel_idx;
                }
                if (cat && app->current_channel_idx < cat->count) {
                    app_update_vfd_channel(app, cat->channel_indices[app->current_channel_idx]);
                }
            }
            break;'''
    ),
    (
        '''        case KEY_NAV_DOWN:
            if (cat && app->current_channel_idx + 1 < cat->count) {
                app->current_channel_idx++;
                if (app->current_channel_idx >= app->scroll_offset + app->channels_per_page) {
                    app->scroll_offset = app->current_channel_idx - app->channels_per_page + 1;
                }
            }
            break;''',
        '''        case KEY_NAV_DOWN:
            if (cat && app->current_channel_idx + 1 < cat->count) {
                app->current_channel_idx++;
                if (app->current_channel_idx >= app->scroll_offset + app->channels_per_page) {
                    app->scroll_offset = app->current_channel_idx - app->channels_per_page + 1;
                }
                app_update_vfd_channel(app, cat->channel_indices[app->current_channel_idx]);
            }
            break;'''
    ),
    (
        '''        case KEY_NAV_LEFT:
            if (app->current_category_idx > 0) {
                app->current_category_idx--;
                app->current_channel_idx = 0;
                app->scroll_offset = 0;
            }
            break;

        case KEY_NAV_RIGHT:
            if (app->current_category_idx + 1 < app->catalog.total_categories) {
                app->current_category_idx++;
                app->current_channel_idx = 0;
                app->scroll_offset = 0;
            }
            break;''',
        '''        case KEY_NAV_LEFT:
            if (app->current_category_idx > 0) {
                app->current_category_idx--;
                app->current_channel_idx = 0;
                app->scroll_offset = 0;
                cat = &app->catalog.categories[app->current_category_idx];
                if (cat && cat->count > 0) app_update_vfd_channel(app, cat->channel_indices[0]);
            }
            break;

        case KEY_NAV_RIGHT:
            if (app->current_category_idx + 1 < app->catalog.total_categories) {
                app->current_category_idx++;
                app->current_channel_idx = 0;
                app->scroll_offset = 0;
                cat = &app->catalog.categories[app->current_category_idx];
                if (cat && cat->count > 0) app_update_vfd_channel(app, cat->channel_indices[0]);
            }
            break;'''
    )
]

for old, new in replacements:
    if old in text:
        text = text.replace(old, new, 1)
        print('Applied replacement (LF)')
    else:
        old_crlf = old.replace('\n', '\r\n')
        new_crlf = new.replace('\n', '\r\n')
        if old_crlf in text:
            text = text.replace(old_crlf, new_crlf, 1)
            print('Applied replacement (CRLF)')
        else:
            print('WARNING: Not found:', old[:30])

with open(path, 'wb') as f:
    f.write(text.encode('utf-8'))
print('Saved main.c successfully!')
