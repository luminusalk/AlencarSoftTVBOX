/**
 * @file app.h
 * @brief Maquina de estados e definicoes centrais do AlencarSoftTVBOX-Clang.
 */

#ifndef ALENCARSOFT_APP_H
#define ALENCARSOFT_APP_H

#include "config.h"
#include "platform.h"
#include "framebuffer.h"
#include "iptv_catalog.h"
#include "input.h"
#include "display_vfd.h"
#include "player.h"

typedef enum {
    VIEW_CATEGORIES = 0, /* Grade de categorias */
    VIEW_CHANNEL_LIST,   /* Grade/lista de canais da categoria selecionada */
    VIEW_PLAYING,        /* Canal em tela cheia */
    VIEW_UPDATING_CATALOG, /* Tela de progresso da atualizacao */
    VIEW_CONFIRM_EXIT    /* Dialogo para sair do app */
} app_view_t;

typedef struct {
    int running;
    app_view_t view;
    
    platform_info_t platform;
    fb_context_t fb;
    iptv_catalog_t catalog;
    
    /* Selecao na grade */
    int current_category_idx;
    int current_channel_idx;
    int scroll_offset;
    int channels_per_page;
    
    /* Canal em reproducao */
    int active_playing_idx;
    int osd_visible;
    uint32_t osd_timer_ms;
    
    /* Status de mensagem */
    char status_message[128];
    uint32_t status_timer_ms;

    /* Dialogo de saida (0 = SIM, 1 = NAO) */
    int exit_dialog_choice;
} app_context_t;

/* Ciclo de vida da aplicacao */
int  app_init(app_context_t *app);
void app_run(app_context_t *app);
void app_render(app_context_t *app);
void app_handle_input(app_context_t *app, app_key_t key);
void app_update_catalog_from_web(app_context_t *app);
void app_cleanup(app_context_t *app);

#endif /* ALENCARSOFT_APP_H */
