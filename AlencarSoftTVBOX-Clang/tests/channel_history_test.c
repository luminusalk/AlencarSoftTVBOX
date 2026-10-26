#include "channel_history.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[] = "/tmp/alencar-history-XXXXXX";
    iptv_channel_t channels[3];
    iptv_catalog_t cat;
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    unlink(path);
    memset(channels, 0, sizeof(channels));
    memset(&cat, 0, sizeof(cat));
    cat.channels = channels;
    cat.total_channels = 3;
    strcpy(channels[0].url, "https://example.test/first.m3u8");
    strcpy(channels[1].url, "https://example.test/last.m3u8");
    strcpy(channels[2].url, "http://example.test/video.ts");
    assert(channel_history_save(&channels[1], path) == 0);
    assert(channel_history_find(&cat, path) == 1);
    /* History survives changes in catalogue order. */
    strcpy(channels[1].url, "https://example.test/other.m3u8");
    strcpy(channels[2].url, "https://example.test/last.m3u8");
    assert(channel_history_find(&cat, path) == 2);
    unlink(path);
    return 0;
}
