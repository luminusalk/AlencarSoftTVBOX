/**
 * @file config.h
 * @brief Configuracoes globais do AlencarSoftTVBOX-Clang.
 */

#ifndef ALENCARSOFT_CONFIG_H
#define ALENCARSOFT_CONFIG_H

#define APP_NAME            "AlencarSoft TVBOX"
#define APP_VERSION         "1.0.0"
#define APP_AUTHOR          "Pedro Alencar"

/* Caminhos padrao no sistema embarcado */
#define DEFAULT_CATALOG_PATH "/mnt/plugin/catalog.json"
#define DEFAULT_M3U_PATH     "/mnt/plugin/iptv/iptv.m3u8"
#define DEFAULT_FB_DEV       "/dev/fb0"
#define DEFAULT_IR_DEV       "/dev/gxirr0"
#define DEFAULT_VFD_DEV      "/dev/ttyS1"

/* Constantes visuais */
#define OSD_MAX_CHANNELS_PER_PAGE 9
#define OSD_TITLE_MAX_LEN         64
#define OSD_URL_MAX_LEN           512
#define OSD_LOGO_MAX_LEN          256
#define OSD_CAT_MAX_LEN           48

/* Limite maximo de canais suportados na memoria */
#define MAX_CHANNELS_COUNT        1024
#define MAX_CATEGORIES_COUNT      32

#endif /* ALENCARSOFT_CONFIG_H */
