/**
 * @file display_vfd.h
 * @brief Driver de controle do visor frontal (VFD e LED).
 */

#ifndef ALENCARSOFT_DISPLAY_VFD_H
#define ALENCARSOFT_DISPLAY_VFD_H

#include "platform.h"

/* Inicializa o driver do painel frontal de acordo com o hardware detectado */
int vfd_init(panel_type_t type);

/* Exibe um texto ou nome de canal no visor */
int vfd_set_text(const char *text);

/* Exibe o numero do canal formatado (ex: "CH01", "0252") */
int vfd_set_channel_number(int number);

/* Exibe o relogio (ex: "22:30") */
int vfd_set_clock(int hours, int minutes);

/* Encerra o driver do visor */
void vfd_close(void);

#endif /* ALENCARSOFT_DISPLAY_VFD_H */
