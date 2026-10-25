/**
 * @file font_default.h
 * @brief Tabela de fonte bitmap 8x16 embutida (ASCII 32 a 126).
 */

#ifndef ALENCARSOFT_FONT_DEFAULT_H
#define ALENCARSOFT_FONT_DEFAULT_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* Matriz 8x16 para cada caractere ASCII de 32 a 126 */
extern const uint8_t font_8x16[95][16];

#endif /* ALENCARSOFT_FONT_DEFAULT_H */
