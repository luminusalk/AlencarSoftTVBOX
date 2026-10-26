#include "channel_history.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int channel_history_find(const iptv_catalog_t *catalog, const char *path) {
    char url[OSD_URL_MAX_LEN + 2];
    FILE *file;
    size_t length;
    if (!catalog || !catalog->channels || !path) return -1;
    file = fopen(path, "r");
    if (!file) return -1;
    if (!fgets(url, sizeof(url), file)) { fclose(file); return -1; }
    fclose(file);
    length = strcspn(url, "\r\n");
    if (length == 0 || length >= OSD_URL_MAX_LEN) return -1;
    url[length] = '\0';
    for (int i = 0; i < catalog->total_channels; i++)
        if (strcmp(catalog->channels[i].url, url) == 0) return i;
    return -1;
}

int channel_history_save(const iptv_channel_t *channel, const char *path) {
    char temporary[512];
    FILE *file;
    int n, fd, written, closed;
    if (!channel || !channel->url[0] || !path) return -1;
    n = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(temporary)) return -1;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    file = fdopen(fd, "w");
    if (!file) { close(fd); unlink(temporary); return -1; }
    written = fprintf(file, "%s\n", channel->url);
    closed = fclose(file);
    if (written < 0 || closed != 0) {
        unlink(temporary);
        return -1;
    }
    if (rename(temporary, path) < 0) { unlink(temporary); return -1; }
    return 0;
}
