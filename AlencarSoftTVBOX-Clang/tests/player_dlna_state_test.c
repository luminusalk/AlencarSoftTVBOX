#include "player.h"

#include <time.h>

static time_t fake_now = 1000;
static int renderer_active = 1;
static int renderer_playing = 1;
static int stream_started = 0;
static int stream_disconnected = 0;
static int stream_progress_recent = 0;

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
int firmware_dlna_stream_started(void) { return stream_started; }
int firmware_dlna_stream_disconnected(void) { return stream_disconnected; }
int firmware_dlna_stream_progress_recent(int seconds) {
    (void)seconds;
    return stream_progress_recent;
}

int main(void) {
    player_init();
    stream_started = 0;
    if (player_play("https://example.invalid/live.m3u8", "Canal") != 0) return 1;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 2;
    renderer_playing = 0;
    fake_now = 1002;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 3;
    fake_now = 1003;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 4;
    renderer_playing = 1;
    stream_started = 1;
    fake_now = 1004;
    /* The first confirmed local HTTP playback must clear the loading OSD.
     * Keeping it for a second SOAP poll hides real video and causes flicker. */
    if (player_get_state() != PLAYER_STATE_PLAYING) return 8;
    fake_now = 1005;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 9;
    renderer_playing = 0;
    fake_now = 1006;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 5;
    stream_progress_recent = 1;
    fake_now = 1007;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 6;
    stream_progress_recent = 0;
    fake_now = 1008;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 19;
    fake_now = 1009;
    if (player_get_state() != PLAYER_STATE_ERROR) return 20;
    player_stop();
    if (player_get_state() != PLAYER_STATE_STOPPED) return 7;
    renderer_playing = 0;
    fake_now = 2000;
    if (player_play("http://example.invalid/video.ts", "Canal") != 0) return 10;
    for (fake_now = 2002; fake_now < 2012; fake_now++)
        if (player_get_state() != PLAYER_STATE_BUFFERING) return 11;
    fake_now = 2012;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 12;
    fake_now = 2013;
    if (player_get_state() != PLAYER_STATE_ERROR) return 13;
    player_stop();
    renderer_playing = 1;
    stream_started = 0;
    stream_disconnected = 0;
    fake_now = 3000;
    if (player_play("https://example.invalid/live.m3u8", "Canal") != 0) return 14;
    fake_now = 3002;
    if (player_get_state() != PLAYER_STATE_BUFFERING) return 15;
    stream_started = 1;
    fake_now = 3003;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 16;
    fake_now = 3004;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 17;
    stream_disconnected = 1;
    fake_now = 3005;
    if (player_get_state() != PLAYER_STATE_ERROR) return 18;
    player_stop();
    stream_disconnected = 0;
    stream_progress_recent = 1;
    fake_now = 4000;
    if (player_play("https://example.invalid/live.m3u8", "Canal") != 0) return 21;
    fake_now = 4002;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 22;
    fake_now = 4003;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 23;
    renderer_playing = 0;
    for (fake_now = 4004; fake_now < 4009; fake_now++)
        if (player_get_state() != PLAYER_STATE_PLAYING) return 24;
    fake_now = 4009;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 26;
    fake_now = 4010;
    if (player_get_state() != PLAYER_STATE_ERROR) return 25;
    player_stop();

    /* The firmware may already be rendering while the loading OSD remains.
     * A fresh local HTTP stream plus PLAYING should uncover it immediately. */
    renderer_playing = 1;
    stream_started = 1;
    stream_disconnected = 0;
    fake_now = 5000;
    if (player_play("https://example.invalid/live.m3u8", "Canal") != 0) return 27;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 28;
    player_stop();

    /* Direct HTTP has no local byte counter; one confirmed PLAYING report
     * after launch is sufficient to remove the stale warning. */
    fake_now = 6000;
    if (player_play("http://example.invalid/video.ts", "Canal") != 0) return 29;
    fake_now = 6001;
    if (player_get_state() != PLAYER_STATE_PLAYING) return 30;
    return 0;
}
