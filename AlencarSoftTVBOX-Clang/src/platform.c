/**
 * @file platform.c
 * @brief Deteccao automatica de arquitetura e modelo de receptor.
 */

#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

platform_info_t platform_detect(void) {
    platform_info_t info;
    memset(&info, 0, sizeof(info));
    strcpy(info.model_name, "Receptor Generico");
    info.soc = SOC_UNKNOWN;
    info.panel = PANEL_NONE;
    info.ram_mb = 256;
    info.flash_mb = 16;

    /* 1. Tenta identificar se e um NationalChip GX6622 via /etc/gx.cfg */
    FILE *f = fopen("/etc/gx.cfg", "r");
    if (f) {
        char line[128];
        int panel_type = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "MODEID=")) {
                char *start = strstr(line, "MODEID=") + 7;
                char *end = strchr(start, ';');
                if (!end) end = strchr(start, '\n');
                if (end) *end = '\0';
                strncpy(info.model_name, start, sizeof(info.model_name) - 1);
            }
            if (strstr(line, "FRANTPANNELTYPE=")) {
                panel_type = atoi(strstr(line, "FRANTPANNELTYPE=") + 16);
            }
        }
        fclose(f);

        info.soc = SOC_NATIONALCHIP_GX6622;
        info.flash_mb = 128;
        info.ram_mb = 256;

        if (panel_type == 6) {
            info.panel = PANEL_PT6393_VFD; /* VFD Alfanumerico */
        } else {
            info.panel = PANEL_7SEG_LED_4DIGIT;
        }
        return info;
    }

    /* 2. Tenta identificar via /proc/cpuinfo se e Availink AVL2569D */
    f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "csky") || strstr(line, "CSKY")) {
                info.soc = SOC_NATIONALCHIP_GX6622;
            } else if (strstr(line, "MIPS") || strstr(line, "Availink") || strstr(line, "AVL")) {
                info.soc = SOC_AVAILINK_AVL2569D;
                info.panel = PANEL_7SEG_LED_4DIGIT;
                strcpy(info.model_name, "Availink Box");
            }
        }
        fclose(f);
    }

    return info;
}
