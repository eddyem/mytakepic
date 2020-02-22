/*
 *                                                                                                  geany_encoding=koi8-r
 * main.c
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

// for strcasestr
#define _GNU_SOURCE
#include <string.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fitsio.h>
#include <math.h>

#include "main.h"

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

static long fli_err;
#define TRYFUNC(f, ...)             \
do{ if((fli_err = f(__VA_ARGS__)))  \
        WARNX(#f "() failed");      \
}while(0)

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data);
#endif /* USEPNG */

#define BUFF_SIZ 4096

#define TMBUFSIZ 40
static char tm_buf[TMBUFSIZ];  // buffer for string with time value

static glob_pars *G = NULL; // default parameters see in cmdlnopts.c

static uint16_t max = 0, min = 65535; // max/min values for given image
static double avr, std; // stat values
static char *camera = NULL, viewfield[80];
static double pixX, pixY; // pixel size in um

static void print_stat(u_int16_t *img, long size);

static size_t curtime(char *s_time){ // current date/time
    time_t tm = time(NULL);
    return strftime(s_time, TMBUFSIZ, "%d/%m/%Y,%H:%M:%S", localtime(&tm));
}

static fliframe_t frametype = FLI_FRAME_TYPE_NORMAL;
static double t_ext, t_int;    // external & CCD temperatures @exp. end
static time_t expStartsAt;     // exposition start time (time_t)

static long filterpos = -1;    // filter position
static long focuserpos = -1;   // focuser position

static int check_filename(char *buff, char *outfile, char *ext){
    struct stat filestat;
    int num;
    for(num = 1; num < 10000; num++){
        if(snprintf(buff, BUFF_SIZ, "%s_%04d.%s", outfile, num, ext) < 1)
            return 0;
        if(stat(buff, &filestat)) // no such file or can't stat()
            return 1;
    }
    return 0;
}

void signals(int signo){
    exit(signo);
}

extern const char *__progname;
static void info(const char *fmt, ...){
    va_list ar;
    if(!verbose) return;
    printf("%s: ", __progname);
    va_start(ar, fmt);
    vprintf(fmt, ar);
    va_end(ar);
    printf("\n");
}

int main(int argc, char **argv){
    int i, num;
    long ltmp;
    char libver[LIBVERSIZ]; // FLI library version
    cam_t *cam = NULL;      // list of CCDs available
    flidev_t dev;
    char buff[BUFF_SIZ];
    initial_setup();
    G = parse_args(argc, argv);
   // #ifdef EBUG
    TRYFUNC(FLISetDebugLevel, NULL /* "NO HOST" */, FLIDEBUG_NONE);
  /*  #else
    TRYFUNC(FLISetDebugLevel, NULL, FLIDEBUG_NONE);
    #endif */
    TRYFUNC(FLIGetLibVersion, libver, LIBVERSIZ);
    // ������ ���������� '%s'
    if(!fli_err) info(_("Library version '%s'"), libver);
    /*
     * Find focusers and work with each of them
     */
    num = findcams(FLIDOMAIN_USB | FLIDEVICE_FOCUSER, &cam);
    int nfocs = 0;
    for (i = 0; i < num; i++){
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        if(!fli_err){
            if(!strcasestr(buff, "focuser")){ // not focuser
                TRYFUNC(FLIClose, dev);
                continue;
            }
            // ������:\t\t%s
            info(_("Model:\t\t%s"), buff);
        }
        ++nfocs;
        info(_("Focuser '%s', domain %s"), cam[i].name, cam[i].dname);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // ���. ������: %ld
        if(!fli_err) info(_("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // �����. ������: %ld
        if(!fli_err) info(_("SW revision: %ld"), ltmp);
        TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_INTERNAL, &t_ext);
        if(!fli_err) green("FOCTEMP=%.1f\n", t_ext);
        long curpos = -1, maxpos = -1;
        TRYFUNC(FLIGetStepperPosition, dev, &ltmp);
        if(!fli_err){
            curpos = ltmp;
        }
        TRYFUNC(FLIGetFocuserExtent, dev, &ltmp);
        if(!fli_err){
            green("FOCMAXPOS=%ld\n", ltmp);
            maxpos = ltmp;
        }
        do{
            if(G->gotopos != INT_MAX && G->addsteps != INT_MAX){
                // ������ ������������ ��������� ������������� � ���������� �������
                WARNX(_("You can't use both relative and absolute position"));
                break;
            }
            if(curpos < 0 || maxpos < 0){
                // ������ ����������� �������
                WARNX(_("Error in position detection"));
                break;
            }
            long pos = -1, steps = 0;
            if(G->gotopos != INT_MAX){ // absolute pointing
                pos = G->gotopos;
                steps = pos - curpos;
            }else if(G->addsteps != INT_MAX){ // relative pointing
                steps = G->addsteps;
                pos = curpos + steps;
            }else break;
            if(!steps){
                info(_("Already at position"));
                break;
            }
            if(pos > maxpos || pos < 0){
                // ������� �� ������ �������� �� ������� 0...%ld
                WARNX(_("Position should be in 0...%ld"), maxpos);
                break;
            }
            if(pos == 0){
                // ����������� � ������� �������
                info(_("Moving to home position"));
                if(G->async) TRYFUNC(FLIHomeDevice, dev);
                else TRYFUNC(FLIHomeFocuser, dev);
            }else{
                // ����������� �� %ld �����
                info(_("Moving for %ld steps"), steps);
                if(G->async) TRYFUNC(FLIStepMotorAsync, dev, steps);
                else TRYFUNC(FLIStepMotor, dev, steps);
            }
        }while(0);
        TRYFUNC(FLIGetStepperPosition, dev, &focuserpos);
        if(!fli_err){
            green("FOCPOS=%ld\n", focuserpos);
            curpos = focuserpos;
        }else DBG("Error getting fpos: %ld", fli_err);
        TRYFUNC(FLIClose, dev);
    }
    if(!nfocs) WARNX(_("No focusers found"));
    for (i = 0; i < num; i++)
        FREE(cam[i].name);
    FREE(cam);
    /*
     * Find wheels and work with each of them
     */
    num = findcams(FLIDOMAIN_USB | FLIDEVICE_FILTERWHEEL, &cam);
    int nwheels = 0;
    for (i = 0; i < num; i++){
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetFilterCount, dev, &ltmp);
        if(fli_err || ltmp < 2){// not a wheel
            TRYFUNC(FLIClose, dev);
            continue;
        }
        info(_("Wheel '%s', domain %s"), cam[i].name, cam[i].dname);
        green("WHEELTOTALPOS=%ld\n", ltmp);
        int wheelmaxpos = (int)ltmp - 1;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        // ������:\t\t%s
        if(!fli_err) info(_("Model:\t\t%s"), buff);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // ���. ������: %ld
        if(!fli_err) info(_("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // �����. ������: %ld
        if(!fli_err) info(_("SW revision: %ld"), ltmp);
        else goto closewheeldev;
        if(G->setwheel > -1 && G->setwheel >= ltmp){
            G->setwheel = -1;
            WARNX(_("Wheel position should be from 0 to %ld"), ltmp - 1);
        }
        /**
        TRYFUNC(FLIHomeDevice, dev);
        int ii;
        for(ii = 0; ii < 100; ++ii){
            TRYFUNC(FLIGetStepperPosition, dev, &ltmp);
            if(!fli_err) printf("%ld\t", ltmp);
            TRYFUNC( FLIGetStepsRemaining, dev, &ltmp);
            if(!fli_err) printf("%ld\t", ltmp);
            TRYFUNC(FLIGetFilterPos, dev, &ltmp);
            if(!fli_err) printf("%ld\n", ltmp);
            usleep(50000);
        }

        TRYFUNC(FLIGetActiveWheel, dev, &ltmp);
        if(!fli_err) info(_("Wheel number: %ld"), ltmp);
        TRYFUNC(FLIGetStepperPosition, dev, &ltmp);
        if(!fli_err) info(_("stepper position: %ld"), ltmp);
        */
        ++nwheels;
        if(G->setwheel > -1){
            ltmp = G->setwheel;
            if(ltmp > wheelmaxpos){
                WARNX(_("Position is too big (max %d)"), wheelmaxpos);
                goto closewheeldev;
            }
            TRYFUNC(FLISetFilterPos, dev, ltmp);
            if(!fli_err) info(_("Arrive to position"));
        }
        // this function returns -1 every connection without SETpos!!!
        TRYFUNC(FLIGetFilterPos, dev, &filterpos);
        if(!fli_err && filterpos > -1){
            green("WHEELPOS=%ld\n", filterpos);
        }else{
            filterpos = -1;
        // so try to check current position by steps
            TRYFUNC(FLIGetStepperPosition, dev, &ltmp);
            if(ltmp < 0) ltmp = -ltmp;
            DBG("steps: %ld", ltmp);
            if(!fli_err){
                int pos = (ltmp - WHEEL_POS0STPS+WHEEL_STEPPOS/2)/WHEEL_STEPPOS;
                DBG("pos = %d", pos);
                if(pos > -1 && pos <= wheelmaxpos){
                    filterpos = pos;
                    green("WHEELPOS=%ld\n", filterpos);
                }
            }
        }
closewheeldev:
        TRYFUNC(FLIClose, dev);
    }
    if(!nwheels) WARNX(_("No wheels found"));
    for (i = 0; i < num; i++)
        FREE(cam[i].name);
    FREE(cam);
        /*
     * Find CCDs and work with each of them
     */
    num = findcams(FLIDOMAIN_USB | FLIDEVICE_CAMERA, &cam);
    if(!num) WARNX(_("No CCD found"));
    for (i = 0; i < num; i++){
        long x0,x1, y0,y1, row, img_rows, row_width;
        u_int16_t *img;
        int j;
        // ������ '%s' �� ������ %s
        info(_("Camera '%s', domain %s"), cam[i].name, cam[i].dname);
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        // ������:\t\t%s
        if(!fli_err) info(_("Model:\t\t%s"), buff);
        camera = strdup(buff);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // ���. ������: %ld
        if(!fli_err) info(_("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // �����. ������: %ld
        if(!fli_err) info(_("SW revision: %ld"), ltmp);
        TRYFUNC(FLIGetPixelSize, dev, &pixX, &pixY);
        // ������ �������: %g x %g
        if(!fli_err) info(_("Pixel size: %g x %g"), pixX, pixY);
        TRYFUNC(FLIGetVisibleArea, dev, &x0, &y0, &x1, &y1);
        snprintf(viewfield, 80, "(%ld, %ld)(%ld, %ld)", x0, y0, x1, y1);
        // ������� ����: %s
        if(!fli_err) info(_("Field of view: %s"), viewfield);
        if(G->X1 > x1) G->X1 = x1;
        if(G->Y1 > y1) G->Y1 = y1;
        TRYFUNC(FLIGetArrayArea, dev, &x0, &y0, &x1, &y1);
        // ���� �����������: (%ld, %ld)(%ld, %ld)
        if(!fli_err) info(_("Array field: (%ld, %ld)(%ld, %ld)"), x0, y0, x1, y1);
        TRYFUNC(FLISetHBin, dev, G->hbin);
        TRYFUNC(FLISetVBin, dev, G->vbin);
        if(G->X0 == -1) G->X0 = x0; // default values
        if(G->Y0 == -1) G->Y0 = y0;
        if(G->X1 == -1) G->X1 = x1;
        if(G->Y1 == -1) G->Y1 = y1;
        row_width = (G->X1 - G->X0) / G->hbin;
        img_rows = (G->Y1 - G->Y0) / G->vbin;
        TRYFUNC(FLISetImageArea, dev, G->X0, G->Y0,
            G->X0 + (G->X1 - G->X0) / G->hbin, G->Y0 + (G->Y1 - G->Y0) / G->vbin);
        TRYFUNC(FLISetNFlushes, dev, G->nflushes);
        if(G->temperature < 40.){
            // "��������� ����������� ���: %g �������� �������\n"
            green(_("Set CCD temperature to %g degr.C\n"), G->temperature);
            TRYFUNC(FLISetTemperature, dev, G->temperature);
        }
        TRYFUNC(FLIGetTemperature, dev, &t_int);
        if(!fli_err) green("CCDTEMP=%.1f\n", t_int);
        TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
        if(!fli_err) green("EXTTEMP=%.1f\n", t_ext);
        if(G->shtr_cmd > -1){
            flishutter_t shtr = G->shtr_cmd;
            char *str = NULL;
            switch(shtr){
                case FLI_SHUTTER_CLOSE:
                    str = "close";
                break;
                case FLI_SHUTTER_OPEN:
                    str = "open";
                break;
                case FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL|FLI_SHUTTER_EXTERNAL_TRIGGER_LOW:
                    str = "open @ LOW";
                break;
                case FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL|FLI_SHUTTER_EXTERNAL_TRIGGER_HIGH:
                    str = "open @ HIGH";
                break;
                default:
                    str = "WTF?";
            }
            green(_("%s CCD shutter\n"), str);
            TRYFUNC(FLIControlShutter, dev, shtr);
            /*for(int i = 0; i < 100; ++i){
                long iop;
                TRYFUNC(FLIReadIOPort, dev, &iop);
                if(!r) printf("I/O port data: 0x%02lx\n", iop);
                sleep(1);
            }*/
        }
        if(G->confio > -1){
            // "������� ���������������� ���� I/O ��� %d\n"
            green(_("Try to convfigure I/O port as %d\n"), G->confio);
            TRYFUNC(FLIConfigureIOPort, dev, G->confio);
        }
        if(G->getio){
            long iop;
            TRYFUNC(FLIReadIOPort, dev, &iop);
            if(!fli_err) green("CCDIOPORT=0x%02lx\n", iop);
        }
        if(G->setio > -1){
            // "������� ������ %d � ���� I/O\n"
            green(_("Try to write %d to I/O port\n"), G->setio);
            TRYFUNC(FLIWriteIOPort, dev, G->setio);
        }

        if(G->exptime < DBL_EPSILON) continue;
        /*
        char str[256];
        flimode_t m = 0;
        int ret;
        while((ret = FLIGetCameraModeString (dev, m, str, 255)) == 0){
            str[255] = 0;
            red("String %ld: %s", m, str);
            m++;
        }*/
//        TRYFUNC(FLIGetCameraModeString, dev, m, str, 255);

        TRYFUNC(FLISetExposureTime, dev, G->exptime);
        if(G->dark) frametype = FLI_FRAME_TYPE_DARK;
        TRYFUNC(FLISetFrameType, dev, frametype);
        if(G->_8bit){
            TRYFUNC(FLISetBitDepth, dev, FLI_MODE_8BIT);
            if(fli_err == 0) green(_("8 bit mode\n"));
        }
        TRYFUNC(FLISetCameraMode, dev, G->fast ? 0 : 1);
        if(G->fast) green(_("Fast readout mode\n"));
        if(!G->outfile) red(_("Only show statistics\n"));
        img = MALLOC(uint16_t, img_rows * row_width);
        for (j = 0; j < G->nframes; j ++){
            TRYFUNC(FLIGetTemperature, dev, &G->temperature); // temperature @ exp. start
            printf("\n\n");
            // ������ ����� %d\n
            printf(_("Capture frame %d\n"), j);
            TRYFUNC(FLIExposeFrame, dev);
            expStartsAt = time(NULL); // ����� ������ ����������
            do{
                TRYFUNC(FLIGetTemperature, dev, &t_int);
                TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
                if(curtime(tm_buf)){
                    // ����/�����
                    info("%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
                }
                else WARNX("curtime() error");
                TRYFUNC(FLIGetExposureStatus, dev, &ltmp);
                if(fli_err) continue;
                if(G->shtr_cmd > 0 && G->shtr_cmd & FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL && ltmp == G->exptime){
                    // "�������� �������� ��������"
                    printf(_("wait for external trigger...\n"));
                    sleep(1);
                }else{
                    // %.3f ������ �� ��������� ����������\n
                    printf(_("%.3f seconds till exposition ends\n"), ((float)ltmp) / 1000.);
                    if(ltmp > 10000) sleep(10);
                    else usleep(ltmp * 1000);
                }
            }while(ltmp);
            // ���������� �����������:
            printf(_("Read image: "));
            int portion = 0;
            for (row = 0; row < img_rows; row++){
                TRYFUNC(FLIGrabRow, dev, &img[row * row_width], row_width);
                if(fli_err) break;
                int progress = (int)(((float)row / (float)img_rows) * 100.);
                if(progress/5 > portion){
                    if((++portion)%2) printf("..");
                    else printf("%d%%", portion*5);
                    fflush(stdout);
                }
            }
            printf("100%%\n");
            curtime(tm_buf);
            print_stat(img, row_width * img_rows);
            inline void WRITEIMG(int (*writefn)(char*,int,int,void*), char *ext){
                if(G->outfile == NULL) return;
                if(!check_filename(buff, G->outfile, ext) && !rewrite_ifexists)
                    // �� ���� ��������� ����
                    WARNX(_("Can't save file"));
                else{
                    if(rewrite_ifexists){
                        char *p = "";
                        if(strcmp(ext, "fit") == 0) p = "!";
                        snprintf(buff, BUFF_SIZ, "%s%s.%s", p, G->outfile, ext);
                    }
                    TRYFUNC(writefn, buff, row_width, img_rows, img);
                    // ���� ������� � '%s'
                    if (fli_err == 0) info(_("File saved as '%s'"), buff);
                }
            }
                #ifdef USERAW
                WRITEIMG(writeraw, "raw");
                #endif // USERAW
                WRITEIMG(writefits, "fit");
                #ifdef USEPNG
                WRITEIMG(writepng, "png");
                #endif /* USEPNG */
            if(G->pause_len){
                double delta, time1 = dtime() + G->pause_len;
                while((delta = time1 - dtime()) > 0.){
                    // %d ������ �� ��������� �����\n
                    printf(_("%d seconds till pause ends\n"), (int)delta);
                    TRYFUNC(FLIGetTemperature, dev, &t_int);
                    TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
                    if(curtime(tm_buf)){
                        // ����/�����
                        info("%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
                    }
                    else info("curtime() error");
                    if(delta > 10) sleep(10);
                    else sleep((int)delta);
                }
            }
        }
        FREE(camera);
        FREE(img);
        TRYFUNC(FLIClose, dev);
    }
    for (i = 0; i < num; i++)
        FREE(cam[i].name);
    FREE(cam);
    return 0;
}

static int findcams(flidomain_t domain, cam_t **cam){
    char **tmplist;
    int numcams = 0;
    TRYFUNC(FLIList, domain, &tmplist);
    if (tmplist && tmplist[0]){
        int i, cams = 0;
        for (i = 0; tmplist[i]; i++) cams++;
        if ((*cam = realloc(*cam, (numcams + cams) * sizeof(cam_t))) == NULL)
            ERR("realloc() failed");
        for (i = 0; tmplist[i]; i++){
            int j;
            cam_t *tmpcam = *cam + i;
            for (j = 0; tmplist[i][j] != '\0'; j++)
                if (tmplist[i][j] == ';'){
                    tmplist[i][j] = '\0';
                    break;
                }
            tmpcam->domain = domain;
            tmpcam->name = strdup(tmplist[i]);
            switch (domain & 0xff){
            case FLIDOMAIN_PARALLEL_PORT:
                tmpcam->dname = "parallel port";
                break;
            case FLIDOMAIN_USB:
                tmpcam->dname = "USB";
                ;
                break;
            case FLIDOMAIN_SERIAL:
                tmpcam->dname = "serial";
                break;
            case FLIDOMAIN_INET:
                tmpcam->dname = "inet";
                break;
            default:
                tmpcam->dname = "Unknown domain";
                break;
            }
            DBG("found: %s @ %s", tmpcam->name, tmpcam->dname);
        }
        numcams += cams;
    }
    else DBG("No devices");
    TRYFUNC(FLIFreeList, tmplist);
    return numcams;
}

#ifdef USERAW
static int writeraw(char *filename, int width, int height, void *data){
    int fd, size, err;
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )) == -1){
        WARN("open(%s) failed", filename);
        return -errno;
    }
    size = width * height * sizeof(u_int16_t);
    if ((err = write(fd, data, size)) != size){
        WARN("write() failed");
        err = -errno;
    }
    else err = 0;
    close(fd);
    return err;
}
#endif // USERAW

/**
 * @brief addrec - add FITS records from file
 * @param f (i)        - FITS filename
 * @param filename (i) - name of file
 */
static void addrec(fitsfile *f, char *filename){
    FILE *fp = fopen(filename, "r");
    if(!fp) return;
    char buf[2*FLEN_CARD];
    while(fgets(buf, 2*FLEN_CARD, fp)){
        DBG("check record _%s_", buf);
        int keytype, status = 0;
        char newcard[FLEN_CARD], keyname[FLEN_CARD];
        fits_parse_template(buf, newcard, &keytype, &status);
        if(status){
            fits_report_error(stderr, status);
            continue;
        }
        DBG("reformatted to _%s_", newcard);
        strncpy(keyname, newcard, FLEN_CARD);
        char *eq = strchr(keyname, '='); if(eq) *eq = 0;
        eq = strchr(keyname, ' '); if(eq) *eq = 0;
        DBG("keyname: %s", keyname);
        fits_update_card(f, keyname, newcard, &status);
    }
}

static int writefits(char *filename, int width, int height, void *data){
    long naxes[2] = {width, height}, startTime;
    double tmp = 0.0;
    struct tm *tm_starttime;
    char buf[FLEN_CARD];
    time_t savetime = time(NULL);
    fitsfile *fp;
    TRYFITS(fits_create_file, &fp, filename);
    TRYFITS(fits_create_img, fp, USHORT_IMG, 2, naxes);
    // FILE / Input file original name
    WRITEKEY(fp, TSTRING, "FILE", filename, "Input file original name");
    // ORIGIN / organization responsible for the data
    WRITEKEY(fp, TSTRING, "ORIGIN", "SAO RAS", "organization responsible for the data");
    // OBSERVAT / Observatory name
    WRITEKEY(fp, TSTRING, "OBSERVAT", "Special Astrophysical Observatory, Russia", "Observatory name");
    // DETECTOR / detector
    if(camera){
        WRITEKEY(fp, TSTRING, "DETECTOR", camera, "Detector model");
    }
    // INSTRUME / Instrument
    if(G->instrument){
        WRITEKEY(fp, TSTRING, "INSTRUME", G->instrument, "Instrument");
    }else
        WRITEKEY(fp, TSTRING, "INSTRUME", "direct imaging", "Instrument");
    snprintf(buf, 80, "%.g x %.g", pixX, pixY);
    // PXSIZE / pixel size
    WRITEKEY(fp, TSTRING, "PXSIZE", buf, "Pixel size in m");
    WRITEKEY(fp, TSTRING, "VIEWFLD", viewfield, "Camera field of view");
    // CRVAL1, CRVAL2 / Offset in X, Y
    if(G->X0) WRITEKEY(fp, TINT, "X0", &G->X0, "Subframe left border");
    if(G->Y0) WRITEKEY(fp, TINT, "Y0", &G->Y0, "Subframe upper border");
    if(G->exptime < 2.*DBL_EPSILON) sprintf(buf, "bias");
    else if(frametype == FLI_FRAME_TYPE_DARK) sprintf(buf, "dark");
    else if(G->objtype) strncpy(buf, G->objtype, FLEN_CARD-1);
    else sprintf(buf, "object");
    // IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
    WRITEKEY(fp, TSTRING, "IMAGETYP", buf, "Image type");
    // DATAMAX, DATAMIN / Max,min pixel value
    int itmp = 0;
    WRITEKEY(fp, TINT, "DATAMIN", &itmp, "Min pixel value");
    //itmp = G->fast ? 255 : 65535;
    itmp = 65535;
    WRITEKEY(fp, TINT, "DATAMAX", &itmp, "Max pixel value");
    WRITEKEY(fp, TUSHORT, "STATMAX", &max, "Max data value");
    WRITEKEY(fp, TUSHORT, "STATMIN", &min, "Min data value");
    WRITEKEY(fp, TDOUBLE, "STATAVR", &avr, "Average data value");
    WRITEKEY(fp, TDOUBLE, "STATSTD", &std, "Std. of data value");
    WRITEKEY(fp, TDOUBLE, "TEMP0", &G->temperature, "Camera temperature at exp. start (degr C)");
    WRITEKEY(fp, TDOUBLE, "TEMP1", &t_int, "Camera temperature at exp. end (degr C)");
    WRITEKEY(fp, TDOUBLE, "TEMPBODY", &t_ext, "Camera body temperature at exp. end (degr C)");
    tmp = (G->temperature + t_int) / 2. + 273.15;
    // CAMTEMP / Camera temperature (K)
    WRITEKEY(fp, TDOUBLE, "CAMTEMP", &tmp, "Camera temperature (K)");
    // WHEEL & FOCUSER positions:
    tmp = (double)focuserpos / FOCSCALE;
    WRITEKEY(fp, TDOUBLE, "FOCUS", &tmp, "Current focuser position, mm");
    if(filterpos > -1)
        WRITEKEY(fp, TINT, "FILTER", &filterpos, "Current filter number");
    // EXPTIME / actual exposition time (sec)
    tmp = (double)G->exptime / 1000.;
    WRITEKEY(fp, TDOUBLE, "EXPTIME", &tmp, "Actual exposition time (sec)");
    // DATE / Creation date (YYYY-MM-DDThh:mm:ss, UTC)
    strftime(buf, 80, "%Y-%m-%dT%H:%M:%S", gmtime(&savetime));
    WRITEKEY(fp, TSTRING, "DATE", buf, "Creation date (YYYY-MM-DDThh:mm:ss, UTC)");
    startTime = (long)expStartsAt;
    tm_starttime = localtime(&expStartsAt);
    strftime(buf, 80, "Exposition start time (UNIX)", tm_starttime);
    WRITEKEY(fp, TLONG, "UNIXTIME", &startTime, buf);
    strftime(buf, 80, "%Y/%m/%d", tm_starttime);
    // DATE-OBS / DATE (YYYY/MM/DD) OF OBS.
    WRITEKEY(fp, TSTRING, "DATE-OBS", buf, "DATE OF OBS. (YYYY/MM/DD, local)");
    strftime(buf, 80, "%H:%M:%S", tm_starttime);
    // START / Measurement start time (local) (hh:mm:ss)
    WRITEKEY(fp, TSTRING, "START", buf, "Measurement start time (hh:mm:ss, local)");
    // OBJECT  / Object name
    if(G->objname){
        WRITEKEY(fp, TSTRING, "OBJECT", G->objname, "Object name");
    }
    // BINNING / Binning
    if(G->hbin != 1 || G->vbin != 1){
        snprintf(buf, 80, "%d x %d", G->hbin, G->vbin);
        WRITEKEY(fp, TSTRING, "BINNING", buf, "Binning (hbin x vbin)");
    }
    // OBSERVER / Observers
    if(G->observers){
        WRITEKEY(fp, TSTRING, "OBSERVER", G->observers, "Observers");
    }
    // PROG-ID / Observation program identifier
    if(G->prog_id){
        WRITEKEY(fp, TSTRING, "PROG-ID", G->prog_id, "Observation program identifier");
    }
    // AUTHOR / Author of the program
    if(G->author){
        WRITEKEY(fp, TSTRING, "AUTHOR", G->author, "Author of the program");
    }
    if(G->addhdr){ // add records from files
        char **nxtfile = G->addhdr;
        while(*nxtfile){
            addrec(fp, *nxtfile++);
        }
    }
    TRYFITS(fits_write_img, fp, TUSHORT, 1, width * height, data);
    TRYFITS(fits_close_file, fp);
    return 0;
}

#ifdef USEPNG
static int writepng(char *filename, int width, int height, void *data){
    int err;
    FILE *fp = NULL;
    png_structp pngptr = NULL;
    png_infop infoptr = NULL;
    void *row;
    if ((fp = fopen(filename, "wb")) == NULL){
        err = -errno;
        goto done;
    }
    if ((pngptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                            NULL, NULL, NULL)) == NULL){
        err = -ENOMEM;
        goto done;
    }
    if ((infoptr = png_create_info_struct(pngptr)) == NULL){
        err = -ENOMEM;
        goto done;
    }
    png_init_io(pngptr, fp);
    png_set_compression_level(pngptr, Z_BEST_COMPRESSION);
    png_set_IHDR(pngptr, infoptr, width, height, 16, PNG_COLOR_TYPE_GRAY,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pngptr, infoptr);
    png_set_swap(pngptr);
    for(row = data; height > 0; row += width * sizeof(u_int16_t), height--)
        png_write_row(pngptr, row);
    png_write_end(pngptr, infoptr);
    err = 0;
    done:
    if(fp) fclose(fp);
    if(pngptr) png_destroy_write_struct(&pngptr, &infoptr);
    return err;
}
#endif /* USEPNG */

static void print_stat(u_int16_t *img, long size){
    long i, Noverld = 0L;
    double pv, sum=0., sum2=0., sz = (double)size;
    u_int16_t *ptr = img, val;
    max = 0; min = 65535;
    for(i = 0; i < size; i++, ptr++){
        val = *ptr;
        pv = (double) val;
        sum += pv;
        sum2 += (pv * pv);
        if(max < val) max = val;
        if(min > val) min = val;
        if(val >= 65530) Noverld++;
    }
    // ���������� �� �����������:\n
    printf(_("Image stat:\n"));
    avr = sum/sz;
    printf("avr = %.1f, std = %.1f, Noverload = %ld\n", avr,
        std = sqrt(fabs(sum2/sz - avr*avr)), Noverld);
    printf("max = %u, min = %u, size = %ld\n", max, min, size);
}
