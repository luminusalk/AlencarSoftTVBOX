/**
 * @file input.c
 * @brief Leitor de eventos de controle remoto infravermelho e teclado.
 */

#include "input.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

static int s_ir_fd = -1;

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static app_key_t filter_key_debounce(app_key_t raw_key) {
    static app_key_t s_active_key = KEY_NONE;
    static uint64_t s_first_press_time = 0;
    static uint64_t s_last_repeat_time = 0;
    static uint64_t s_last_seen_time = 0;

    uint64_t now = get_time_ms();

    if (raw_key == KEY_NONE) {
        /* Se decorreram mais de 160ms sem sinal, a tecla foi liberada pelo usuario */
        if (s_active_key != KEY_NONE && (now - s_last_seen_time > 160)) {
            s_active_key = KEY_NONE;
            s_first_press_time = 0;
            s_last_repeat_time = 0;
        }
        return KEY_NONE;
    }

    s_last_seen_time = now;

    /* Nova tecla diferente da tecla anterior: dispara imediatamente sem atraso */
    if (raw_key != s_active_key) {
        s_active_key = raw_key;
        s_first_press_time = now;
        s_last_repeat_time = now;
        return raw_key;
    }

    /* Tecla repetida (sinal continuo de transmissao do controle remoto) */

    /* 1. Teclas de acao NUNCA repetem sozinhas (evita duplo clique e fechamentos involuntarios) */
    if (raw_key == KEY_ACTION_OK || raw_key == KEY_ACTION_BACK ||
        raw_key == KEY_ACTION_MENU || raw_key == KEY_EXIT_APP) {
        return KEY_NONE;
    }

    /* 2. Teclas de navegacao repetem com retardo inicial de 350ms e cadencia de 180ms */
    if (now - s_first_press_time >= 350) {
        if (now - s_last_repeat_time >= 180) {
            s_last_repeat_time = now;
            return raw_key;
        }
    }

    return KEY_NONE;
}

app_key_t input_decode_gxirr_packet(const uint8_t packet[4]) {
    if (!packet || packet[2] != 0xbf || packet[3] != 0xbf ||
        (uint8_t)(packet[0] ^ packet[1]) != 0xff) {
        return KEY_NONE;
    }

    /* Azamerica S1009+ (RCUTYPE=61): NEC command, inverse, address BF BF. */
    switch (packet[0]) {
        case 0xd0: return KEY_NAV_UP;
        case 0x70: return KEY_NAV_DOWN;
        case 0x88: return KEY_NAV_LEFT;
        case 0x08: return KEY_NAV_RIGHT;
        case 0xb0: return KEY_ACTION_OK;
        case 0xf8: return KEY_EXIT_APP;     /* Botao EXIT fisico Azamerica S1009+ */
        case 0x68: return KEY_ACTION_BACK;  /* Botao MENU / BACK */
        case 0x58: return KEY_EXIT_APP;     /* EXIT alternativo */
        case 0x48: return KEY_EXIT_APP;     /* EXIT alternativo */
        case 0x78: return KEY_EXIT_APP;     /* EXIT alternativo */
        case 0x28: return KEY_EXIT_APP;     /* EXIT alternativo */
        case 0xe8: return KEY_ACTION_BACK;  /* RECALL */
        default:
            fprintf(stderr, "[IR UNMAPPED] Azamerica cmd=0x%02x\n", packet[0]);
            return KEY_NONE;
    }
}

int input_init(const char *ir_device) {
    int stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (stdin_flags >= 0) fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
    /* 1. Tenta abrir o dispositivo de controle remoto infravermelho nativo.
     * /dev/gxirr_real tem prioridade para capturar o hardware fisico direto
     * enquanto /dev/gxirr0 e mantido como FIFO mudo para o dvbapp. */
    const char *devs[] = { "/dev/gxirr_real", ir_device, "/dev/gxirr0", "/dev/lirc0", "/dev/input/event0", NULL };
    for (int i = 0; devs[i]; i++) {
        if (!devs[i]) continue;
        s_ir_fd = open(devs[i], O_RDONLY | O_NONBLOCK);
        if (s_ir_fd >= 0) {
            fprintf(stderr, "[IR] Device opened: %s\n", devs[i]);
            uint8_t dummy[64];
            while (read(s_ir_fd, dummy, sizeof(dummy)) > 0) {}
            break;
        }
    }

    return 0;
}

app_key_t input_poll(void) {
    app_key_t raw_key = KEY_NONE;

    /* 1. Le do controle remoto infravermelho se disponivel */
    if (s_ir_fd >= 0) {
        uint32_t ir_code = 0;
        int n = read(s_ir_fd, &ir_code, sizeof(ir_code));
        if (n == (int)sizeof(ir_code)) {
            raw_key = input_decode_gxirr_packet((const uint8_t *)&ir_code);
            if (raw_key != KEY_NONE) {
                fprintf(stderr, "[IR] Azamerica command 0x%02x -> raw key %d\n",
                        (unsigned)((const uint8_t *)&ir_code)[0], raw_key);
            } else {
                uint8_t cmd = (ir_code >> 8) & 0xFF;
                switch (cmd) {
                    case 0x11: case 0x48: raw_key = KEY_NAV_UP; break;
                    case 0x13: case 0x50: raw_key = KEY_NAV_DOWN; break;
                    case 0x14: case 0x4B: raw_key = KEY_NAV_LEFT; break;
                    case 0x15: case 0x4D: raw_key = KEY_NAV_RIGHT; break;
                    case 0x12: case 0x1C: raw_key = KEY_ACTION_OK; break;
                    case 0x10: case 0x01: raw_key = KEY_ACTION_BACK; break;
                    case 0x0E: raw_key = KEY_ACTION_MENU; break;
                    case 0x1A: raw_key = KEY_CHANNEL_UP; break;
                    case 0x1B: raw_key = KEY_CHANNEL_DOWN; break;
                    default: break;
                }
            }
        }
    }

    /* 2. Le do teclado/terminal padrao (Escape sequences e teclas normais) */
    if (raw_key == KEY_NONE) {
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 27) { /* Escape sequence */
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A': raw_key = KEY_NAV_UP; break;
                            case 'B': raw_key = KEY_NAV_DOWN; break;
                            case 'C': raw_key = KEY_NAV_RIGHT; break;
                            case 'D': raw_key = KEY_NAV_LEFT; break;
                        }
                    }
                }
                if (raw_key == KEY_NONE) raw_key = KEY_ACTION_BACK;
            } else {
                switch (ch) {
                    case '\n': case '\r': raw_key = KEY_ACTION_OK; break;
                    case 'q': case 'Q': raw_key = KEY_EXIT_APP; break;
                    case 'm': case 'M': raw_key = KEY_ACTION_MENU; break;
                    case 'w': case 'W': raw_key = KEY_NAV_UP; break;
                    case 's': case 'S': raw_key = KEY_NAV_DOWN; break;
                    case 'a': case 'A': raw_key = KEY_NAV_LEFT; break;
                    case 'd': case 'D': raw_key = KEY_NAV_RIGHT; break;
                    case 'b': case 'B': raw_key = KEY_ACTION_BACK; break;
                    default: break;
                }
            }
        }
    }

    /* 3. Filtro de debounce e controle de repeticao */
    app_key_t filtered_key = filter_key_debounce(raw_key);
    if (filtered_key == KEY_ACTION_OK || filtered_key == KEY_ACTION_BACK ||
        filtered_key == KEY_ACTION_MENU || filtered_key == KEY_EXIT_APP) {
        if (s_ir_fd >= 0) {
            uint8_t dummy[64];
            while (read(s_ir_fd, dummy, sizeof(dummy)) > 0) {}
        }
    }
    return filtered_key;
}

void input_close(void) {
    if (s_ir_fd >= 0) {
        close(s_ir_fd);
        s_ir_fd = -1;
    }
}
