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
#include <signal.h>
#include <fcntl.h>

static app_context_t s_app;

static void sig_handler(int sig) {
    (void)sig;
    s_app.running = 0;
}

int app_init(app_context_t *app) {
    system("echo APP_INIT_START > /tmp/alencar_status.txt");
    memset(app, 0, sizeof(*app));
    app->running = 1;
    app->view = VIEW_CATEGORIES;
    app->channels_per_page = OSD_MAX_CHANNELS_PER_PAGE;
    
    /* Configura tratamento de sinais para encerramento seguro */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    /* O backend DLNA depende da instancia original do dvbapp. Nao substituir
     * nos de /dev nem reiniciar o firmware para obter o controle remoto. */
#ifndef RECEIVER_DLNA_BACKEND
    system("echo PAUSING_DVBAPP >> /tmp/alencar_status.txt");
    system("killall -9 dvbapp 2>/dev/null; umount -l /app 2>/dev/null;");
#endif

    /* 2. Deteccao automatica de hardware */
    system("echo DETECTING_PLATFORM >> /tmp/alencar_status.txt");
    app->platform = platform_detect();
    printf("[%s] Hardware detectado: %s (SoC=%d, Painel=%d)\n", 
           APP_NAME, app->platform.model_name, app->platform.soc, app->platform.panel);
    fflush(stdout);

    /* 3. Inicializacao do visor frontal */
    system("echo INIT_VFD >> /tmp/alencar_status.txt");
    vfd_init(app->platform.panel);
    vfd_set_text("ALENCARSOFT");

    /* 4. Inicializacao do Framebuffer (/dev/fb0) */
    system("echo INIT_FB >> /tmp/alencar_status.txt");
    if (fb_init(&app->fb, DEFAULT_FB_DEV) < 0) {
        fprintf(stderr, "Aviso: Nao foi possivel abrir %s diretamente.\n", DEFAULT_FB_DEV);
        fflush(stderr);
    }

    /* 5. Inicializacao do leitor de controle remoto */
    system("echo INIT_INPUT >> /tmp/alencar_status.txt");
    input_init(DEFAULT_IR_DEV);

    /* 6. Inicializacao do Player de video */
    system("echo INIT_PLAYER >> /tmp/alencar_status.txt");
    player_init();

    /* 7. Inicializa e carrega catalogo inicial (M3U ou JSON) */
    system("echo INIT_CATALOG >> /tmp/alencar_status.txt");
    catalog_init(&app->catalog);
    if (catalog_load_m3u(&app->catalog, DEFAULT_M3U_PATH) <= 0) {
        catalog_load_m3u(&app->catalog, "/mnt/plugin/lists/Canais ao Vivo.m3u");
    }

    /* Fallback com canais essenciais caso nao haja lista salva */
    if (app->catalog.total_channels == 0) {
        strcpy(app->catalog.channels[0].name, "Canal Teste Local HD (Audio MP3)");
        strcpy(app->catalog.channels[0].url, "/mnt/plugin/video_mp3.mp4");
        strcpy(app->catalog.channels[0].category, "Canais Abertos");
        app->catalog.channels[0].is_hd = 1;

        strcpy(app->catalog.channels[1].name, "TV Cultura ao vivo [720p HD]");
        strcpy(app->catalog.channels[1].url, "https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8");
        strcpy(app->catalog.channels[1].category, "Canais Abertos");
        app->catalog.channels[1].is_hd = 1;
        app->catalog.total_channels = 2;

        strcpy(app->catalog.categories[0].name, "Canais Abertos");
        app->catalog.categories[0].channel_indices[0] = 0;
        app->catalog.categories[0].channel_indices[1] = 1;
        app->catalog.categories[0].count = 2;
        app->catalog.total_categories = 1;
    }

    strcpy(app->status_message, "Grade carregada com sucesso.");
    system("echo APP_INIT_DONE >> /tmp/alencar_status.txt");
    return 0;
}

void app_render(app_context_t *app) {
    player_state_t playback_state;
    static player_state_t last_rendered = PLAYER_STATE_STOPPED;
    static int last_channel = -2;
    static app_view_t last_view = (app_view_t)-1;
    static time_t play_start_time = 0;
    static int play_frame = 0;
    if (!app->fb.back_buffer) return;

    playback_state = player_get_state();
    if (app->view == VIEW_PLAYING && (playback_state == PLAYER_STATE_PLAYING || playback_state == PLAYER_STATE_BUFFERING)) {
        if (last_channel != app->active_playing_idx || last_view != app->view) {
            play_start_time = time(NULL);
            last_channel = app->active_playing_idx;
            last_rendered = playback_state;
            last_view = app->view;
            play_frame = 0;
        }

        /* Modo Reproducao: Framebuffer 100% transparente para permitir que o
         * plano de hardware de video subjacente seja exibido diretamente na TV */
        fb_clear(&app->fb, 0x00000000);

        /* Barra de informacoes inferior sobreposta com identificacao do canal por 5s */
        time_t now = time(NULL);
        if (now - play_start_time <= 5 || playback_state == PLAYER_STATE_BUFFERING) {
            if (app->active_playing_idx >= 0 && app->active_playing_idx < app->catalog.total_channels) {
                iptv_channel_t *ch = &app->catalog.channels[app->active_playing_idx];
                fb_draw_rect(&app->fb, 40, app->fb.height - 80, app->fb.width - 80, 50, COLOR_PANEL_BG);
                fb_draw_text(&app->fb, 60, app->fb.height - 65, ch->name, COLOR_WHITE, 1);
                fb_draw_text(&app->fb, app->fb.width - 280, app->fb.height - 65, "[BACK] Voltar a Grade", COLOR_ACCENT, 1);
                if (playback_state == PLAYER_STATE_BUFFERING)
                    fb_draw_text(&app->fb, 60, app->fb.height - 105, "Aguardando dados do canal...", COLOR_WHITE, 1);
            }
        }

        fb_flip(&app->fb);
        return;
    }

    last_rendered = PLAYER_STATE_STOPPED;
    last_channel = -2;
    last_view = app->view;

    /* Fundo escuro elegante para o Menu / Grade de Canais */
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
    if (playback_state == PLAYER_STATE_ERROR)
        fb_draw_text(&app->fb, 40, app->fb.height - 95,
                     "Falha ao iniciar canal. Consulte o log do aplicativo.", COLOR_WHITE, 1);

    /* Dialogo modal de confirmacao de saida para a TV Box original */
    if (app->view == VIEW_CONFIRM_EXIT) {
        int dw = 520;
        int dh = 200;
        int dx = (app->fb.width - dw) / 2;
        int dy = (app->fb.height - dh) / 2;

        fb_draw_rect(&app->fb, dx - 4, dy - 4, dw + 8, dh + 8, COLOR_HIGHLIGHT);
        fb_draw_rect(&app->fb, dx, dy, dw, dh, COLOR_PANEL_BG);

        fb_draw_text(&app->fb, dx + 40, dy + 45, "Deseja sair para o sistema original?", COLOR_WHITE, 1);

        int btn_w = 140;
        int btn_h = 45;
        int btn_y = dy + 115;
        int sim_x = dx + 80;
        int nao_x = dx + 300;

        uint32_t sim_bg = (app->exit_dialog_choice == 0) ? COLOR_HIGHLIGHT : COLOR_DARK_BG;
        uint32_t nao_bg = (app->exit_dialog_choice == 1) ? COLOR_HIGHLIGHT : COLOR_DARK_BG;

        fb_draw_rect(&app->fb, sim_x, btn_y, btn_w, btn_h, sim_bg);
        fb_draw_text(&app->fb, sim_x + 45, btn_y + 14, "SIM", COLOR_WHITE, 1);

        fb_draw_rect(&app->fb, nao_x, btn_y, btn_w, btn_h, nao_bg);
        fb_draw_text(&app->fb, nao_x + 45, btn_y + 14, "NAO", COLOR_WHITE, 1);
    }

    /* Envia o buffer pronto para a tela */
    fb_flip(&app->fb);
}

void app_handle_input(app_context_t *app, app_key_t key) {
    if (key == KEY_NONE) return;

    /* 1. Tratamento durante a reproducao de video em tela cheia */
    if (app->view == VIEW_PLAYING) {
        if (key == KEY_ACTION_BACK || key == KEY_EXIT_APP || key == KEY_ACTION_MENU) {
            /* NAO encerra o player DLNA! Apenas traz a grade para a frente.
             * Manter o DLNA ativo engana o firmware para nunca voltar ao satelite! */
            app->view = VIEW_CATEGORIES;
            vfd_set_text("GRADE");
            fb_clear(&app->fb, COLOR_DARK_BG);
            return;
        }
        return;
    }

    /* 2. Tratamento no dialogo de confirmacao de saida */
    if (app->view == VIEW_CONFIRM_EXIT) {
        if (key == KEY_NAV_LEFT || key == KEY_NAV_RIGHT) {
            app->exit_dialog_choice = !app->exit_dialog_choice;
        } else if (key == KEY_ACTION_BACK || key == KEY_EXIT_APP) {
            app->view = VIEW_CATEGORIES;
        } else if (key == KEY_ACTION_OK) {
            if (app->exit_dialog_choice == 0) {
                /* SIM: sai do app e restaura o dvbapp de fabrica */
                printf("[%s] Encerrando app e iniciando dvbapp de fabrica...\n", APP_NAME);
                vfd_set_text("SAINDO");
                player_stop();
                fb_clear(&app->fb, 0x00000000);
                fb_flip(&app->fb);
#ifndef RECEIVER_DLNA_BACKEND
                system("mount /dev/mtdblock4 /app -t squashfs 2>/dev/null; /app/dvbapp &");
#endif
                app->running = 0;
            } else {
                /* NAO: cancela e volta para a grade */
                app->view = VIEW_CATEGORIES;
            }
        }
        return;
    }

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
                printf("[%s] Selecionado canal: %s (idx=%d, ativo=%d)\n",
                       APP_NAME, ch->name, ch_idx, app->active_playing_idx);

                /* Se o canal selecionado ja esta em reproducao no player de fundo,
                 * nao reinicia o stream! Apenas oculta a grade de interface e vai para tela cheia */
                if (ch_idx == app->active_playing_idx && player_get_state() == PLAYER_STATE_PLAYING) {
                    printf("[%s] Canal '%s' ja esta tocando em background! Ocultando grade imediatamente...\n",
                           APP_NAME, ch->name);
                    app->view = VIEW_PLAYING;
                    vfd_set_text(ch->name);
                    fb_clear(&app->fb, 0x00000000);
                    fb_flip(&app->fb);
                    return;
                }

                /* Atualiza o visor frontal fisico imediatamente */
                vfd_set_text(ch->name);

                /* Inicia player para o novo canal */
                if (player_play(ch->url, ch->name) == 0) {
                    app->active_playing_idx = ch_idx;
                    app->view = VIEW_PLAYING;
                } else {
                    snprintf(app->status_message, sizeof(app->status_message),
                             "Falha ao abrir canal: %.90s", ch->name);
                    fprintf(stderr, "[%s] %s\n", APP_NAME, app->status_message);
                    vfd_set_text("FALHA");
                }
            }
            break;

        case KEY_ACTION_BACK:
        case KEY_EXIT_APP:
            /* Na grade de canais, abre o dialogo de confirmacao de saida */
            app->view = VIEW_CONFIRM_EXIT;
            app->exit_dialog_choice = 1; /* Padrao selecionado: NAO */
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
    catalog_free(&app->catalog);
    vfd_set_text("PRONTO");
    vfd_close();
    input_close();
    fb_clear(&app->fb, 0x00000000);
    fb_flip(&app->fb);
    fb_close(&app->fb);
    
    printf("[%s] Aplicativo finalizado com sucesso.\n", APP_NAME);
}

int main(int argc, char **argv) {

    system("echo MAIN_ENTERED > /tmp/alencar_status.txt");
    printf("\n=========================================\n");
    printf("   Iniciando %s v%s\n", APP_NAME, APP_VERSION);
    printf("=========================================\n");
    fflush(stdout);

    /* Modo C isolado (headless): testa o transporte DLNA e player C, com flags para isolar componentes */
    /* Modo Daemon EXIT 3x: escuta controle remoto e dispara o app se apertar EXIT 3 vezes */
    if (argc >= 2 && strcmp(argv[1], "--exit3-daemon") == 0) {
        input_init(DEFAULT_IR_DEV);
        printf("[%s] Daemon EXIT 3x ativo em segundo plano (PID %d).\n", APP_NAME, getpid());
        fflush(stdout);

        int exit_count = 0;
        time_t last_exit_time = 0;

        while (1) {
            app_key_t key = input_poll();
            if (key == KEY_EXIT_APP) {
                time_t now = time(NULL);
                if (now - last_exit_time <= 2) {
                    exit_count++;
                } else {
                    exit_count = 1;
                }
                last_exit_time = now;
                printf("[%s] [Daemon] EXIT detectado (%d/3)...\n", APP_NAME, exit_count);
                fflush(stdout);

                if (exit_count >= 3) {
                    printf("[%s] [Daemon] ATALHO EXIT 3x ACIONADO! Abrindo AlencarSoft TVBOX...\n", APP_NAME);
                    fflush(stdout);
                    exit_count = 0;
                    system("/mnt/plugin/alencarsoft_tvbox --headless-osd-ir 'https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8?vhost=player-tvcultura.stream.uol.com.br' 86400 &");
                }
            }
            usleep(25000);
        }
        return 0;
    }

    if (argc >= 2 && (strcmp(argv[1], "--headless") == 0 ||
                      strcmp(argv[1], "--headless-vfd") == 0 ||
                      strcmp(argv[1], "--headless-watchdog") == 0 ||
                      strcmp(argv[1], "--headless-fb") == 0 ||
                      strcmp(argv[1], "--headless-osd") == 0 ||
                      strcmp(argv[1], "--headless-input") == 0 ||
                      strcmp(argv[1], "--headless-osd-ir") == 0 ||
                      strcmp(argv[1], "--interactive") == 0)) {
        const char *source = (argc >= 3) ? argv[2] : "https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8?vhost=player-tvcultura.stream.uol.com.br";
        int duration = argc >= 4 ? atoi(argv[3]) : 15;
        int with_vfd = (strcmp(argv[1], "--headless-vfd") == 0);
        int with_watchdog = (strcmp(argv[1], "--headless-watchdog") == 0);
        int with_fb = (strcmp(argv[1], "--headless-fb") == 0);
        int with_osd = (strcmp(argv[1], "--headless-osd") == 0);
        int with_input = (strcmp(argv[1], "--headless-input") == 0);
        int with_osd_ir = (strcmp(argv[1], "--headless-osd-ir") == 0 || strcmp(argv[1], "--interactive") == 0);
        fb_context_t test_fb;
        memset(&test_fb, 0, sizeof(test_fb));

        if (with_osd_ir) {
            if (argc < 4) duration = 300;
            printf("[%s] [OSD+IR] Modo Interativo: duracao=%ds\n", APP_NAME, duration);
            fflush(stdout);

            /* Limpa fb1 (still layer 1080p do dvbapp) para evitar interferencia visual */
            int fb1_fd = open("/dev/fb1", O_WRONLY);
            if (fb1_fd >= 0) {
                uint8_t zero[4096];
                memset(zero, 0, sizeof(zero));
                for (int z = 0; z < (1920*1080*2)/4096; z++) write(fb1_fd, zero, sizeof(zero));
                close(fb1_fd);
            }

            memset(&s_app, 0, sizeof(s_app));
            s_app.platform = platform_detect();
            s_app.view = VIEW_CATEGORIES;
            s_app.channels_per_page = OSD_MAX_CHANNELS_PER_PAGE;
            s_app.running = 1;
            s_app.active_playing_idx = -1;

            catalog_init(&s_app.catalog);
            if (catalog_load_m3u(&s_app.catalog, DEFAULT_M3U_PATH) <= 0) {
                catalog_load_m3u(&s_app.catalog, "/mnt/plugin/lists/Canais ao Vivo.m3u");
            }
            if (s_app.catalog.total_channels == 0) {
                strcpy(s_app.catalog.channels[0].name, "TV Cultura ao vivo [720p HD]");
                strcpy(s_app.catalog.channels[0].url, source);
                strcpy(s_app.catalog.channels[0].category, "Notícias — Apuração");
                s_app.catalog.channels[0].is_hd = 1;
                s_app.catalog.total_channels = 1;
                strcpy(s_app.catalog.categories[0].name, "Notícias — Apuração");
                s_app.catalog.categories[0].channel_indices[0] = 0;
                s_app.catalog.categories[0].count = 1;
                s_app.catalog.total_categories = 1;
            }

            if (fb_init(&s_app.fb, DEFAULT_FB_DEV) < 0) {
                fprintf(stderr, "[%s] Falha ao abrir framebuffer para OSD+IR\n", APP_NAME);
                return 1;
            }

            input_init(DEFAULT_IR_DEV);
            player_init();

            /* Bloqueia o modo satelite do dvbapp iniciando imediatamente o DLNA em background */
            if (s_app.catalog.total_channels > 0) {
                iptv_channel_t *ch0 = &s_app.catalog.channels[0];
                printf("[%s] [OSD+IR] Enganando dvbapp: ativando modo DLNA continuo (%s)...\n",
                       APP_NAME, ch0->name);
                if (player_play(ch0->url, ch0->name) == 0) {
                    s_app.active_playing_idx = 0;
                }
            }
            s_app.view = VIEW_CATEGORIES; /* Garante que a grade de canais esta visivel sobreposta */

            printf("[%s] [OSD+IR] Grade carregada: %d canais, %d categorias.\n",
                   APP_NAME, s_app.catalog.total_channels, s_app.catalog.total_categories);
            printf("[%s] [OSD+IR] Categoria ativa: '%s'\n", APP_NAME, s_app.catalog.categories[0].name);
            printf("[%s] [OSD+IR] Use as SETAS (CIMA/BAIXO) para navegar na lista de noticias.\n", APP_NAME);
            printf("[%s] [OSD+IR] Pressione [OK] para abrir o canal selecionado.\n", APP_NAME);
            printf("[%s] [OSD+IR] Pressione [BACK/EXIT] durante o video para voltar a grade.\n", APP_NAME);
            fflush(stdout);

            app_render(&s_app);

            time_t start_time = time(NULL);
            time_t last_sec = 0;
            time_t play_banner_timeout = 0;
            int needs_render = 0;
            int loop_cnt = 0;
            while (s_app.running) {
                app_key_t key = input_poll();
                if (key != KEY_NONE) {
                    player_state_t cur_st = player_get_state();
                    printf("[%s] [OSD+IR] Tecla=%d (estado player=%d, canal=%d)\n",
                           APP_NAME, key, cur_st, s_app.current_channel_idx);
                    fflush(stdout);
                    app_handle_input(&s_app, key);
                    needs_render = 1;
                    if (s_app.view == VIEW_PLAYING) {
                        play_banner_timeout = time(NULL) + 5;
                    }
                }

                time_t now = time(NULL);
                if (now != last_sec) {
                    last_sec = now;
                    /* Na grade, atualiza o relogio a cada segundo */
                    if (s_app.view == VIEW_CATEGORIES) {
                        needs_render = 1;
                    }
                    /* Em reproducao, limpa o banner quando expirar os 5 segundos */
                    if (s_app.view == VIEW_PLAYING && play_banner_timeout > 0 && now >= play_banner_timeout) {
                        play_banner_timeout = 0;
                        needs_render = 1;
                    }
                }

                if (needs_render) {
                    app_render(&s_app);
                    needs_render = 0;
                }

                usleep(15000); /* 15ms de intervalo = ~66Hz de polling instantaneo com quase 0% CPU */

                if (++loop_cnt % 200 == 0) {
                    player_state_t cur_st = player_get_state();
                    printf("[%s] [OSD+IR] t=%ds/%ds, estado player=%d\n", APP_NAME,
                           (int)(time(NULL) - start_time), duration, cur_st);
                    fflush(stdout);
                }

                if (duration > 0 && (time(NULL) - start_time) > duration) {
                    printf("[%s] [OSD+IR] Tempo limite atingido (%ds). Encerrando...\n", APP_NAME, duration);
                    break;
                }
            }

            printf("[%s] [OSD+IR] Encerrando modo interativo e liberando recursos...\n", APP_NAME);
            player_stop();
            player_close();
            input_close();
            fb_clear(&s_app.fb, 0x00000000);
            fb_flip(&s_app.fb);
            fb_close(&s_app.fb);
            catalog_free(&s_app.catalog);
            return 0;
        }
        if (duration <= 0) duration = 15;
        printf("[%s] Modo HEADLESS isolado (%s): fonte='%s', duracao=%ds\n", APP_NAME, argv[1], source, duration);
        fflush(stdout);

        if (with_vfd) {
            platform_info_t plat = platform_detect();
            printf("[%s] Habilitando componente VFD...\n", APP_NAME);
            vfd_init(plat.panel);
            vfd_set_text("ALENCARSOFT");
        }
        if (with_input) {
            printf("[%s] Habilitando componente Input (IR)...\n", APP_NAME);
            input_init(DEFAULT_IR_DEV);
        }
        if (with_fb) {
            printf("[%s] Habilitando componente Framebuffer (/dev/fb0)...\n", APP_NAME);
            if (fb_init(&test_fb, DEFAULT_FB_DEV) == 0) {
                fb_clear(&test_fb, 0x00000000);
                fb_flip(&test_fb);
            }
        }
        if (with_osd) {
            printf("[%s] [OSD] Configurando interface grafica e grade...\n", APP_NAME);
            memset(&s_app, 0, sizeof(s_app));
            s_app.platform = platform_detect();
            s_app.view = VIEW_CATEGORIES;
            s_app.channels_per_page = OSD_MAX_CHANNELS_PER_PAGE;
            s_app.running = 1;
            catalog_init(&s_app.catalog);
            strcpy(s_app.catalog.channels[0].name, "TV Cultura ao vivo [720p HD]");
            strcpy(s_app.catalog.channels[0].url, source);
            strcpy(s_app.catalog.channels[0].category, "Canais Abertos");
            s_app.catalog.channels[0].is_hd = 1;
            s_app.catalog.total_channels = 1;
            strcpy(s_app.catalog.categories[0].name, "Canais Abertos");
            s_app.catalog.categories[0].channel_indices[0] = 0;
            s_app.catalog.categories[0].count = 1;
            s_app.catalog.total_categories = 1;

            if (fb_init(&s_app.fb, DEFAULT_FB_DEV) < 0) {
                fprintf(stderr, "[%s] Falha ao abrir framebuffer para OSD\n", APP_NAME);
                return 1;
            }

            printf("[%s] [OSD] Exibindo grade completa por 3 segundos...\n", APP_NAME);
            fflush(stdout);
            app_render(&s_app);
            sleep(3);
        }

        player_init();
        if (with_osd) s_app.active_playing_idx = 0;
        if (player_play(source, "Headless Test") < 0) {
            fprintf(stderr, "[%s] Falha no player_play headless\n", APP_NAME);
            if (with_osd) {
                fb_close(&s_app.fb);
                catalog_free(&s_app.catalog);
            }
            if (with_fb) fb_close(&test_fb);
            if (with_input) input_close();
            if (with_vfd) vfd_close();
            return 1;
        }

        for (int i = 0; i < duration; i++) {
            if (with_osd) {
                app_render(&s_app);
            }
            if (with_watchdog) {
                system("echo 1 > /dev/watchdog 2>/dev/null || true");
            }
            if (with_input) {
                input_poll();
            }
            if (with_fb) {
                fb_clear(&test_fb, 0x00000000);
                fb_flip(&test_fb);
            }
            sleep(1);
            printf("[%s] Headless t=%ds/%ds, estado player=%d\n", APP_NAME, i + 1, duration, player_get_state());
            fflush(stdout);
        }

        printf("[%s] Encerrando reproducao headless...\n", APP_NAME);
        player_stop();
        player_close();

        if (with_osd) {
            printf("[%s] [OSD] Retornando a grade por 2 segundos apos parada...\n", APP_NAME);
            fflush(stdout);
            s_app.active_playing_idx = -1;
            app_render(&s_app);
            sleep(2);
            fb_clear(&s_app.fb, 0x00000000);
            fb_flip(&s_app.fb);
            fb_close(&s_app.fb);
            catalog_free(&s_app.catalog);
        }
        if (with_fb) fb_close(&test_fb);
        if (with_input) input_close();
        if (with_vfd) {
            vfd_set_text("PRONTO");
            vfd_close();
        }

        printf("[%s] Modo headless concluido com sucesso.\n", APP_NAME);
        return 0;
    }

    if (app_init(&s_app) < 0) {
        fprintf(stderr, "Erro ao inicializar %s\n", APP_NAME);
        fflush(stderr);
        return 1;
    }

    /* Diagnostico temporario: abre uma URL sem depender do controle remoto. */
    if (argc == 3 && strcmp(argv[1], "--play-url") == 0) {
        if (player_play(argv[2], "Teste IPTV") < 0) {
            app_cleanup(&s_app);
            return 1;
        }
        s_app.active_playing_idx = -1;
    } else if (argc != 1) {
        fprintf(stderr, "Uso: %s [--play-url URL]\n", argv[0]);
        app_cleanup(&s_app);
        return 2;
    }

    printf("[%s] Iniciando loop de eventos nativo...\n", APP_NAME);
    system("echo MAIN_LOOP_RUNNING >> /tmp/alencar_status.txt");
    int frame_cnt = 0;
    while (s_app.running) {
        if (++frame_cnt % 30 == 0) {
            system("echo 1 > /dev/watchdog 2>/dev/null || true");
        }
        app_key_t key = input_poll();
        if (key != KEY_NONE) {
            app_handle_input(&s_app, key);
        }
        app_render(&s_app);
        usleep(30000); /* ~33 FPS para suavidade maxima com CPU baixa */
    }

    app_cleanup(&s_app);
    return 0;
}
