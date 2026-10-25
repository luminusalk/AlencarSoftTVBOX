/**
 * @file iptv_catalog.h
 * @brief Gerenciador de catalogo e canais IPTV.
 */

#ifndef ALENCARSOFT_IPTV_CATALOG_H
#define ALENCARSOFT_IPTV_CATALOG_H

#include "config.h"

typedef struct {
    char name[OSD_TITLE_MAX_LEN];
    char url[OSD_URL_MAX_LEN];
    char logo[OSD_LOGO_MAX_LEN];
    char category[OSD_CAT_MAX_LEN];
    int  is_hd;     /* 1 se for 720p HD */
    int  is_fullhd; /* 1 se for 1080p FULL HD */
} iptv_channel_t;

typedef struct {
    char name[OSD_CAT_MAX_LEN];
    int  channel_indices[MAX_CHANNELS_COUNT];
    int  count;
} iptv_category_t;

typedef struct {
    iptv_channel_t  *channels;
    iptv_category_t *categories;
    int             total_channels;
    int             total_categories;
} iptv_catalog_t;

int  catalog_init(iptv_catalog_t *cat);
void catalog_free(iptv_catalog_t *cat);
int  catalog_load_m3u(iptv_catalog_t *cat, const char *path);
int  catalog_load_json(iptv_catalog_t *cat, const char *path);
void catalog_filter_quality(iptv_catalog_t *cat, int prefer_fullhd);

#endif /* ALENCARSOFT_IPTV_CATALOG_H */
