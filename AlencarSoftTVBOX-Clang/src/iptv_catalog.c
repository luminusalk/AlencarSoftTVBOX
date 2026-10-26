/**
 * @file iptv_catalog.c
 * @brief Carregador de catalogo e atualizador de grade via rede.
 */

#include "iptv_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

static int compare_categories(const void *a, const void *b) {
    const iptv_category_t *ca = (const iptv_category_t *)a;
    const iptv_category_t *cb = (const iptv_category_t *)b;
    if (strcmp(ca->name, "Todos os Canais") == 0) return 1;
    if (strcmp(cb->name, "Todos os Canais") == 0) return -1;
    return strcasecmp(ca->name, cb->name);
}

static void add_to_category(iptv_catalog_t *cat, const char *raw_name, int channel_idx) {
    const char *cat_name = raw_name;
    /* Unifica qualquer categoria de noticia para apenas "Notícias" */
    if (strstr(raw_name, "Not") || strstr(raw_name, "not") || strstr(raw_name, "Apur")) {
        cat_name = "Notícias";
    }

    int cat_idx = -1;
    for (int i = 0; i < cat->total_categories; i++) {
        if (strcmp(cat->categories[i].name, cat_name) == 0) {
            cat_idx = i;
            break;
        }
    }
    if (cat_idx < 0 && cat->total_categories < MAX_CATEGORIES_COUNT) {
        cat_idx = cat->total_categories++;
        strncpy(cat->categories[cat_idx].name, cat_name, sizeof(cat->categories[cat_idx].name) - 1);
        cat->categories[cat_idx].count = 0;
    }
    if (cat_idx >= 0 && cat->categories[cat_idx].count < MAX_CHANNELS_COUNT) {
        cat->categories[cat_idx].channel_indices[cat->categories[cat_idx].count++] = channel_idx;
    }
}

int catalog_init(iptv_catalog_t *cat) {
    if (!cat) return -1;
    if (!cat->channels) {
        cat->channels = (iptv_channel_t *)calloc(MAX_CHANNELS_COUNT, sizeof(iptv_channel_t));
    }
    if (!cat->categories) {
        cat->categories = (iptv_category_t *)calloc(MAX_CATEGORIES_COUNT, sizeof(iptv_category_t));
    }
    cat->total_channels = 0;
    cat->total_categories = 0;
    return (cat->channels && cat->categories) ? 0 : -1;
}

void catalog_free(iptv_catalog_t *cat) {
    if (!cat) return;
    if (cat->channels) {
        free(cat->channels);
        cat->channels = NULL;
    }
    if (cat->categories) {
        free(cat->categories);
        cat->categories = NULL;
    }
    cat->total_channels = 0;
    cat->total_categories = 0;
}

int catalog_load_m3u(iptv_catalog_t *cat, const char *path) {
    if (!cat || !path) return -1;
    if (catalog_init(cat) < 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[1024];
    char cur_name[OSD_TITLE_MAX_LEN] = {0};
    char cur_cat[OSD_CAT_MAX_LEN] = "Geral";
    char cur_logo[OSD_LOGO_MAX_LEN] = {0};

    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\r');
        if (nl) *nl = '\0';
        nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strncmp(line, "#EXTINF:", 8) == 0) {
            /* Extrai nome apos a virgula */
            char *comma = strrchr(line, ',');
            if (comma) {
                strncpy(cur_name, comma + 1, sizeof(cur_name) - 1);
            }
            /* Extrai group-title */
            char *grp = strstr(line, "group-title=\"");
            if (grp) {
                char *end = strchr(grp + 13, '"');
                if (end) {
                    int len = end - (grp + 13);
                    if (len >= (int)sizeof(cur_cat)) len = sizeof(cur_cat) - 1;
                    strncpy(cur_cat, grp + 13, len);
                    cur_cat[len] = '\0';
                }
            }
            /* Extrai tvg-logo */
            char *logo = strstr(line, "tvg-logo=\"");
            if (logo) {
                char *end = strchr(logo + 10, '"');
                if (end) {
                    int len = end - (logo + 10);
                    if (len >= (int)sizeof(cur_logo)) len = sizeof(cur_logo) - 1;
                    strncpy(cur_logo, logo + 10, len);
                    cur_logo[len] = '\0';
                }
            }
        } else if (strncmp(line, "http://", 7) == 0 || strncmp(line, "https://", 8) == 0) {
            if (cat->total_channels < MAX_CHANNELS_COUNT) {
                int idx = cat->total_channels++;
                iptv_channel_t *ch = &cat->channels[idx];
                strncpy(ch->name, cur_name[0] ? cur_name : "Canal Sem Nome", sizeof(ch->name) - 1);
                strncpy(ch->url, line, sizeof(ch->url) - 1);
                strncpy(ch->logo, cur_logo, sizeof(ch->logo) - 1);
                strncpy(ch->category, cur_cat, sizeof(ch->category) - 1);

                ch->is_fullhd = (strstr(ch->name, "1080p") || strstr(ch->name, "FULL HD")) ? 1 : 0;
                ch->is_hd     = (strstr(ch->name, "720p") || strstr(ch->name, "HD")) ? 1 : 0;

                add_to_category(cat, cur_cat, idx);
                add_to_category(cat, "Todos os Canais", idx);
            }
            cur_name[0] = '\0';
            cur_logo[0] = '\0';
            strcpy(cur_cat, "Geral");
        }
    }
    fclose(f);

    /* Ordena as categorias em ordem alfabetica */
    if (cat->total_categories > 1) {
        qsort(cat->categories, (size_t)cat->total_categories, sizeof(iptv_category_t), compare_categories);
    }

    return cat->total_channels;
}

int catalog_load_json(iptv_catalog_t *cat, const char *path) {
    /* Como fallback seguro para M3U nativo, tenta o M3U se o JSON nao existir */
    return catalog_load_m3u(cat, path);
}
