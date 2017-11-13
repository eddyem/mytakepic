/*
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <limits.h>
#include <libfli.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars  G;

int rewrite_ifexists = 0, // rewrite existing files == 0 or 1
    verbose = 0; // each -v increments this value, e.g. -vvv sets it to 3
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .outfile = "fli_out",
    .objtype = "object",
    .instrument = "direct imaging",
    .exptime = -1,
    .nframes = 1,
    .hbin = 1, .vbin = 1,
    .X0 = -1, .Y0 = -1,
    .X1 = -1, .Y1 = -1,
    .temperature = 1e6,
    .shtr_cmd = -1,
    .confio = -1, .setio = -1,
    .gotopos = INT_MAX, .addsteps = INT_MAX,
    .setwheel = -1,
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    &help,   1,     arg_none,   NULL,               N_("show this help")},
    {"force",   NO_ARGS,    &rewrite_ifexists,1,arg_none,NULL,              N_("rewrite output file if exists")},
    {"verbose", NO_ARGS,    NULL,   'V',    arg_none,   APTR(&verbose),     N_("verbose level (each -v increase it)")},
    {"dark",    NO_ARGS,    NULL,   'd',    arg_int,    APTR(&G.dark),      N_("not open shutter, when exposing (\"dark frames\")")},
    {"open-shutter",NO_ARGS,&G.shtr_cmd,FLI_SHUTTER_OPEN,arg_none,NULL,     N_("open shutter")},
    {"close-shutter",NO_ARGS,&G.shtr_cmd,FLI_SHUTTER_CLOSE,arg_none,NULL,   N_("close shutter")},
    {"shutter-on-low",NO_ARGS,&G.shtr_cmd,FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL|FLI_SHUTTER_EXTERNAL_TRIGGER_LOW,arg_none,NULL,   N_("run exposition on LOW @ pin5 I/O port")},
    {"shutter-on-high",NO_ARGS,&G.shtr_cmd,FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL|FLI_SHUTTER_EXTERNAL_TRIGGER_HIGH,arg_none,NULL, N_("run exposition on HIGH @ pin5 I/O port")},
    {"get-ioport",NO_ARGS,  NULL,   'i',    arg_int,    APTR(&G.getio),     N_("get value of I/O port pins")},
    {"async",   NO_ARGS,    &G.async,1,     arg_none,   NULL,               N_("move stepper motor asynchronous")},
    //{"fast",    NO_ARGS,    NULL,   '8',    arg_int,    APTR(&G.fast),      N_("run in 8-bit mode")},
    //{"",  NO_ARGS,    NULL,   '',    arg_int,  APTR(&G.),    N_("")},

    {"author",  NEED_ARG,   NULL,   'A',    arg_string, APTR(&G.author),    N_("program author")},
    {"objtype", NEED_ARG,   NULL,   'Y',    arg_string, APTR(&G.objtype),   N_("object type (neon, object, flat etc)")},
    {"instrument",NEED_ARG, NULL,   'I',    arg_string, APTR(&G.instrument),N_("instrument name")},
    {"object",  NEED_ARG,   NULL,   'O',    arg_string, APTR(&G.objname),   N_("object name")},
    {"obsname", NEED_ARG,   NULL,   'N',    arg_string, APTR(&G.observers), N_("observers' names")},
    {"prog-id", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.prog_id),   N_("observing program name")},
    //{"",  NEED_ARG,   NULL,   '',    arg_string, APTR(&G.),    N_("")},

    {"nflushes",NEED_ARG,   NULL,   'f',    arg_int,    APTR(&G.nflushes),  N_("N flushes before exposing")},
    {"hbin",    NEED_ARG,   NULL,   'h',    arg_int,    APTR(&G.hbin),      N_("horizontal binning to N pixels")},
    {"vbin",    NEED_ARG,   NULL,   'v',    arg_int,    APTR(&G.vbin),      N_("vertical binning to N pixels")},
    {"nframes", NEED_ARG,   NULL,   'n',    arg_int,    APTR(&G.nframes),   N_("make series of N frames")},
    {"pause",   NEED_ARG,   NULL,   'p',    arg_int,    APTR(&G.pause_len), N_("make pause for N seconds between expositions")},
    {"exptime", NEED_ARG,   NULL,   'x',    arg_int,    APTR(&G.exptime),   N_("set exposure time to given value (ms)")},
    {"X0",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.X0),        N_("frame X0 coordinate (-1 - all with overscan)")},
    {"Y0",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.Y0),        N_("frame Y0 coordinate (-1 - all with overscan)")},
    {"X1",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.X1),        N_("frame X1 coordinate (-1 - all with overscan)")},
    {"Y1",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.Y1),        N_("frame Y1 coordinate (-1 - all with overscan)")},
    {"set-ioport",NEED_ARG, NULL,   's',    arg_int,    APTR(&G.setio),     N_("set I/O port pins to given value (decimal number, pin1 is LSB)")},
    {"conf-ioport",NEED_ARG,NULL,   'c',    arg_int,    APTR(&G.confio),    N_("configure I/O port pins to given value (decimal number, pin1 is LSB, 1 == output, 0 == input)")},
    {"goto",    NEED_ARG,   NULL,   'g',    arg_int,    APTR(&G.gotopos),   N_("move focuser to absolute position")},
    {"addsteps",NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.addsteps),  N_("move focuser to relative position")},
    {"wheel-get",NO_ARGS,   NULL,   0,      arg_none,   APTR(&G.getwheel),  N_("get current wheel position")},
    {"wheel-set",NEED_ARG,  NULL,   'w',    arg_int,    APTR(&G.setwheel),  N_("set wheel position")},
    //{"",  NEED_ARG,   NULL,   '',    arg_int,   APTR(&G.),    N_("")},

    {"set-temp",NEED_ARG,   NULL,   't',    arg_double, APTR(&G.temperature),N_("set CCD temperature to given value (degr C)")},
    //{"",  NEED_ARG,   NULL,   '',    arg_double,   APTR(&G.),    N_("")},

    end_option
};


/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    // format of help: "Usage: progname [args]\n"
    change_helpstring("Usage: %s [args] <output file prefix>\n\n\tWhere args are:\n");
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.outfile = strdup(argv[0]);
        if(argc > 1){
            WARNX("%d unused parameters:\n", argc - 1);
            for(int i = 1; i < argc; ++i)
                printf("\t%4d: %s\n", i, argv[i]);
        }
    }
    return &G;
}

