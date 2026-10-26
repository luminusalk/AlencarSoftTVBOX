/**
 * @file framebuffer.h
 * @brief Motor grafico direto em Linux Framebuffer (/dev/fb0).
 */

#ifndef ALENCARSOFT_FRAMEBUFFER_H
#define ALENCARSOFT_FRAMEBUFFER_H

#include <stdint.h>

/* Cores em formato ARGB32 */
#define COLOR_BLACK       0xFF000000
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_DARK_BG     0xFF12161E
#define COLOR_PANEL_BG    0xFF1B2230
#define COLOR_ACCENT      0xFF00B4D8  /* Azul claro suave */
#define COLOR_HIGHLIGHT   0xFF0077B6  /* Destaque de selecao */
#define COLOR_TEXT_MUTED  0xFFA0AAB8
#define COLOR_GREEN_LIVE  0xFF2EC4B6
#define COLOR_RED_BADGE   0xFFE63946

typedef struct {
    int      fd;
    uint8_t *fbp;
    uint8_t *back_buffer;
    int      width;
    int      height;
    int      bpp;
    int      line_len;
    int      screensize;
    int      map_len;
    int      xoffset;
    int      yoffset;
} fb_context_t;

/* Inicializacao e encerramento */
int  fb_init(fb_context_t *ctx, const char *device);
void fb_close(fb_context_t *ctx);
void fb_flip(fb_context_t *ctx);

/* Primitivas graficas */
void fb_clear(fb_context_t *ctx, uint32_t color);
void fb_draw_rect(fb_context_t *ctx, int x, int y, int w, int h, uint32_t color);
void fb_draw_rect_outline(fb_context_t *ctx, int x, int y, int w, int h, int thickness, uint32_t color);

/* Renderizacao de texto */
void fb_draw_char(fb_context_t *ctx, int x, int y, char c, uint32_t color, int scale);
void fb_draw_text(fb_context_t *ctx, int x, int y, const char *str, uint32_t color, int scale);
void fb_draw_text_fit(fb_context_t *ctx, int x, int y, const char *str,
                      int max_width, uint32_t color, int scale);

#endif /* ALENCARSOFT_FRAMEBUFFER_H */
