/**
 * @file player.c
 * @brief Gerenciador de reproducao de audio e video acelerado por hardware.
 */

#include "player.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static player_state_t s_player_state = PLAYER_STATE_STOPPED;
static char s_current_url[512] = {0};

int player_init(void) {
    s_player_state = PLAYER_STATE_STOPPED;
    s_current_url[0] = '\0';
    return 0;
}

int player_play(const char *url, const char *channel_name) {
    (void)channel_name;
    if (!url) return -1;
    
    /* Para qualquer reproducao anterior */
    player_stop();

    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_player_state = PLAYER_STATE_PLAYING;

    /* No GX6622 / Nationalchip, o showcartoon reproduz streams com aceleracao por hardware */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "showcartoon -p \"%s\" 100 >/dev/null 2>&1 &", url);
    system(cmd);

    return 0;
}

void player_stop(void) {
    if (s_player_state != PLAYER_STATE_STOPPED) {
        system("killall -9 showcartoon 2>/dev/null");
        s_player_state = PLAYER_STATE_STOPPED;
        s_current_url[0] = '\0';
    }
}

player_state_t player_get_state(void) {
    return s_player_state;
}

void player_close(void) {
    player_stop();
}
