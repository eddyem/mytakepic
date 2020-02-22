/*
 *                                                                                                  geany_encoding=koi8-r
 * main.h
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#pragma once
#ifndef __MAIN_H__
#define __MAIN_H__

#include "usefull_macros.h"
#include "cmdlnopts.h"

#ifdef USEPNG
#include <png.h>
static int writepng(char *filename, int width, int height, void *data);
#endif /* USEPNG */

#include <libfli.h>

#define LIBVERSIZ 1024

// wheel position in steps = WHEEL_POS0STPS + WHEEL_STEPPOS*N
#define WHEEL_POS0STPS  (239)
#define WHEEL_STEPPOS   (48)
// 1mm == FOCSCALE steps of focuser
#define FOCSCALE        (10000.)

typedef struct{
    flidomain_t domain;
    char *dname;
    char *name;
}cam_t;

static int findcams(flidomain_t domain, cam_t **cam);

#ifdef USERAW
static int writeraw(char *filename, int width, int height, void *data);
#endif // USERAW

#define TRYFITS(f, ...)                     \
do{ int status = 0;                         \
    f(__VA_ARGS__, &status);                \
    if (status){                            \
        fits_report_error(stderr, status);  \
        return -1;}                         \
}while(0)
#define WRITEKEY(...)                           \
do{ int status = 0;                             \
    fits_write_key(__VA_ARGS__, &status);       \
    if(status) fits_report_error(stderr, status);\
}while(0)
static int writefits(char *filename, int width, int height, void *data);


#endif // __MAIN_H__
