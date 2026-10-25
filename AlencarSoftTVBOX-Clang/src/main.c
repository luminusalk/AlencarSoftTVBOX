/**
 * @file main.c
 * @brief Aplicativo nativo C para TV Box e Receptores Satelite (AlencarSoftTVBOX-Clang).
 */

#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int app_init(app_context_t *app) {
    memset(app, 0, sizeof(*app));
    app->running = 1;
    app->view = VIEW_CATEGORIES;
    app->channels_per_page = OSD_MAX_CHANNELS_PER_PAGE;
    
    /* 1. Deteccao automatica de hardware */
    app->platform = platform_detect();
    printf("[%s] Hardware detectado: %s (SoC=%d, Painel=%d)\n", 
           APP_NAME, app->platform.model_name, app->platform.soc, app->platform.panel);

    /* 2. Inicializacao do visor frontal */
    vfd_init(app->platform.panel);
    vfd_set_text("ALENCARSOFT");

    /* 3. Inicializacao do Framebuffer (/dev/fb0) */
    if (fb_init(&app->fb, DEFAULT_FB_DEV) < 0) {
        fprintf(stderr, "Aviso: Nao foi possivel abrir %s diretamente.\n", DEFAULT_FB_DEV);
    }

    /* 4. Inicializacao do leitor de controle remoto */
    input_init(DEFAULT_IR_DEV);

    /* 5. Inicializacao do Player de video */
    player_init();

    /* 6. Carrega catalogo inicial (M3U ou JSON) */
    if (catalog_load_m3u(&app->catalog, DEFAULT_M3U_PATH) <= 0) {
        catalog_load_m3u(&app->catalog, "/mnt/plugin/lists/Canais ao Vivo.m3u");
    }

    /* Fallback com canais essenciais caso nao haja lista salva */
    if (app->catalog.total_channels == 0) {
        strcpy(app->catalog.channels[0].name, "TV Cultura ao vivo [720p HD]");
        strcpy(app->catalog.channels[0].url, "https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8");
        strcpy(app->catalog.channels[0].category, "Canais Abertos");
        app->catalog.channels[0].is_hd = 1;
        app->catalog.total_channels = 1;

        strcpy(app->catalog.categories[0].name, "Canais Abertos");
        app->catalog.categories[0].channel_indices[0] = 0;
        app->catalog.categories[0].count = 1;
        app->catalog.total_categories = 1;
    }

    strcpy(app->status_message, "Grade carregada com sucesso.");
    return 0;
}

void app_render(app_context_t *app) {
    if (!app->fb.back_buffer) return;

    /* Fundo escuro elegante */
    fb_clear(&app->fb, COLOR_DARK_BG);

    /* --- CABECALHO (Header) --- */
    fb_draw_rect(&app->fb, 0, 0, app->fb.width, 70, COLOR_PANEL_BG);
    fb_draw_text(&app->fb, 40, 24, APP_NAME, COLOR_ACCENT, 2);

    char model_tag[64];
    snprintf(model_tag, sizeof(model_tag), "[%s]", app->platform.model_name);
    fb_draw_text(&app->fb, 360, 28, model_tag, COLOR_TEXT_MUTED, 1);

    /* Relogio no canto direito */
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char clock_str[16];
    snprintf(clock_str, sizeof(clock_str), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    fb_draw_text(&app->fb, app->fb.width - 120, 24, clock_str, COLOR_WHITE, 2);

    /* --- BARRA DE CATEGORIAS (Tabs horizontais) --- */
    fb_draw_rect(&app->fb, 0, 72, app->fb.width, 50, COLOR_PANEL_BG);
    int cat_x = 40;
    for (int i = 0; i < app->catalog.total_categories && i < 6; i++) {
        uint32_t tab_color = (i == app->current_category_idx) ? COLOR_ACCENT : COLOR_TEXT_MUTED;
        if (i == app->current_category_idx) {
            fb_draw_rect(&app->fb, cat_x - 10, 76, 180, 42, COLOR_HIGHLIGHT);
            fb_draw_text(&app->fb, cat_x, 88, app->catalog.categories[i].name, COLOR_WHITE, 1);
        } else {
            fb_draw_text(&app->fb, cat_x, 88, app->catalog.categories[i].name, tab_color, 1);
        }
        cat_x += 200;
    }

    /* --- GRADE DE CANAIS (Centro) --- */
    if (app->current_category_idx < app->catalog.total_categories) {
        iptv_category_t *cat = &app->catalog.categories[app->current_category_idx];
        int start_idx = app->scroll_offset;
        int y_pos = 140;

        for (int i = 0; i < app->channels_per_page && (start_idx + i) < cat->count; i++) {
            int ch_idx = cat->channel_indices[start_idx + i];
            iptv_channel_t *ch = &app->catalog.channels[ch_idx];
            int is_selected = (start_idx + i) == app->current_channel_idx;

            /* Item da grade */
            uint32_t item_bg = is_selected ? COLOR_HIGHLIGHT : COLOR_PANEL_BG;
            fb_draw_rect(&app->fb, 40, y_pos, app->fb.width - 80, 50, item_bg);

            if (is_selected) {
                fb_draw_rect_outline(&app->fb, 40, y_pos, app->fb.width - 80, 50, 2, COLOR_ACCENT);
            }

            /* Numero */
            char num_str[16];
            snprintf(num_str, sizeof(num_str), "%03d", start_idx + i + 1);
            fb_draw_text(&app->fb, 60, y_pos + 16, num_str, COLOR_ACCENT, 1);

            /* Nome do Canal */
            fb_draw_text(&app->fb, 120, y_pos + 16, ch->name, COLOR_WHITE, 1);

            /* Badges de Qualidade */
            if (ch->is_fullhd) {
                fb_draw_rect(&app->fb, app->fb.width - 240, y_pos + 12, 140, 26, COLOR_RED_BADGE);
                fb_draw_text(&app->fb, app->fb.width - 225, y_pos + 17, "FULL HD", COLOR_WHITE, 1);
            } else if (ch->is_hd) {
                fb_draw_rect(&app->fb, app->fb.width - 200, y_pos + 12, 100, 26, COLOR_GREEN_LIVE);
                fb_draw_text(&app->fb, app->fb.width - 180, y_pos + 17, "HD 720p", COLOR_BLACK, 1);
            }

            y_pos += 56;
        }
    }

    /* --- RODAPE (Footer informativo e controle remoto) --- */
    fb_draw_rect(&app->fb, 0, app->fb.height - 60, app->fb.width, 60, COLOR_PANEL_BG);
    fb_draw_text(&app->fb, 40, app->fb.height - 40, 
                 "[OK] Assistir   [SETAS] Navegar Grade   [MENU] Atualizar Grade   [EXIT/BACK] Sair", 
                 COLOR_TEXT_MUTED, 1);

    /* Envia o buffer pronto para a tela */
    fb_flip(&app->fb);
}

void app_handle_input(app_context_t *app, app_key_t key) {
    if (key == KEY_NONE) return;

    if (app->current_category_idx >= app->catalog.total_categories) {
        app->current_category_idx = 0;
    }
    iptv_category_t *cat = &app->catalog.categories[app->current_category_idx];

    switch (key) {
        case KEY_NAV_UP:
            if (app->current_channel_idx > 0) {
                app->current_channel_idx--;
                if (app->current_channel_idx < app->scroll_offset) {
                    app->scroll_offset = app->current_channel_idx;
                }
            }
            break;

        case KEY_NAV_DOWN:
            if (cat && app->current_channel_idx + 1 < cat->count) {
                app->current_channel_idx++;
                if (app->current_channel_idx >= app->scroll_offset + app->channels_per_page) {
                    app->scroll_offset = app->current_channel_idx - app->channels_per_page + 1;
                }
            }
            break;

        case KEY_NAV_LEFT:
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
            break;

        case KEY_ACTION_OK:
            if (cat && cat->count > 0) {
                int ch_idx = cat->channel_indices[app->current_channel_idx];
                iptv_channel_t *ch = &app->catalog.channels[ch_idx];
                printf("[%s] Selecionado canal: %s -> URL: %s\n", APP_NAME, ch->name, ch->url);

                /* Atualiza o visor frontal fisico imediatamente */
                vfd_set_text(ch->name);

                /* Inicia player */
                player_play(ch->url, ch->name);
                app->active_playing_idx = ch_idx;
            }
            break;

        case KEY_ACTION_BACK:
            /* Se estiver assistindo a um canal, volta para a grade */
            if (player_get_state() == PLAYER_STATE_PLAYING) {
                player_stop();
                vfd_set_text("GRADE");
            } else {
                /* Se ja estiver na grade, encerra o app limpo */
                app->running = 0;
            }
            break;

        case KEY_EXIT_APP:
            app->running = 0;
            break;

        case KEY_ACTION_MENU:
            printf("[%s] Atualizando grade de canais a partir do GitHub...\n", APP_NAME);
            vfd_set_text("ATUALIZANDO");
            app_update_catalog_from_web(app);
            vfd_set_text("GRADE");
            break;

        default:
            break;
    }
}

void app_update_catalog_from_web(app_context_t *app) {
    /* Faz download da grade atualizada do repositorio GitHub */
    const char *cmd = "wget -q -O /mnt/plugin/iptv/iptv.m3u8 "
                      "https://raw.githubusercontent.com/luminusalk/Iptv-Brasil-2026/master/az-iptv/lists/Canais%20ao%20Vivo.m3u "
                      "2>/dev/null || true";
    system(cmd);
    catalog_load_m3u(&app->catalog, DEFAULT_M3U_PATH);
    strcpy(app->status_message, "Grade atualizada com sucesso!");
}

void app_cleanup(app_context_t *app) {
    player_close();
    vfd_set_text("PRONTO");
    vfd_close();
    input_close();
    fb_close(&app->fb);
    printf("[%s] Aplicativo finalizado com sucesso.\n", APP_NAME);
}

int main(int argc, char **argv) {
    app_context_t app;

    if (app_init(&app) < 0) {
        fprintf(stderr, "Erro ao inicializar %s\n", APP_NAME);
        return 1;
    }

    printf("[%s] Iniciando loop de eventos nativo...\n", APP_NAME);
    while (app.running) {
        app_key_t key = input_poll();
        if (key != KEY_NONE) {
            app_handle_input(&app, key);
        }
        app_render(&app);
        usleep(30000); /* ~33 FPS para suavidade maxima com CPU baixa */
    }

    app_cleanup(&app);
    return 0;
}
