/**
 * @file main.c
 * @brief Aplicativo nativo C para TV Box e Receptores Satelite (AlencarSoftTVBOX-Clang).
 */

#include "app.h"
#include "channel_history.h"
#ifdef RECEIVER_DLNA_BACKEND
#include "ir_guard.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

static app_context_t s_app;

static int app_select_channel(app_context_t *app, int index) {
    if (index < 0 || index >= app->catalog.total_channels) return -1;
    for (int c = 0; c < app->catalog.total_categories; c++) {
        iptv_category_t *cat = &app->catalog.categories[c];
        if (strcmp(cat->name, "Todos os Canais") == 0) continue;
        for (int row = 0; row < cat->count; row++) {
            if (cat->channel_indices[row] == index) {
                app->current_category_idx = c;
                app->current_channel_idx = row;
                app->scroll_offset = row >= app->channels_per_page ? row - app->channels_per_page + 1 : 0;
                return 0;
            }
        }
    }
    return -1;
}

static int app_start_channel(app_context_t *app, int index) {
    if (index < 0 || index >= app->catalog.total_channels) return -1;
    iptv_channel_t *ch = &app->catalog.channels[index];
    if (player_play(ch->url, ch->name) < 0) {
        app->active_playing_idx = -1;
        app->view = VIEW_CATEGORIES;
        snprintf(app->status_message, sizeof(app->status_message), "Canal indisponivel: %.90s", ch->name);
        vfd_set_text("FALHA");
        return -1;
    }
    app->active_playing_idx = index;
    app->view = VIEW_PLAYING;
    app_select_channel(app, index);
    vfd_set_text(ch->name);
    if (channel_history_save(ch, CHANNEL_HISTORY_PATH) < 0)
        fprintf(stderr, "[Historico] Nao foi possivel salvar ultimo canal.\n");
    return 0;
}

static int app_start_last_channel(app_context_t *app) {
    int index = channel_history_find(&app->catalog, CHANNEL_HISTORY_PATH);
    if (index < 0) {
        for (int i = 0; i < app->catalog.total_channels; i++) {
            if (strstr(app->catalog.channels[i].name, "TV Cultura")) { index = i; break; }
        }
    }
    if (index < 0 && app->catalog.total_channels > 0) index = 0;
    return app_start_channel(app, index);
}

static void sig_handler(int sig) {
    (void)sig;
    s_app.running = 0;
}

int app_init(app_context_t *app) {
    system("echo APP_INIT_START > /tmp/alencar_status.txt");
    memset(app, 0, sizeof(*app));
    app->running = 1;
    app->view = VIEW_CATEGORIES;
    app->active_playing_idx = -1;
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
#ifdef RECEIVER_DLNA_BACKEND
    if (ir_guard_start() < 0) {
        fprintf(stderr, "[IR] Nao foi possivel obter controle exclusivo.\n");
        return -1;
    }
#endif
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
    static int last_channel = -2;
    static app_view_t last_view = (app_view_t)-1;
    static time_t play_start_time = 0;
    if (!app->fb.back_buffer) return;

    playback_state = player_get_state();
    if (app->view == VIEW_PLAYING && (playback_state == PLAYER_STATE_PLAYING || playback_state == PLAYER_STATE_BUFFERING)) {
        if (last_channel != app->active_playing_idx || last_view != app->view) {
            play_start_time = time(NULL);
            last_channel = app->active_playing_idx;
            last_view = app->view;
        }

        /* Modo Reproducao: Framebuffer 100% transparente para permitir que o
         * plano de hardware de video subjacente seja exibido diretamente na TV */
        fb_clear(&app->fb, 0x00000000);

        /* Barra de informacoes inferior sobreposta com identificacao do canal por 5s */
        time_t now = time(NULL);
        if (now - play_start_time <= 5 || playback_state == PLAYER_STATE_BUFFERING) {
            if (app->active_playing_idx >= 0 && app->active_playing_idx < app->catalog.total_channels) {
                iptv_channel_t *ch = &app->catalog.channels[app->active_playing_idx];
                int inset = app->fb.width / 32;
                fb_draw_rect(&app->fb, inset, app->fb.height - 76, app->fb.width - 2 * inset, 52, COLOR_PANEL_BG);
                fb_draw_rect(&app->fb, inset, app->fb.height - 76, 5, 52, COLOR_ACCENT);
                fb_draw_text_fit(&app->fb, inset + 18, app->fb.height - 59, ch->name,
                                 app->fb.width - 2 * inset - 36, COLOR_WHITE, 1);
                if (playback_state == PLAYER_STATE_BUFFERING)
                    fb_draw_text(&app->fb, 60, app->fb.height - 105, "Aguardando dados do canal...", COLOR_WHITE, 1);
            }
        }

        fb_flip(&app->fb);
        return;
    }

    last_channel = -2;
    last_view = app->view;

    int width = app->fb.width, height = app->fb.height;
    int margin = width / 40;
    int header_h = height >= 720 ? 76 : 58;
    int footer_h = height >= 720 ? 54 : 42;
    int sidebar_w = width / 4;
    if (sidebar_w < 145) sidebar_w = 145;
    if (sidebar_w > 280) sidebar_w = 280;
    if (sidebar_w > width / 2) sidebar_w = width / 2;
    int list_x = sidebar_w + margin;
    int list_w = width - list_x - margin;
    int list_y = header_h + 58;
    int row_h = height >= 720 ? 54 : 43;
    int visible = (height - footer_h - list_y - 16) / row_h;
    if (visible < 1) visible = 1;
    if (visible > OSD_MAX_CHANNELS_PER_PAGE) visible = OSD_MAX_CHANNELS_PER_PAGE;
    app->channels_per_page = visible;
    if (app->current_channel_idx < app->scroll_offset) app->scroll_offset = app->current_channel_idx;
    if (app->current_channel_idx >= app->scroll_offset + visible)
        app->scroll_offset = app->current_channel_idx - visible + 1;

    fb_clear(&app->fb, COLOR_DARK_BG);
    fb_draw_rect(&app->fb, 0, 0, width, header_h, COLOR_PANEL_BG);
    fb_draw_rect(&app->fb, 0, header_h - 3, width, 3, COLOR_ACCENT);
    fb_draw_text_fit(&app->fb, margin, header_h / 2 - 14, "ALENCARSOFT TVBOX",
                     width - 115 - 2 * margin, COLOR_WHITE, 2);
    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    char clock_str[16] = "--:--";
    if (timeinfo) snprintf(clock_str, sizeof(clock_str), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    fb_draw_text(&app->fb, width - 100, header_h / 2 - 10, clock_str, COLOR_ACCENT, 1);

    /* Categorias em coluna: todas acessiveis, inclusive quando a tela e 576p. */
    fb_draw_rect(&app->fb, 0, header_h, sidebar_w, height - header_h - footer_h, COLOR_PANEL_BG);
    fb_draw_text_fit(&app->fb, margin, header_h + 20, "CATEGORIAS",
                     sidebar_w - 2 * margin, COLOR_ACCENT, 1);
    int cat_y = header_h + 55;
    int cat_row_h = height >= 720 ? 43 : 34;
    int cat_visible = (height - footer_h - cat_y - 8) / cat_row_h;
    if (cat_visible < 1) cat_visible = 1;
    int cat_start = 0;
    if (app->current_category_idx >= cat_visible)
        cat_start = app->current_category_idx - cat_visible + 1;
    for (int i = cat_start; i < app->catalog.total_categories && i < cat_start + cat_visible; i++) {
        int y = cat_y + (i - cat_start) * cat_row_h;
        int selected = i == app->current_category_idx;
        if (selected) {
            fb_draw_rect(&app->fb, 0, y, sidebar_w, cat_row_h - 2, COLOR_HIGHLIGHT);
            fb_draw_rect(&app->fb, 0, y, 5, cat_row_h - 2, COLOR_ACCENT);
        }
        fb_draw_text_fit(&app->fb, margin, y + (cat_row_h - 16) / 2,
                         app->catalog.categories[i].name, sidebar_w - 2 * margin,
                         selected ? COLOR_WHITE : COLOR_TEXT_MUTED, 1);
    }

    if (app->current_category_idx < app->catalog.total_categories) {
        iptv_category_t *cat = &app->catalog.categories[app->current_category_idx];
        char subtitle[72];
        snprintf(subtitle, sizeof(subtitle), "%d canais", cat->count);
        fb_draw_text_fit(&app->fb, list_x, header_h + 16, cat->name,
                         list_w - 100, COLOR_WHITE, 1);
        fb_draw_text_fit(&app->fb, list_x, header_h + 34, subtitle,
                         list_w, COLOR_TEXT_MUTED, 1);
        for (int i = 0; i < visible && app->scroll_offset + i < cat->count; i++) {
            int row = app->scroll_offset + i;
            int ch_idx = cat->channel_indices[row];
            iptv_channel_t *ch = &app->catalog.channels[ch_idx];
            int y = list_y + i * row_h;
            int selected = row == app->current_channel_idx;
            fb_draw_rect(&app->fb, list_x, y, list_w, row_h - 5,
                         selected ? COLOR_HIGHLIGHT : COLOR_PANEL_BG);
            if (selected) fb_draw_rect(&app->fb, list_x, y, 5, row_h - 5, COLOR_ACCENT);
            char number[12];
            snprintf(number, sizeof(number), "%03d", row + 1);
            fb_draw_text(&app->fb, list_x + 14, y + (row_h - 16) / 2, number, COLOR_ACCENT, 1);
            int badge_w = ch->is_fullhd ? 62 : (ch->is_hd ? 42 : 0);
            int name_x = list_x + 52;
            int name_w = list_w - 62 - badge_w - 10;
            fb_draw_text_fit(&app->fb, name_x, y + (row_h - 16) / 2, ch->name,
                             name_w, COLOR_WHITE, 1);
            if (badge_w > 0) {
                int badge_x = list_x + list_w - badge_w - 12;
                fb_draw_rect(&app->fb, badge_x, y + (row_h - 24) / 2,
                             badge_w, 24, ch->is_fullhd ? COLOR_RED_BADGE : COLOR_GREEN_LIVE);
                fb_draw_text(&app->fb, badge_x + 6, y + (row_h - 16) / 2,
                             ch->is_fullhd ? "1080p" : "HD", COLOR_BLACK, 1);
            }
        }
    }

    fb_draw_rect(&app->fb, 0, height - footer_h, width, footer_h, COLOR_PANEL_BG);
    fb_draw_text_fit(&app->fb, margin, height - footer_h + (footer_h - 16) / 2,
                     "OK Assistir    ESQ/DIR Categoria    CIMA/BAIXO Canal    EXIT Voltar",
                     width - 2 * margin, COLOR_TEXT_MUTED, 1);
    if (app->status_message[0] && playback_state == PLAYER_STATE_ERROR)
        fb_draw_text_fit(&app->fb, list_x, height - footer_h - 18,
                         app->status_message, list_w, COLOR_WHITE, 1);

    /* Dialogo modal de confirmacao de saida para a TV Box original */
    if (app->view == VIEW_CONFIRM_EXIT) {
        int dw = width * 3 / 5;
        if (dw > 520) dw = 520;
        int dh = height / 3;
        if (dh > 200) dh = 200;
        int dx = (app->fb.width - dw) / 2;
        int dy = (app->fb.height - dh) / 2;

        fb_draw_rect(&app->fb, dx - 4, dy - 4, dw + 8, dh + 8, COLOR_HIGHLIGHT);
        fb_draw_rect(&app->fb, dx, dy, dw, dh, COLOR_PANEL_BG);

        fb_draw_text_fit(&app->fb, dx + 20, dy + 28,
                         "Sair para o sistema original?", dw - 40, COLOR_WHITE, 1);

        int btn_w = 140;
        int btn_h = 45;
        int btn_y = dy + dh - btn_h - 20;
        if (btn_w * 2 + 40 > dw) btn_w = (dw - 60) / 2;
        int sim_x = dx + 20;
        int nao_x = dx + dw - btn_w - 20;

        uint32_t sim_bg = (app->exit_dialog_choice == 0) ? COLOR_HIGHLIGHT : COLOR_DARK_BG;
        uint32_t nao_bg = (app->exit_dialog_choice == 1) ? COLOR_HIGHLIGHT : COLOR_DARK_BG;

        fb_draw_rect(&app->fb, sim_x, btn_y, btn_w, btn_h, sim_bg);
        fb_draw_text(&app->fb, sim_x + 18, btn_y + 14, "SIM", COLOR_WHITE, 1);

        fb_draw_rect(&app->fb, nao_x, btn_y, btn_w, btn_h, nao_bg);
        fb_draw_text(&app->fb, nao_x + 18, btn_y + 14, "NAO", COLOR_WHITE, 1);
    }

    /* Envia o buffer pronto para a tela */
    fb_flip(&app->fb);
}

void app_handle_input(app_context_t *app, app_key_t key) {
    if (key == KEY_NONE) return;

    /* 1. Tratamento durante a reproducao de video em tela cheia */
    if (app->view == VIEW_PLAYING) {
        if (key == KEY_ACTION_BACK || key == KEY_EXIT_APP || key == KEY_ACTION_MENU || key == KEY_ACTION_SAT) {
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

                app_start_channel(app, ch_idx);
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

        case KEY_ACTION_SAT:
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
#ifdef RECEIVER_DLNA_BACKEND
    atexit(ir_guard_stop);
#endif

    system("echo MAIN_ENTERED > /tmp/alencar_status.txt");
    printf("\n=========================================\n");
    printf("   Iniciando %s v%s\n", APP_NAME, APP_VERSION);
    printf("=========================================\n");
    fflush(stdout);

    /* Modo C isolado (headless): testa o transporte DLNA e player C, com flags para isolar componentes */
    /* Modo Daemon SAT / EXIT 3x: escuta controle remoto e dispara o app ao apertar SAT ou EXIT 3 vezes */
    if (argc >= 2 && (strcmp(argv[1], "--exit3-daemon") == 0 || strcmp(argv[1], "--sat-daemon") == 0)) {
#ifdef RECEIVER_DLNA_BACKEND
        /* O gxirr entrega cada tecla a apenas um leitor. Sem o guard, o
         * key_task do firmware pode consumir SAT antes deste processo. */
        if (ir_guard_start() < 0) {
            fprintf(stderr, "[Daemon] Nao foi possivel reservar o controle remoto.\n");
            return 1;
        }
#endif
        input_init(DEFAULT_IR_DEV);
        printf("[%s] Daemon SAT / EXIT 3x ativo em segundo plano (PID %d).\n", APP_NAME, getpid());
        fflush(stdout);

        pid_t app_child_pid = -1;
        int exit_count = 0;
        time_t last_exit_time = 0;

        while (1) {
            if (app_child_pid > 0) {
                int status = 0;
                pid_t res = waitpid(app_child_pid, &status, WNOHANG);
                if (res == app_child_pid || (res == -1 && errno == ECHILD)) {
                    printf("[%s] [Daemon] App IPTV encerrado. Reativando escuta do controle...\n", APP_NAME);
                    fflush(stdout);
                    app_child_pid = -1;
#ifdef RECEIVER_DLNA_BACKEND
                    if (ir_guard_start() < 0) {
                        fprintf(stderr, "[Daemon] Nao foi possivel retomar o controle remoto.\n");
                        return 1;
                    }
#endif
                    input_init(DEFAULT_IR_DEV);
                } else {
                    /* Enquanto o app estiver ativo na tela, o daemon dorme para nao disputar /dev/gxirr0 */
                    usleep(200000);
                    continue;
                }
            }

#ifdef RECEIVER_DLNA_BACKEND
            if (!ir_guard_active()) {
                fprintf(stderr, "[Daemon] Protecao do controle remoto perdida.\n");
                return 1;
            }
#endif
            app_key_t key = input_poll();
            int should_launch = 0;

            if (key == KEY_ACTION_SAT) {
                printf("[%s] [Daemon] BOTAO SAT PRESSIONADO! Abrindo AlencarSoft TVBOX...\n", APP_NAME);
                fflush(stdout);
                should_launch = 1;
            } else if (key == KEY_EXIT_APP && strcmp(argv[1], "--exit3-daemon") == 0) {
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
                    should_launch = 1;
                }
            }

            if (should_launch) {
                /* Entrega o IR ao app; o filho assume seu proprio guard. */
                input_close();
#ifdef RECEIVER_DLNA_BACKEND
                ir_guard_stop();
#endif

                pid_t pid = fork();
                if (pid == 0) {
                    int log_fd = open("/tmp/alencar_app.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (log_fd >= 0) {
                        dup2(log_fd, STDOUT_FILENO);
                        dup2(log_fd, STDERR_FILENO);
                        close(log_fd);
                    }
                    int devnull = open("/dev/null", O_RDONLY);
                    if (devnull >= 0) {
                        dup2(devnull, STDIN_FILENO);
                        close(devnull);
                    }
                    char *args[] = {
                        "/mnt/plugin/alencarsoft_tvbox",
                        "--headless-osd-ir",
                        NULL,
                        NULL,
                        NULL
                    };
                    execv(args[0], args);
                    _exit(1);
                } else if (pid > 0) {
                    app_child_pid = pid;
                    printf("[%s] [Daemon] Processo IPTV iniciado com PID %d\n", APP_NAME, (int)pid);
                    fflush(stdout);
                } else {
                    fprintf(stderr, "[%s] [Daemon] Erro no fork: %s\n", APP_NAME, strerror(errno));
#ifdef RECEIVER_DLNA_BACKEND
                    if (ir_guard_start() < 0) return 1;
#endif
                    input_init(DEFAULT_IR_DEV);
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
            signal(SIGINT, sig_handler);
            signal(SIGTERM, sig_handler);
            signal(SIGHUP, SIG_IGN);
            if (argc < 4) duration = 0;
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
            fb_clear(&s_app.fb, 0);
            fb_flip(&s_app.fb);

#ifdef RECEIVER_DLNA_BACKEND
            if (ir_guard_start() < 0) {
                fprintf(stderr, "[IR] Falha no controle exclusivo; retornando ao firmware.\n");
                fb_clear(&s_app.fb, 0); fb_flip(&s_app.fb); fb_close(&s_app.fb);
                return 1;
            }
#endif
            input_init(DEFAULT_IR_DEV);
            player_init();

            /* A primeira imagem do OSD e o canal, sem mostrar a grade durante
             * a transicao do firmware para DLNA. A grade so abre por MENU/EXIT. */
            app_start_last_channel(&s_app);

            printf("[%s] [OSD+IR] Grade carregada: %d canais, %d categorias.\n",
                   APP_NAME, s_app.catalog.total_channels, s_app.catalog.total_categories);
            if (s_app.catalog.total_categories > 0)
                printf("[%s] [OSD+IR] Categoria ativa: '%s'\n", APP_NAME, s_app.catalog.categories[s_app.current_category_idx].name);
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
#ifdef RECEIVER_DLNA_BACKEND
                if (!ir_guard_active()) {
                    fprintf(stderr, "[IR] Protecao perdida; encerrando interface.\n");
                    break;
                }
#endif
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
    if (argc == 1) {
        app_start_last_channel(&s_app);
    } else if (argc == 3 && strcmp(argv[1], "--play-url") == 0) {
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
    time_t last_render_sec = 0;
    int needs_render = 1;
    while (s_app.running) {
#ifdef RECEIVER_DLNA_BACKEND
        if (!ir_guard_active()) {
            fprintf(stderr, "[IR] Protecao perdida; encerrando interface.\n");
            break;
        }
#endif
        if (++frame_cnt % 30 == 0) {
            system("echo 1 > /dev/watchdog 2>/dev/null || true");
        }
        app_key_t key = input_poll();
        if (key != KEY_NONE) {
            app_handle_input(&s_app, key);
            needs_render = 1;
        }
        time_t now = time(NULL);
        if (now != last_render_sec) {
            last_render_sec = now;
            if (s_app.view == VIEW_CATEGORIES || s_app.view == VIEW_PLAYING)
                needs_render = 1;
        }
        if (needs_render) {
            app_render(&s_app);
            needs_render = 0;
        }
        usleep(30000);
    }

    app_cleanup(&s_app);
    return 0;
}
