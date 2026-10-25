/**
 * @file framebuffer.c
 * @brief Implementacao do motor grafico direto em Linux Framebuffer (/dev/fb0).
 */

#include "framebuffer.h"
#include "font_default.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int fb_init(fb_context_t *ctx, const char *device) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));

    ctx->fd = open(device ? device : "/dev/fb0", O_RDWR);
    if (ctx->fd < 0) {
        perror("fb_init: Erro ao abrir framebuffer");
        return -1;
    }

    if (ioctl(ctx->fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(ctx->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("fb_init: Erro ao ler informacoes da tela");
        close(ctx->fd);
        return -1;
    }

    ctx->width = vinfo.xres;
    ctx->height = vinfo.yres;
    ctx->bpp = vinfo.bits_per_pixel;
    ctx->line_len = finfo.line_length;
    ctx->screensize = vinfo.yres * finfo.line_length;

    ctx->fbp = (uint8_t *)mmap(0, ctx->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
    if (ctx->fbp == MAP_FAILED) {
        perror("fb_init: Erro ao mapear memoria do framebuffer");
        close(ctx->fd);
        return -1;
    }

    /* Aloca back-buffer para desenho sem flicker */
    ctx->back_buffer = (uint8_t *)malloc(ctx->screensize);
    if (!ctx->back_buffer) {
        munmap(ctx->fbp, ctx->screensize);
        close(ctx->fd);
        return -1;
    }

    memset(ctx->back_buffer, 0, ctx->screensize);
    return 0;
}

void fb_close(fb_context_t *ctx) {
    if (!ctx) return;
    if (ctx->back_buffer) {
        free(ctx->back_buffer);
        ctx->back_buffer = NULL;
    }
    if (ctx->fbp && ctx->fbp != MAP_FAILED) {
        munmap(ctx->fbp, ctx->screensize);
        ctx->fbp = NULL;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

void fb_flip(fb_context_t *ctx) {
    if (!ctx || !ctx->fbp || !ctx->back_buffer) return;
    memcpy(ctx->fbp, ctx->back_buffer, ctx->screensize);
}

static inline void put_pixel(fb_context_t *ctx, int x, int y, uint32_t color) {
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) return;

    if (ctx->bpp == 32) {
        uint32_t *dest = (uint32_t *)(ctx->back_buffer + (y * ctx->line_len) + (x * 4));
        *dest = color;
    } else if (ctx->bpp == 16) {
        /* Converte ARGB32 para RGB565 */
        uint16_t r = (color >> 19) & 0x1F;
        uint16_t g = (color >> 10) & 0x3F;
        uint16_t b = (color >> 3)  & 0x1F;
        uint16_t rgb565 = (r << 11) | (g << 5) | b;
        uint16_t *dest = (uint16_t *)(ctx->back_buffer + (y * ctx->line_len) + (x * 2));
        *dest = rgb565;
    }
}

void fb_clear(fb_context_t *ctx, uint32_t color) {
    if (!ctx || !ctx->back_buffer) return;
    if (color == 0) {
        memset(ctx->back_buffer, 0, (size_t)ctx->screensize);
        return;
    }
    if (ctx->bpp == 32) {
        uint32_t *p = (uint32_t *)ctx->back_buffer;
        size_t total = (size_t)ctx->width * ctx->height;
        for (size_t i = 0; i < total; i++) p[i] = color;
        return;
    }
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            put_pixel(ctx, x, y, color);
        }
    }
}

void fb_draw_rect(fb_context_t *ctx, int x, int y, int w, int h, uint32_t color) {
    if (!ctx || !ctx->back_buffer) return;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            put_pixel(ctx, i, j, color);
        }
    }
}

void fb_draw_rect_outline(fb_context_t *ctx, int x, int y, int w, int h, int thickness, uint32_t color) {
    if (!ctx || !ctx->back_buffer) return;
    /* Linha superior e inferior */
    fb_draw_rect(ctx, x, y, w, thickness, color);
    fb_draw_rect(ctx, x, y + h - thickness, w, thickness, color);
    /* Linha esquerda e direita */
    fb_draw_rect(ctx, x, y, thickness, h, color);
    fb_draw_rect(ctx, x + w - thickness, y, thickness, h, color);
}

void fb_draw_char(fb_context_t *ctx, int x, int y, char c, uint32_t color, int scale) {
    if (c < 32 || c > 126) c = '?';
    int char_idx = c - 32;
    if (scale < 1) scale = 1;

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = font_8x16[char_idx][row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1) {
                    put_pixel(ctx, x + col, y + row, color);
                } else {
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            put_pixel(ctx, x + col * scale + dx, y + row * scale + dy, color);
                        }
                    }
                }
            }
        }
    }
}

void fb_draw_text(fb_context_t *ctx, int x, int y, const char *str, uint32_t color, int scale) {
    if (!ctx || !str) return;
    int cur_x = x;
    int char_width = FONT_WIDTH * scale;

    while (*str) {
        if (*str == '\n') {
            y += FONT_HEIGHT * scale + 4;
            cur_x = x;
        } else {
            fb_draw_char(ctx, cur_x, y, *str, color, scale);
            cur_x += char_width;
        }
        str++;
    }
}
