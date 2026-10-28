#include "iptv_catalog.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "catalog_quality_test.m3u8";
    const char *names[] = {"HBO FHD", "HBO HD", "TV 1080p", "TV 720p"};
    FILE *file = fopen(path, "w");
    iptv_catalog_t catalog = {0};
    int result = 0;
    if (!file) return 1;
    fputs("#EXTM3U\n", file);
    for (int i = 0; i < 4; i++)
        fprintf(file, "#EXTINF:-1 group-title=\"Teste\",%s\nhttp://example.invalid/%d.ts\n",
                names[i], i);
    if (fclose(file) != 0) { remove(path); return 2; }
    if (catalog_load_m3u(&catalog, path) != 4) result = 3;
    else if (!catalog.channels[0].is_fullhd || catalog.channels[0].is_hd) result = 4;
    else if (!catalog.channels[1].is_hd || catalog.channels[1].is_fullhd) result = 5;
    else if (!catalog.channels[2].is_fullhd || catalog.channels[2].is_hd) result = 6;
    else if (!catalog.channels[3].is_hd || catalog.channels[3].is_fullhd) result = 7;
    catalog_free(&catalog);
    remove(path);
    return result;
}
