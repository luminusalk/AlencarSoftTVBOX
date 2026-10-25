/**
 * @file input.c
 * @brief Leitor de eventos de controle remoto infravermelho e teclado.
 */

#include "input.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>

static int s_ir_fd = -1;
static struct termios s_orig_termios;
static int s_raw_terminal_set = 0;

int input_init(const char *ir_device) {
    /* 1. Tenta abrir o dispositivo de controle remoto infravermelho nativo */
    const char *devs[] = { ir_device, "/dev/gxirr0", "/dev/lirc0", "/dev/input/event0", NULL };
    for (int i = 0; devs[i]; i++) {
        if (!devs[i]) continue;
        s_ir_fd = open(devs[i], O_RDONLY | O_NONBLOCK);
        if (s_ir_fd >= 0) break;
    }

    /* 2. Configura stdin como modo non-blocking raw para testes e controle via console */
    if (tcgetattr(STDIN_FILENO, &s_orig_termios) == 0) {
        struct termios raw = s_orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        s_raw_terminal_set = 1;
    }
    return 0;
}

app_key_t input_poll(void) {
    /* 1. Le do controle remoto infravermelho se disponivel */
    if (s_ir_fd >= 0) {
        uint32_t ir_code = 0;
        int n = read(s_ir_fd, &ir_code, sizeof(ir_code));
        if (n > 0) {
            uint8_t cmd = (ir_code >> 8) & 0xFF;
            switch (cmd) {
                case 0x11: case 0x48: return KEY_NAV_UP;
                case 0x13: case 0x50: return KEY_NAV_DOWN;
                case 0x14: case 0x4B: return KEY_NAV_LEFT;
                case 0x15: case 0x4D: return KEY_NAV_RIGHT;
                case 0x12: case 0x1C: return KEY_ACTION_OK;
                case 0x10: case 0x01: return KEY_ACTION_BACK;
                case 0x0E: return KEY_ACTION_MENU;
                case 0x1A: return KEY_CHANNEL_UP;
                case 0x1B: return KEY_CHANNEL_DOWN;
                default: break;
            }
        }
    }

    /* 2. Le do teclado/terminal padrao (Escape sequences e teclas normais) */
    char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == 27) { /* Escape sequence */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': return KEY_NAV_UP;
                        case 'B': return KEY_NAV_DOWN;
                        case 'C': return KEY_NAV_RIGHT;
                        case 'D': return KEY_NAV_LEFT;
                    }
                }
            }
            return KEY_ACTION_BACK;
        }
        switch (ch) {
            case '\n': case '\r': return KEY_ACTION_OK;
            case 'q': case 'Q': return KEY_EXIT_APP;
            case 'm': case 'M': return KEY_ACTION_MENU;
            case 'w': case 'W': return KEY_NAV_UP;
            case 's': case 'S': return KEY_NAV_DOWN;
            case 'a': case 'A': return KEY_NAV_LEFT;
            case 'd': case 'D': return KEY_NAV_RIGHT;
            case 'b': case 'B': return KEY_ACTION_BACK;
            default: break;
        }
    }
    return KEY_NONE;
}

void input_close(void) {
    if (s_ir_fd >= 0) {
        close(s_ir_fd);
        s_ir_fd = -1;
    }
    if (s_raw_terminal_set) {
        tcsetattr(STDIN_FILENO, TCSANOW, &s_orig_termios);
        s_raw_terminal_set = 0;
    }
}
