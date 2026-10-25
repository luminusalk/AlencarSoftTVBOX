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
#include <stdint.h>
#include <sys/ioctl.h>

#define GXAV_IOCTL_FIFO_UNLINK 0x1407

struct gxav_fifo_link {
    uint32_t module_id;
    void    *channel;
    uint32_t pin_id;
    uint32_t dir;
};

static player_state_t s_player_state = PLAYER_STATE_STOPPED;
static char s_current_url[512] = {0};

static void gxav_cleanup_audio(void) {
    int fd = open("/dev/gxav0", O_RDWR);
    if (fd < 0) return;

    struct gxav_fifo_link req;
    /* Desvincula canais presos nos modulos de hardware GXAV:
     * demux=1, vdec=2, adec=3, vpu=4, aout=5, vout=6
     * Garante que module->in_channel[1] fique livre para o decodificador de audio */
    for (uint32_t mod = 1; mod <= 6; mod++) {
        for (uint32_t pin = 0; pin < 8; pin++) {
            memset(&req, 0, sizeof(req));
            req.module_id = mod;
            req.pin_id = pin;
            req.dir = 1; /* GXAV_PIN_INPUT */
            ioctl(fd, GXAV_IOCTL_FIFO_UNLINK, &req);

            req.dir = 2; /* GXAV_PIN_OUTPUT */
            ioctl(fd, GXAV_IOCTL_FIFO_UNLINK, &req);
        }
    }
    close(fd);
}

int player_init(void) {
    s_player_state = PLAYER_STATE_STOPPED;
    s_current_url[0] = '\0';
    gxav_cleanup_audio();
    return 0;
}

int player_play(const char *url, const char *channel_name) {
    (void)channel_name;
    if (!url) return -1;
    
    /* Para qualquer reproducao anterior e limpa processos conflitantes */
    player_stop();
    system("killall -9 dvbapp 2>/dev/null; umount -l /app 2>/dev/null; usleep 50000;");

    /* Forca liberacao de qualquer canal de audio/video preso no driver */
    gxav_cleanup_audio();

    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_player_state = PLAYER_STATE_PLAYING;

    /* No GX6622 / Nationalchip, o showcartoon sem '-p' ativa o decodificador completo de AV (Audio + Video) */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "/usr/local/bin/showcartoon \"%s\" 60 >/tmp/showcartoon.log 2>&1 &", url);
    system(cmd);

    return 0;
}

void player_stop(void) {
    if (s_player_state != PLAYER_STATE_STOPPED) {
        system("killall -TERM showcartoon 2>/dev/null; usleep 200000; killall -9 showcartoon 2>/dev/null");
        s_player_state = PLAYER_STATE_STOPPED;
        s_current_url[0] = '\0';
        gxav_cleanup_audio();
    }
}

player_state_t player_get_state(void) {
    return s_player_state;
}

void player_close(void) {
    player_stop();
}
