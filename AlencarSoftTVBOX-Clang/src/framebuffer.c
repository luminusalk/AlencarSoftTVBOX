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
    if ((ctx->bpp != 16 && ctx->bpp != 32) || !ctx->width || !ctx->height ||
        finfo.line_length < (unsigned int)((vinfo.xoffset + ctx->width) * (ctx->bpp / 8)) ||
        (size_t)finfo.smem_len < (size_t)finfo.line_length * (vinfo.yoffset + vinfo.yres)) {
        fprintf(stderr, "fb_init: Geometria de framebuffer invalida\n");
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }
    if (vinfo.xoffset != 0 || vinfo.yoffset != 0) {
        vinfo.xoffset = 0;
        vinfo.yoffset = 0;
        ioctl(ctx->fd, FBIOPAN_DISPLAY, &vinfo);
    }
    ctx->xoffset = 0;
    ctx->yoffset = 0;
    ctx->map_len = (int)finfo.smem_len;
    ctx->screensize = ctx->width * ctx->height * (ctx->bpp / 8);

    ctx->fbp = (uint8_t *)mmap(0, ctx->map_len, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
    if (ctx->fbp == MAP_FAILED) {
        perror("fb_init: Erro ao mapear memoria do framebuffer");
        close(ctx->fd);
        return -1;
    }

    /* Aloca back-buffer para desenho sem flicker */
    ctx->back_buffer = (uint8_t *)malloc(ctx->screensize);
    if (!ctx->back_buffer) {
        munmap(ctx->fbp, ctx->map_len);
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
        munmap(ctx->fbp, ctx->map_len);
        ctx->fbp = NULL;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

void fb_flip(fb_context_t *ctx) {
    if (!ctx || !ctx->fbp || !ctx->back_buffer) return;
    size_t row = (size_t)ctx->width * (ctx->bpp / 8);

    for (int y = 0; y < ctx->height; y++) {
        size_t dest = ((size_t)y + ctx->yoffset) * ctx->line_len +
                      (size_t)ctx->xoffset * (ctx->bpp / 8);
        if (dest + row > (size_t)ctx->map_len) return;

        /* No NationalChip GX6622, a controladora de video HDMI efetua a varredura
         * do OSD com inicio fixo em +2048 bytes (512 pixels @ 32bpp) de cada linha.
         * Mapeando os primeiros 768 pixels para +2048 e os ultimos 512 pixels para +0,
         * o enquadramento na TV fica 100% matematicamente perfeito em todas as colunas. */
        if (ctx->width == 1280 && ctx->line_len == 5120) {
            const size_t hw_offset = 512 * 4;       /* 2048 bytes */
            const size_t first_part = (1280 - 512) * 4; /* 3072 bytes */
            memcpy(ctx->fbp + dest + hw_offset, ctx->back_buffer + (size_t)y * row, first_part);
            memcpy(ctx->fbp + dest, ctx->back_buffer + (size_t)y * row + first_part, hw_offset);
        } else {
            memcpy(ctx->fbp + dest, ctx->back_buffer + (size_t)y * row, row);
        }
    }
}

static inline void put_pixel(fb_context_t *ctx, int x, int y, uint32_t color) {
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) return;

    if (ctx->bpp == 32) {
        uint32_t *dest = (uint32_t *)(ctx->back_buffer + ((size_t)y * ctx->width + x) * 4);
        *dest = color;
    } else if (ctx->bpp == 16) {
        /* Converte ARGB32 para RGB565 */
        uint16_t r = (color >> 19) & 0x1F;
        uint16_t g = (color >> 10) & 0x3F;
        uint16_t b = (color >> 3)  & 0x1F;
        uint16_t rgb565 = (r << 11) | (g << 5) | b;
        uint16_t *dest = (uint16_t *)(ctx->back_buffer + ((size_t)y * ctx->width + x) * 2);
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

void fb_draw_text_fit(fb_context_t *ctx, int x, int y, const char *str,
                      int max_width, uint32_t color, int scale) {
    if (!ctx || !str || scale < 1 || max_width < FONT_WIDTH * scale) return;
    int max_chars = max_width / (FONT_WIDTH * scale);
    int count = 0;
    const unsigned char *p = (const unsigned char *)str;
    while (*p && count < max_chars) {
        char glyph = '?';
        if (*p < 0x80) glyph = (char)*p++;
        else if (*p == 0xc3 && p[1]) {
            unsigned char accent = p[1];
            if (accent == 0xa7 || accent == 0x87) glyph = 'c';
            else if (accent == 0xa3 || accent == 0xa1 || accent == 0xa0 || accent == 0xa2) glyph = 'a';
            else if (accent == 0xa9 || accent == 0xaa) glyph = 'e';
            else if (accent == 0xad) glyph = 'i';
            else if (accent == 0xb3 || accent == 0xb4 || accent == 0xb5) glyph = 'o';
            else if (accent == 0xba || accent == 0xbc) glyph = 'u';
            p += 2;
        } else if (*p == 0xe2 && p[1] == 0x80 &&
                   (p[2] == 0x93 || p[2] == 0x94)) { glyph = '-'; p += 3; }
        else { p++; while ((*p & 0xc0) == 0x80) p++; }
        fb_draw_char(ctx, x + count * FONT_WIDTH * scale, y, glyph, color, scale);
        count++;
    }
    if (*p && max_chars >= 3) {
        for (int i = 0; i < 3; i++)
            fb_draw_char(ctx, x + (max_chars - 3 + i) * FONT_WIDTH * scale,
                         y, '.', color, scale);
    }
}
