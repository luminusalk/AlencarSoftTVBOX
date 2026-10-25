/**
 * @file platform.h
 * @brief Deteccao de hardware e abstracao de plataforma.
 */

#ifndef ALENCARSOFT_PLATFORM_H
#define ALENCARSOFT_PLATFORM_H

typedef enum {
    SOC_UNKNOWN = 0,
    SOC_NATIONALCHIP_GX6622, /* Azamerica S1009+ */
    SOC_AVAILINK_AVL2569D    /* Cinebox Power Q, Duosat Prodigy S, Tuning Origins */
} soc_type_t;

typedef enum {
    PANEL_NONE = 0,
    PANEL_PT6393_VFD,        /* Alfanumerico 14 segmentos (Azamerica S1009+) */
    PANEL_7SEG_LED_4DIGIT,   /* 4 Digitos LED (Cinebox / Tuning) */
    PANEL_SERIAL_STM8S       /* Painel via MCU serial */
} panel_type_t;

typedef struct {
    soc_type_t   soc;
    panel_type_t panel;
    char         model_name[32];
    int          ram_mb;
    int          flash_mb;
} platform_info_t;

/* Detecta a plataforma lendo /etc/gx.cfg ou /proc/cpuinfo */
platform_info_t platform_detect(void);

#endif /* ALENCARSOFT_PLATFORM_H */
