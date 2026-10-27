#include "hls_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int fd, expected, total = 0;
    time_t deadline;
    unsigned char bytes[16384];
    if (argc != 4) return 2;
    expected = atoi(argv[3]);
    if (strcmp(argv[1], "probe") == 0)
        return hls_source_probe(argv[2]) == 0 ? 0 : 10;
    if (hls_bridge_start(argv[2]) < 0) return 3;
    if (strcmp(argv[1], "stop") == 0) {
        usleep(500000);
        hls_bridge_stop();
        return 0;
    }
    if (strcmp(argv[1], "empty") == 0) {
        sleep(2);
        total = hls_bridge_has_data();
        hls_bridge_stop();
        return total ? 4 : 0;
    }
    if (strcmp(argv[1], "ready") == 0) {
        deadline = time(NULL) + 6;
        while (!hls_bridge_has_data() && time(NULL) < deadline) usleep(50000);
        total = hls_bridge_has_data();
        hls_bridge_stop();
        return total ? 0 : 8;
    }
    if ((strcmp(argv[1], "stream") != 0 && strcmp(argv[1], "sample") != 0 &&
         strcmp(argv[1], "live") != 0) ||
        expected <= 0) return 5;
    fd = open(hls_bridge_get_url(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 6;
    /* Bounded diagnostic stream for the firmware player through HTTP CGI. */
    deadline = time(NULL) + (strcmp(argv[1], "live") == 0 ? 120 : 10);
    while (total < expected && time(NULL) < deadline) {
        struct pollfd pfd;
        ssize_t n;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 200) <= 0) continue;
        n = read(fd, bytes,
                 (size_t)(expected - total) < sizeof(bytes) ?
                 (size_t)(expected - total) : sizeof(bytes));
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (n <= 0) break;
        if (fwrite(bytes, 1, (size_t)n, stdout) != (size_t)n) break;
        total += (int)n;
    }
    fflush(stdout);
    close(fd);
    hls_bridge_stop();
    return total == expected ? 0 : 7;
}
