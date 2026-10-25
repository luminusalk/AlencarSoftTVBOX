/**
 * @file input.h
 * @brief Leitor de eventos de controle remoto e teclado.
 */

#ifndef ALENCARSOFT_INPUT_H
#define ALENCARSOFT_INPUT_H

#include <stdint.h>

typedef enum {
    KEY_NONE = 0,
    KEY_NAV_UP,
    KEY_NAV_DOWN,
    KEY_NAV_LEFT,
    KEY_NAV_RIGHT,
    KEY_ACTION_OK,
    KEY_ACTION_BACK,
    KEY_ACTION_MENU,
    KEY_CHANNEL_UP,
    KEY_CHANNEL_DOWN,
    KEY_NUM_0,
    KEY_NUM_1,
    KEY_NUM_2,
    KEY_NUM_3,
    KEY_NUM_4,
    KEY_NUM_5,
    KEY_NUM_6,
    KEY_NUM_7,
    KEY_NUM_8,
    KEY_NUM_9,
    KEY_EXIT_APP
} app_key_t;

int       input_init(const char *ir_device);
app_key_t input_decode_gxirr_packet(const uint8_t packet[4]);
app_key_t input_poll(void);
void      input_close(void);

#endif /* ALENCARSOFT_INPUT_H */
