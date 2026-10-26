#include "framebuffer.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    uint8_t mapped[64], back[16];
    uint32_t pixel;
    fb_context_t fb;
    memset(&fb, 0, sizeof(fb));
    memset(mapped, 0xaa, sizeof(mapped));
    memset(back, 0, sizeof(back));
    fb.fbp = mapped;
    fb.back_buffer = back;
    fb.width = fb.height = 2;
    fb.bpp = 32;
    fb.line_len = 16;
    fb.screensize = sizeof(back);
    fb.map_len = sizeof(mapped);
    fb.xoffset = fb.yoffset = 1;

    fb_clear(&fb, 0xff123456);
    fb_flip(&fb);
    for (int i = 0; i < 4; ++i) {
        memcpy(&pixel, back + i * 4, 4);
        assert(pixel == 0xff123456);
    }
    /* Visible pixels are at row 1+2, column 1+2; padding stays untouched. */
    assert(mapped[0] == 0xaa);
    memcpy(&pixel, mapped + 20, 4); assert(pixel == 0xff123456);
    memcpy(&pixel, mapped + 24, 4); assert(pixel == 0xff123456);
    assert(mapped[28] == 0xaa);
    memcpy(&pixel, mapped + 36, 4); assert(pixel == 0xff123456);
    memcpy(&pixel, mapped + 40, 4); assert(pixel == 0xff123456);
    assert(mapped[44] == 0xaa);
    return 0;
}
