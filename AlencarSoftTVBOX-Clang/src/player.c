/**
 * @file player.c
 * @brief Process ownership and playback state for the receiver backend.
 */

#include "player.h"
#include "hls_bridge.h"
#ifdef RECEIVER_DLNA_BACKEND
#include "firmware_dlna.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static player_state_t s_player_state = PLAYER_STATE_STOPPED;
#ifndef RECEIVER_DLNA_BACKEND
static pid_t s_player_pid = -1;
#endif
static int s_uses_bridge = 0;
static time_t s_start_time = 0;

int player_init(void) {
    s_player_state = PLAYER_STATE_STOPPED;
#ifndef RECEIVER_DLNA_BACKEND
    s_player_pid = -1;
#endif
    s_uses_bridge = 0;
    return 0;
}

int player_play(const char *url, const char *channel_name) {
#ifdef RECEIVER_DLNA_BACKEND
    if (!url || !*url) return -1;
    player_stop();
    if (firmware_dlna_start(url, channel_name) < 0) {
        fprintf(stderr, "[Player] Falha ao iniciar player DLNA do firmware.\n");
        s_player_state = PLAYER_STATE_ERROR;
        return -1;
    }
    s_uses_bridge = strstr(url, ".m3u") != NULL;
    s_start_time = time(NULL);
    s_player_state = PLAYER_STATE_PLAYING;
    return 0;
#else
    const char *play_url = url;
    pid_t pid;
    int status;
    (void)channel_name;

    if (!url || !*url) return -1;
    player_stop();

    if (strstr(url, ".m3u8") || strstr(url, ".m3u")) {
        if (hls_bridge_start(url) < 0) {
            fprintf(stderr, "[Player] Ponte HLS nao iniciou.\n");
            s_player_state = PLAYER_STATE_ERROR;
            return -1;
        }
        play_url = hls_bridge_get_url();
        s_uses_bridge = 1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork showcartoon");
        hls_bridge_stop();
        s_player_state = PLAYER_STATE_ERROR;
        return -1;
    }
    if (pid == 0) {
        int log_fd = open("/tmp/showcartoon.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            if (log_fd > STDERR_FILENO) close(log_fd);
        }
        execl("/usr/local/bin/showcartoon", "showcartoon", play_url, "60", (char *)NULL);
        _exit(127);
    }

    s_player_pid = pid;
    s_player_state = PLAYER_STATE_BUFFERING;
    s_start_time = time(NULL);
    usleep(100000);
    if (waitpid(pid, &status, WNOHANG) == pid) {
        fprintf(stderr, "[Player] showcartoon encerrou na inicializacao: status=%d\n", status);
        s_player_pid = -1;
        hls_bridge_stop();
        s_player_state = PLAYER_STATE_ERROR;
        return -1;
    }
    /* Running process is only a launch check; it does not prove image or audio. */
    if (!s_uses_bridge) s_player_state = PLAYER_STATE_PLAYING;
    return 0;
#endif
}

void player_stop(void) {
#ifdef RECEIVER_DLNA_BACKEND
    firmware_dlna_stop();
#else
    pid_t pid = s_player_pid;
    if (pid > 0) {
        int status;
        int i;
        kill(pid, SIGTERM);
        for (i = 0; i < 20; i++) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid || (result < 0 && errno == ECHILD)) break;
            usleep(10000);
        }
        if (i == 20) {
            kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }
        s_player_pid = -1;
    }
    hls_bridge_stop();
#endif
    s_uses_bridge = 0;
    s_player_state = PLAYER_STATE_STOPPED;
}

player_state_t player_get_state(void) {
#ifdef RECEIVER_DLNA_BACKEND
    if (firmware_dlna_active()) {
        return PLAYER_STATE_PLAYING;
    }
    return s_player_state;
#else
    if (s_player_pid > 0) {
        int status = 0;
        pid_t ended = waitpid(s_player_pid, &status, WNOHANG);
        if (ended == s_player_pid || (ended < 0 && errno == ECHILD)) {
            fprintf(stderr, "[Player] showcartoon encerrou: status=%d\n", status);
            s_player_pid = -1;
            hls_bridge_stop();
            s_player_state = PLAYER_STATE_ERROR;
        }
    }
    if (s_player_state == PLAYER_STATE_BUFFERING && s_uses_bridge) {
        if (hls_bridge_has_data()) s_player_state = PLAYER_STATE_PLAYING;
        else if (time(NULL) - s_start_time > 15) {
            fprintf(stderr, "[Player] Tempo limite aguardando dados HLS.\n");
            player_stop();
            s_player_state = PLAYER_STATE_ERROR;
        }
    }
    return s_player_state;
#endif
}

void player_close(void) {
    player_stop();
}
