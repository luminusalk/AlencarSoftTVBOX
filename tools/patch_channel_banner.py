import os

path = r"D:\EMULADORES\Git\AlencarSoftTVBOX\AlencarSoftTVBOX-Clang\src\main.c"
with open(path, "rb") as f:
    content = f.read()

# 1. Add <sys/time.h>, app_time_ms(), s_banner_end_ms and forward declaration
target1 = b'#include <pthread.h>\n'
replacement1 = (b'#include <pthread.h>\n'
                b'#include <sys/time.h>\n\n'
                b'static uint64_t app_time_ms(void) {\n'
                b'    struct timeval tv;\n'
                b'    gettimeofday(&tv, NULL);\n'
                b'    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;\n'
                b'}\n\n'
                b'static uint64_t s_banner_end_ms = 0;\n'
                b'static void app_draw_channel_banner(app_context_t *app, const char *raw_name);\n')

c_norm = content.replace(b'\r\n', b'\n')
assert target1 in c_norm, "target1 not found"
c_norm = c_norm.replace(target1, replacement1, 1)

# 2. Update app_start_channel to show banner and set 1.8s timer
target2 = (b'#ifdef RECEIVER_DLNA_BACKEND\n'
           b'    /* Player startup can wait for HLS. Hold an opaque frame above the\n'
           b'     * satellite menu while the DLNA renderer switches sources. */\n'
           b'    fb_clear(&app->fb, COLOR_DARK_BG);\n'
           b'    fb_draw_text_fit(&app->fb, app->fb.width / 8, app->fb.height / 2,\n'
           b'                     "Conectando ao canal...", app->fb.width * 3 / 4,\n'
           b'                     COLOR_WHITE, 1);\n'
           b'    fb_flip(&app->fb);\n'
           b'#endif')

replacement2 = (b'#ifdef RECEIVER_DLNA_BACKEND\n'
                b'    /* Mostra a tarja centralizada com o nome do canal enquanto o stream conecta */\n'
                b'    fb_clear(&app->fb, 0x00000000);\n'
                b'    app_draw_channel_banner(app, ch->name);\n'
                b'    fb_flip(&app->fb);\n'
                b'#endif\n'
                b'    s_banner_end_ms = app_time_ms() + 1800;')

assert target2 in c_norm, "target2 not found"
c_norm = c_norm.replace(target2, replacement2, 1)

# 3. Add app_draw_channel_banner function after format_clean_channel_name
target3 = (b'static void format_clean_channel_name(const char *raw, char *out, size_t max_len) {\n'
           b'    if (!raw || !out || max_len == 0) return;\n'
           b'    size_t j = 0;\n'
           b'    int in_bracket = 0;\n'
           b'    for (size_t i = 0; raw[i] && j + 1 < max_len; i++) {\n'
           b'        if (raw[i] == \'[\' || raw[i] == \'(\') {\n'
           b'            in_bracket = 1;\n'
           b'            continue;\n'
           b'        }\n'
           b'        if (in_bracket) {\n'
           b'            if (raw[i] == \']\' || raw[i] == \')\') in_bracket = 0;\n'
           b'            continue;\n'
           b'        }\n'
           b'        out[j++] = raw[i];\n'
           b'    }\n'
           b'    out[j] = \'\\0\';\n'
           b'    while (j > 0 && (out[j - 1] == \' \' || out[j - 1] == \'\\t\' || out[j - 1] == \'-\')) {\n'
           b'        out[--j] = \'\\0\';\n'
           b'    }\n'
           b'}')

replacement3 = target3 + (b'\n\n'
                          b'static void app_draw_channel_banner(app_context_t *app, const char *raw_name) {\n'
                          b'    if (!app || !app->fb.back_buffer) return;\n\n'
                          b'    char clean_name[128];\n'
                          b'    format_clean_channel_name(raw_name ? raw_name : "Canal IPTV", clean_name, sizeof(clean_name));\n\n'
                          b'    int text_len = (int)strlen(clean_name);\n'
                          b'    if (text_len == 0) return;\n\n'
                          b'    /* Usa scale=2 para legibilidade em TV ou scale=1 se o nome for muito longo */\n'
                          b'    int scale = 2;\n'
                          b'    if (text_len * FONT_WIDTH * 2 + 56 > app->fb.width - 48) {\n'
                          b'        scale = 1;\n'
                          b'    }\n\n'
                          b'    int text_w = text_len * FONT_WIDTH * scale;\n'
                          b'    /* Tarja um pouco maior que o nome do canal */\n'
                          b'    int pad_x = (scale == 2) ? 28 : 20;\n'
                          b'    int bar_w = text_w + pad_x * 2;\n'
                          b'    if (bar_w < 180) bar_w = 180;\n'
                          b'    if (bar_w > app->fb.width - 32) bar_w = app->fb.width - 32;\n\n'
                          b'    int bar_h = (scale == 2) ? 52 : 38;\n\n'
                          b'    /* Centralizada perfeitamente na tela (X e Y) */\n'
                          b'    int bar_x = (app->fb.width - bar_w) / 2;\n'
                          b'    int bar_y = (app->fb.height - bar_h) / 2;\n\n'
                          b'    /* Caixa do painel, borda sutil e friso azul de destaque */\n'
                          b'    fb_draw_rect(&app->fb, bar_x, bar_y, bar_w, bar_h, COLOR_PANEL_BG);\n'
                          b'    fb_draw_rect_outline(&app->fb, bar_x, bar_y, bar_w, bar_h, 1, 0xFF2A3448);\n'
                          b'    fb_draw_rect(&app->fb, bar_x, bar_y, 4, bar_h, COLOR_ACCENT);\n\n'
                          b'    /* Texto centralizado dentro da tarja */\n'
                          b'    int text_x = bar_x + (bar_w - text_w) / 2;\n'
                          b'    if (text_x < bar_x + 8) text_x = bar_x + 8;\n'
                          b'    int text_y = bar_y + (bar_h - FONT_HEIGHT * scale) / 2;\n'
                          b'    fb_draw_text_fit(&app->fb, text_x, text_y, clean_name, bar_w - 16, COLOR_WHITE, scale);\n'
                          b'}')

assert target3 in c_norm, "target3 not found"
c_norm = c_norm.replace(target3, replacement3, 1)

# 4. Replace buffering & playing blocks in app_render with unified centered banner
target4 = (b'    if (app->view == VIEW_PLAYING && playback_state == PLAYER_STATE_BUFFERING) {\n'
           b'        const char *name = "Canal IPTV";\n'
           b'        int panel_w = app->fb.width * 3 / 5;\n'
           b'        int panel_x = (app->fb.width - panel_w) / 2;\n'
           b'        int panel_y = app->fb.height / 2 - 60;\n'
           b'        if (app->active_playing_idx >= 0 &&\n'
           b'            app->active_playing_idx < app->catalog.total_channels)\n'
           b'            name = app->catalog.channels[app->active_playing_idx].name;\n'
           b'        fb_clear(&app->fb, COLOR_DARK_BG);\n'
           b'        fb_draw_rect(&app->fb, panel_x, panel_y, panel_w, 120, COLOR_PANEL_BG);\n'
           b'        fb_draw_rect(&app->fb, panel_x, panel_y, 6, 120, COLOR_ACCENT);\n'
           b'        fb_draw_text_fit(&app->fb, panel_x + 24, panel_y + 25,\n'
           b'                         name, panel_w - 48, COLOR_WHITE, 1);\n'
           b'        fb_draw_text(&app->fb, panel_x + 24, panel_y + 68,\n'
           b'                     "Conectando ao canal...", COLOR_TEXT_MUTED, 1);\n'
           b'        fb_flip(&app->fb);\n'
           b'        return;\n'
           b'    }\n'
           b'    if (app->view == VIEW_PLAYING && playback_state == PLAYER_STATE_PLAYING) {\n'
           b'        if (last_channel != app->active_playing_idx || last_view != app->view) {\n'
           b'            play_start_time = time(NULL);\n'
           b'            last_channel = app->active_playing_idx;\n'
           b'            last_view = app->view;\n'
           b'        }\n'
           b'\n'
           b'        /* Modo Reproducao: Framebuffer 100% transparente para permitir que o\n'
           b'         * plano de hardware de video subjacente seja exibido diretamente na TV */\n'
           b'        fb_clear(&app->fb, 0x00000000);\n'
           b'\n'
           b'        /* Barra de informacoes inferior sobreposta com identificacao do canal por 5s */\n'
           b'        time_t now = time(NULL);\n'
           b'        if (now - play_start_time <= 5) {\n'
           b'            if (app->active_playing_idx >= 0 && app->active_playing_idx < app->catalog.total_channels) {\n'
           b'                iptv_channel_t *ch = &app->catalog.channels[app->active_playing_idx];\n'
           b'                char clean_title[128];\n'
           b'                format_clean_channel_name(ch->name, clean_title, sizeof(clean_title));\n'
           b'\n'
           b'                int safe_x = (app->fb.width * 5) / 100;\n'
           b'                if (safe_x < 24) safe_x = 24;\n'
           b'                int bar_w = app->fb.width - 2 * safe_x;\n'
           b'                int bar_h = 56;\n'
           b'                int bar_y = app->fb.height - (app->fb.height * 6 / 100) - bar_h;\n'
           b'\n'
           b'                fb_draw_rect(&app->fb, safe_x, bar_y, bar_w, bar_h, COLOR_PANEL_BG);\n'
           b'                fb_draw_rect(&app->fb, safe_x, bar_y, 6, bar_h, COLOR_ACCENT);\n'
           b'                fb_draw_text_fit(&app->fb, safe_x + 20, bar_y + (bar_h - 16) / 2, clean_title,\n'
           b'                                 bar_w - 40, COLOR_WHITE, 1);\n'
           b'                if (app->status_message[0])\n'
           b'                    fb_draw_text_fit(&app->fb, safe_x + 20, bar_y - 24,\n'
           b'                                     app->status_message, bar_w - 40, COLOR_WHITE, 1);\n'
           b'            }\n'
           b'        }\n'
           b'\n'
           b'        fb_flip(&app->fb);\n'
           b'        return;\n'
           b'    }')

replacement4 = (b'    if (app->view == VIEW_PLAYING) {\n'
                b'        /* Modo Reproducao: Framebuffer 100% transparente */\n'
                b'        fb_clear(&app->fb, 0x00000000);\n\n'
                b'        /* Tarja centralizada com o nome do canal (aparece rapido e sumir em 1.8s) */\n'
                b'        uint64_t now_ms = app_time_ms();\n'
                b'        if (now_ms < s_banner_end_ms && app->active_playing_idx >= 0 &&\n'
                b'            app->active_playing_idx < app->catalog.total_channels) {\n'
                b'            iptv_channel_t *ch = &app->catalog.channels[app->active_playing_idx];\n'
                b'            app_draw_channel_banner(app, ch->name);\n'
                b'        }\n\n'
                b'        fb_flip(&app->fb);\n'
                b'        return;\n'
                b'    }')

assert target4 in c_norm, "target4 not found"
c_norm = c_norm.replace(target4, replacement4, 1)

# 5. Remove unused play_start_time variable in app_render
target5 = b'    static time_t play_start_time = 0;\n'
if target5 in c_norm:
    c_norm = c_norm.replace(target5, b'', 1)

# 6. Update main loop: remove play_banner_timeout, use s_banner_end_ms
target6 = (b'            time_t start_time = time(NULL);\n'
           b'            time_t last_sec = 0;\n'
           b'            time_t play_banner_timeout = 0;\n'
           b'            int needs_render = 0;')

replacement6 = (b'            time_t start_time = time(NULL);\n'
                b'            time_t last_sec = 0;\n'
                b'            int needs_render = 0;')

assert target6 in c_norm, "target6 not found"
c_norm = c_norm.replace(target6, replacement6, 1)

# 7. Update key press in main loop: remove play_banner_timeout = time(NULL) + 5
target7 = (b'                    app_handle_input(&s_app, key);\n'
           b'                    needs_render = 1;\n'
           b'                    if (s_app.view == VIEW_PLAYING) {\n'
           b'                        play_banner_timeout = time(NULL) + 5;\n'
           b'                    }')

replacement7 = (b'                    app_handle_input(&s_app, key);\n'
                b'                    needs_render = 1;')

assert target7 in c_norm, "target7 not found"
c_norm = c_norm.replace(target7, replacement7, 1)

# 8. Update timer check in main loop to check s_banner_end_ms
target8 = (b'                time_t now = time(NULL);\n'
           b'                if (now != last_sec) {\n'
           b'                    last_sec = now;\n'
           b'                    /* A grade so muda por acao do usuario. Uma copia integral\n'
           b'                     * de /dev/fb0 a cada segundo provoca tearing no GX6622. */\n'
           b'                    /* Em reproducao, limpa o banner quando expirar os 5 segundos */\n'
           b'                    if (s_app.view == VIEW_PLAYING && play_banner_timeout > 0 && now >= play_banner_timeout) {\n'
           b'                        play_banner_timeout = 0;\n'
           b'                        needs_render = 1;\n'
           b'                    }\n'
           b'                }')

replacement8 = (b'                time_t now = time(NULL);\n'
                b'                uint64_t now_ms = app_time_ms();\n'
                b'                if (now != last_sec) {\n'
                b'                    last_sec = now;\n'
                b'                }\n\n'
                b'                /* Em reproducao, limpa o banner quando expirar o tempo (aparece rapido e sumir) */\n'
                b'                if (s_app.view == VIEW_PLAYING && s_banner_end_ms > 0 && now_ms >= s_banner_end_ms) {\n'
                b'                    s_banner_end_ms = 0;\n'
                b'                    needs_render = 1;\n'
                b'                }')

assert target8 in c_norm, "target8 not found"
c_norm = c_norm.replace(target8, replacement8, 1)

# 9. Update cur_st state change in main loop: remove play_banner_timeout = time(NULL) + 5
target9 = (b'                        if (cur_st == PLAYER_STATE_PLAYING && s_app.active_playing_idx >= 0) {\n'
           b'                            channel_history_save(&s_app.catalog.channels[s_app.active_playing_idx], CHANNEL_HISTORY_PATH);\n'
           b'                            play_banner_timeout = time(NULL) + 5;\n'
           b'                        }')

replacement9 = (b'                        if (cur_st == PLAYER_STATE_PLAYING && s_app.active_playing_idx >= 0) {\n'
                b'                            channel_history_save(&s_app.catalog.channels[s_app.active_playing_idx], CHANNEL_HISTORY_PATH);\n'
                b'                        }')

assert target9 in c_norm, "target9 not found"
c_norm = c_norm.replace(target9, replacement9, 1)

# Re-apply CRLF if original had it
is_crlf = b'\r\n' in content
if is_crlf:
    final_content = c_norm.replace(b'\n', b'\r\n')
else:
    final_content = c_norm

with open(path, "wb") as f:
    f.write(final_content)

print("Todas as 9 modificacoes aplicadas com sucesso em main.c!")
