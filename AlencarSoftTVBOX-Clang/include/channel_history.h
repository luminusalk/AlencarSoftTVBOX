#ifndef ALENCAR_CHANNEL_HISTORY_H
#define ALENCAR_CHANNEL_HISTORY_H
#include "iptv_catalog.h"

#define CHANNEL_HISTORY_PATH "/mnt/plugin/alencar_last_channel.txt"
int channel_history_find(const iptv_catalog_t *catalog, const char *path);
int channel_history_save(const iptv_channel_t *channel, const char *path);
#endif
