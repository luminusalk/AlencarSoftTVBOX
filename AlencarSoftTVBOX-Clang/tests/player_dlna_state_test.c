#include "player.h"

#include <time.h>

static time_t fake_now = 1000;
static int renderer_active = 1;
static int renderer_playing = 1;

time_t time(time_t *out) {
    if (out) *out = fake_now;
    return fake_now;
}

int firmware_dlna_start(const char *source, const char *title) {
    (void)source;
    (void)title;
    renderer_active = 1;
    return 0;
}
void firmware_dlna_stop(void) { renderer_active = 0; }
int firmware_dlna_active(void) { return renderer_active; }
int firmware_dlna_transport_playing(void) { return renderer_playing; }

int main(void) {
    player_init();
    if (player_play("http://example.invalid/video.ts", "Canal") != 0) return 1;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 2;
    fake_now = 1002;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 3;
    fake_now = 1003;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 4;
    renderer_playing = 0;
    fake_now = 1004;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 5;
    fake_now = 1005;
    if (player_get_state() != PLAYER_STATE_ERROR) return 6;
    player_stop();
    if (player_get_state() != PLAYER_STATE_STOPPED) return 7;
    return 0;
}
