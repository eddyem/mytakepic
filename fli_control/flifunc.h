/*
 * This file is part of the FLI_control project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef FLIFUNC_H__
#define FLIFUNC_H__

#include <libfli.h>
#include <stdint.h>

#define LIBVERSIZ 1024
#define BUFF_SIZ 4096

#ifndef FLIUSB_VENDORID
#define FLIUSB_VENDORID 0xf18
#endif
#ifndef FLIUSB_PROLINE_ID
#define FLIUSB_PROLINE_ID 0x0a
#endif
#ifndef FLIUSB_FILTER_ID
#define FLIUSB_FILTER_ID 0x07
#endif
#ifndef FLIUSB_FOCUSER_ID
#define FLIUSB_FOCUSER_ID 0x06
#endif

// wheel position in steps = WHEEL_POS0STPS + WHEEL_STEPPOS*N
#define WHEEL_POS0STPS  (239)
#define WHEEL_STEPPOS   (48)
// 1mm == FOCSCALE steps of focuser
#define FOCSCALE        (10000.)

typedef struct{
    uint16_t *data;
    int w;
    int h;
} IMG;

int fli_init();
void focusers();
void wheels();
void ccds();
void saveImages(IMG *img, char *filename);

#endif // FLIFUNC_H__
