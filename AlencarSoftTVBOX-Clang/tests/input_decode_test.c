#include "input.h"
#include <assert.h>
#include <stdint.h>

struct key_case {
    uint8_t command;
    app_key_t expected;
};

int main(void) {
    static const struct key_case cases[] = {
        {0xd0, KEY_NAV_UP},
        {0x70, KEY_NAV_DOWN},
        {0x88, KEY_NAV_LEFT},
        {0x08, KEY_NAV_RIGHT},
        {0xb0, KEY_ACTION_OK},
        {0x6a, KEY_ACTION_SAT},
        {0x68, KEY_ACTION_BACK}
    };
    unsigned int i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t packet[4] = {cases[i].command,
                             (uint8_t)~cases[i].command, 0xbf, 0xbf};
        assert(input_decode_gxirr_packet(packet) == cases[i].expected);
    }

    {
        const uint8_t invalid_address[4] = {0xd0, 0x2f, 0x00, 0x00};
        const uint8_t invalid_inverse[4] = {0xd0, 0xd0, 0xbf, 0xbf};
        const uint8_t unknown[4] = {0x55, 0xaa, 0xbf, 0xbf};
        assert(input_decode_gxirr_packet(invalid_address) == KEY_NONE);
        assert(input_decode_gxirr_packet(invalid_inverse) == KEY_NONE);
        assert(input_decode_gxirr_packet(unknown) == KEY_NONE);
    }
    return 0;
}
