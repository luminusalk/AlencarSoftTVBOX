/**
 * @file player.h
 * @brief Interface com o decodificador de hardware de video/audio.
 */

#ifndef ALENCARSOFT_PLAYER_H
#define ALENCARSOFT_PLAYER_H

typedef enum {
    PLAYER_STATE_STOPPED = 0,
    PLAYER_STATE_BUFFERING,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_ERROR
} player_state_t;

int            player_init(void);
int            player_play(const char *url, const char *channel_name);
void           player_stop(void);
player_state_t player_get_state(void);
/* Cached state for rendering and remote navigation; does not perform SOAP I/O. */
player_state_t player_peek_state(void);
void           player_close(void);

#endif /* ALENCARSOFT_PLAYER_H */
