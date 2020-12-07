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

#include <fcntl.h>
#include <fitsio.h>
#include <limits.h>
#include <math.h>
#ifdef USEPNG
#include <png.h>
#include <zlib.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cmdlnopts.h"
#include "flifunc.h"
#ifdef IMAGEVIEW
#include "imageview.h"
#endif

#define TRYFUNC(f, ...)             \
do{ if((fli_err = f(__VA_ARGS__)))  \
        WARNX(#f "() failed");      \
}while(0)

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

static long fli_err;
static char *camera = NULL;
static double t_ext, t_int;    // external & CCD temperatures @exp. end
static time_t expStartsAt;     // exposition start time (time_t)
static long filterpos = -1;    // filter position
static long focuserpos = -1;   // focuser position

static int writefits(char *filename, int width, int height, void *data);
#ifdef USEPNG
static int writepng(char *filename, int width, int height, void *data);
#endif
#ifdef USERAW
static int writeraw(char *filename, int width, int height, void *data);
#endif // USERAW

typedef struct{
    flidomain_t domain;
    char *dname;
    char *name;
}cam_t;

#define TMBUFSIZ 40
static char tm_buf[TMBUFSIZ];  // buffer for string with time value

static uint16_t max = 0, min = 65535; // max/min values for given image
static double avr, std; // stat values
static char viewfield[80];
static double pixX, pixY; // pixel size in um

static void print_stat(u_int16_t *img, long size);

static size_t curtime(char *s_time){ // current date/time
    time_t tm = time(NULL);
    return strftime(s_time, TMBUFSIZ, "%d/%m/%Y,%H:%M:%S", localtime(&tm));
}

static fliframe_t frametype = FLI_FRAME_TYPE_NORMAL;

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

// return 0 if OK
int fli_init(){
    char libver[LIBVERSIZ]; // FLI library version
    // #ifdef EBUG
     TRYFUNC(FLISetDebugLevel, NULL /* "NO HOST" */, FLIDEBUG_NONE);
   /*  #else
     TRYFUNC(FLISetDebugLevel, NULL, FLIDEBUG_...);
     #endif */
     if(fli_err){
         WARNX("Can't clear debug level");
         return 1;
     }
     TRYFUNC(FLIGetLibVersion, libver, LIBVERSIZ);
     // Версия библиотеки '%s'
     if(!fli_err) verbose(1, _("Library version '%s'"), libver);
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
    // Статистика по изображению:\n
    printf(_("Image stat:\n"));
    avr = sum/sz;
    printf("avr = %.1f, std = %.1f, Noverload = %ld\n", avr,
        std = sqrt(fabs(sum2/sz - avr*avr)), Noverld);
    printf("max = %u, min = %u, size = %ld\n", max, min, size);
}

/*
 * Find focusers and work with each of them
 */
void focusers(){
    flidev_t dev;
    int nfocs = 0;
    long ltmp;
    char buff[BUFF_SIZ];
    cam_t *cam = NULL;
    int num = findcams(FLIDOMAIN_USB | FLIDEVICE_FOCUSER, &cam);
    for(int i = 0; i < num; i++){
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        if(!fli_err){
            if(!strcasestr(buff, "focuser")){ // not focuser
                TRYFUNC(FLIClose, dev);
                continue;
            }
            // Модель:\t\t%s
            verbose(1, _("Model:\t\t%s"), buff);
        }
        ++nfocs;
        verbose(1, _("Focuser '%s', domain %s"), cam[i].name, cam[i].dname);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // Апп. версия: %ld
        if(!fli_err) verbose(1, _("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // Прогр. версия: %ld
        if(!fli_err) verbose(1, _("SW revision: %ld"), ltmp);
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
            if(GP->gotopos != INT_MAX && GP->addsteps != INT_MAX){
                // Нельзя одновременно указывать относительную и абсолютную позицию
                WARNX(_("You can't use both relative and absolute position"));
                break;
            }
            if(curpos < 0 || maxpos < 0){
                // Ошибка определения позиции
                WARNX(_("Error in position detection"));
                break;
            }
            long pos = -1, steps = 0;
            if(GP->gotopos != INT_MAX){ // absolute pointing
                pos = GP->gotopos;
                steps = pos - curpos;
            }else if(GP->addsteps != INT_MAX){ // relative pointing
                steps = GP->addsteps;
                pos = curpos + steps;
            }else break;
            if(!steps){
                verbose(1, _("Already at position"));
                break;
            }
            if(pos > maxpos || pos < 0){
                // Позиция не должна выходить за пределы 0...%ld
                WARNX(_("Position should be in 0...%ld"), maxpos);
                break;
            }
            if(pos == 0){
                // Перемещение в нулевую позицию
                verbose(1, _("Moving to home position"));
                if(GP->async) TRYFUNC(FLIHomeDevice, dev);
                else TRYFUNC(FLIHomeFocuser, dev);
            }else{
                // Перемещение на %ld шагов
                verbose(1, _("Moving for %ld steps"), steps);
                if(GP->async) TRYFUNC(FLIStepMotorAsync, dev, steps);
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
    for(int i = 0; i < num; i++) FREE(cam[i].name);
    FREE(cam);
}

/*
 * Find wheels and work with each of them
 */
void wheels(){
    cam_t *cam = NULL;
    flidev_t dev;
    long ltmp;
    char buff[BUFF_SIZ];
    int nwheels = 0;
    int num = findcams(FLIDOMAIN_USB | FLIDEVICE_FILTERWHEEL, &cam);
    for(int i = 0; i < num; i++){
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetFilterCount, dev, &ltmp);
        if(fli_err || ltmp < 2){// not a wheel
            TRYFUNC(FLIClose, dev);
            continue;
        }
        verbose(1, _("Wheel '%s', domain %s"), cam[i].name, cam[i].dname);
        green("WHEELTOTALPOS=%ld\n", ltmp);
        int wheelmaxpos = (int)ltmp - 1;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        // Модель:\t\t%s
        if(!fli_err) verbose(1, _("Model:\t\t%s"), buff);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // Апп. версия: %ld
        if(!fli_err) verbose(1, _("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // Прогр. версия: %ld
        if(!fli_err) verbose(1, _("SW revision: %ld"), ltmp);
        else goto closewheeldev;
        if(GP->setwheel > -1 && GP->setwheel >= ltmp){
            GP->setwheel = -1;
            WARNX(_("Wheel position should be from 0 to %ld"), ltmp - 1);
        }
        ++nwheels;
        if(GP->setwheel > -1){
            ltmp = GP->setwheel;
            if(ltmp > wheelmaxpos){
                WARNX(_("Position is too big (max %d)"), wheelmaxpos);
                goto closewheeldev;
            }
            TRYFUNC(FLISetFilterPos, dev, ltmp);
            if(!fli_err) verbose(1, _("Arrive to position"));
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
    for (int i = 0; i < num; i++)
        FREE(cam[i].name);
    FREE(cam);
}

/**
 * @brief grabImage - grab image into `ima`
 * @param ima (io) - prepared IMG structure with allocated data & initialized sizes
 * @param dev - device
 */
static void grabImage(IMG *ima, flidev_t dev){
    FNAME();
    long ltmp;
    TRYFUNC(FLIGetTemperature, dev, &GP->temperature); // temperature @ exp. start
    TRYFUNC(FLIExposeFrame, dev);
    expStartsAt = time(NULL); // время начала экспозиции
    do{
        TRYFUNC(FLIGetTemperature, dev, &t_int);
        TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
        if(curtime(tm_buf)){
            // дата/время
            verbose(1, "%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
        }
        else WARNX("curtime() error");
        TRYFUNC(FLIGetExposureStatus, dev, &ltmp);
        if(fli_err) continue;
        if(GP->shtr_cmd > 0 && GP->shtr_cmd & FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL && ltmp == GP->exptime){
            // "ожидание внешнего триггера"
            printf(_("wait for external trigger...\n"));
            sleep(1);
        }else{
            // %.3f секунд до окончания экспозиции\n
            printf(_("%.3f seconds till exposition ends\n"), ((float)ltmp) / 1000.);
            if(ltmp > 10000) sleep(10);
            else usleep(ltmp * 1000);
        }
    }while(ltmp);
    // Считывание изображения:
    printf(_("Read image: "));
    int portion = 0;
    for(int row = 0; row < ima->h; row++){
        TRYFUNC(FLIGrabRow, dev, &ima->data[row * ima->w], ima->w);
        if(fli_err) break;
        int progress = (int)(((float)row / (float)ima->h) * 100.);
        if(progress/5 > portion){
            if((++portion)%2) printf("..");
            else printf("%d%%", portion*5);
            fflush(stdout);
        }
    }
    printf("100%%\n");
    curtime(tm_buf);
    print_stat(ima->data, ima->h*ima->w);
}

/*
 * Find CCDs and work with each of them
 */
void ccds(){
#ifdef IMAGEVIEW
    windowData *mainwin = NULL;
    if(GP->showimage) imageview_init();
#endif
    cam_t *cam = NULL;
    flidev_t dev;
    long ltmp;
    char buff[BUFF_SIZ];
    int num = findcams(FLIDOMAIN_USB | FLIDEVICE_CAMERA, &cam);
    if(!num) WARNX(_("No CCD found"));
    for(int i = 0; i < num; i++){
        long x0,x1, y0,y1, img_rows, row_width;
        // Камера '%s' из домена %s
        verbose(1, _("Camera '%s', domain %s"), cam[i].name, cam[i].dname);
        TRYFUNC(FLIOpen, &dev, cam[i].name, cam[i].domain);
        if(fli_err) continue;
        TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
        // Модель:\t\t%s
        if(!fli_err) verbose(1, _("Model:\t\t%s"), buff);
        camera = strdup(buff);
        TRYFUNC(FLIGetHWRevision, dev, &ltmp);
        // Апп. версия: %ld
        if(!fli_err) verbose(1, _("HW revision: %ld"), ltmp);
        TRYFUNC(FLIGetFWRevision, dev, &ltmp);
        // Прогр. версия: %ld
        if(!fli_err) verbose(1, _("SW revision: %ld"), ltmp);
        TRYFUNC(FLIGetPixelSize, dev, &pixX, &pixY);
        // Размер пикселя: %g x %g
        if(!fli_err) verbose(1, _("Pixel size: %g x %g"), pixX, pixY);
        TRYFUNC(FLIGetVisibleArea, dev, &x0, &y0, &x1, &y1);
        snprintf(viewfield, 80, "(%ld, %ld)(%ld, %ld)", x0, y0, x1, y1);
        // Видимое поле: %s
        if(!fli_err) verbose(1, _("Field of view: %s"), viewfield);
        if(GP->X1 > x1) GP->X1 = x1;
        if(GP->Y1 > y1) GP->Y1 = y1;
        TRYFUNC(FLIGetArrayArea, dev, &x0, &y0, &x1, &y1);
        // Поле изображения: (%ld, %ld)(%ld, %ld)
        if(!fli_err) verbose(1, _("Array field: (%ld, %ld)(%ld, %ld)"), x0, y0, x1, y1);
        TRYFUNC(FLISetHBin, dev, GP->hbin);
        TRYFUNC(FLISetVBin, dev, GP->vbin);
        if(GP->X0 == -1) GP->X0 = x0; // default values
        if(GP->Y0 == -1) GP->Y0 = y0;
        if(GP->X1 == -1) GP->X1 = x1;
        if(GP->Y1 == -1) GP->Y1 = y1;
        row_width = (GP->X1 - GP->X0) / GP->hbin;
        img_rows = (GP->Y1 - GP->Y0) / GP->vbin;
        TRYFUNC(FLISetImageArea, dev, GP->X0, GP->Y0,
            GP->X0 + (GP->X1 - GP->X0) / GP->hbin, GP->Y0 + (GP->Y1 - GP->Y0) / GP->vbin);
        TRYFUNC(FLISetNFlushes, dev, GP->nflushes);
        if(GP->temperature < 40.){
            // "Установка температуры ПЗС: %g градусов Цельсия\n"
            green(_("Set CCD temperature to %g degr.C\n"), GP->temperature);
            TRYFUNC(FLISetTemperature, dev, GP->temperature);
        }
        TRYFUNC(FLIGetTemperature, dev, &t_int);
        if(!fli_err) green("CCDTEMP=%.1f\n", t_int);
        TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
        if(!fli_err) green("EXTTEMP=%.1f\n", t_ext);
        if(GP->shtr_cmd > -1){
            flishutter_t shtr = GP->shtr_cmd;
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
        }
        if(GP->confio > -1){
            // "Попытка сконфигурировать порт I/O как %d\n"
            green(_("Try to convfigure I/O port as %d\n"), GP->confio);
            TRYFUNC(FLIConfigureIOPort, dev, GP->confio);
        }
        if(GP->getio){
            long iop;
            TRYFUNC(FLIReadIOPort, dev, &iop);
            if(!fli_err) green("CCDIOPORT=0x%02lx\n", iop);
        }
        if(GP->setio > -1){
            // "Попытка записи %d в порт I/O\n"
            green(_("Try to write %d to I/O port\n"), GP->setio);
            TRYFUNC(FLIWriteIOPort, dev, GP->setio);
        }

        if(GP->exptime < DBL_EPSILON) continue;
        TRYFUNC(FLISetExposureTime, dev, GP->exptime);
        if(GP->dark) frametype = FLI_FRAME_TYPE_DARK;
        TRYFUNC(FLISetFrameType, dev, frametype);
        if(GP->_8bit){
            TRYFUNC(FLISetBitDepth, dev, FLI_MODE_8BIT);
            if(fli_err == 0) green(_("8 bit mode\n"));
        }
        TRYFUNC(FLISetCameraMode, dev, GP->fast ? 0 : 1);
        if(GP->fast) green(_("Fast readout mode\n"));
        if(!GP->outfile) red(_("Only show statistics\n"));
        uint16_t *img = MALLOC(uint16_t, img_rows * row_width);
        IMG ima = {.data = img, .w = row_width, .h = img_rows};
        for(int j = 0; j < GP->nframes; j ++){
            printf("\n\n");
            // Захват кадра %d\n
            printf(_("Capture frame %d\n"), j);
            grabImage(&ima, dev);
            saveImages(&ima, GP->outfile);
#ifdef IMAGEVIEW
            if(GP->showimage){ // display image
                if(!(mainwin = getWin())){
                    DBG("Create new win");
                    mainwin = createGLwin("Sample window", row_width, img_rows, NULL);
                    if(!mainwin){
                        WARNX("Can't open OpenGL window, image preview will be inaccessible");
                    }else
                        pthread_create(&mainwin->thread, NULL, &image_thread, (void*)&ima);
                }
                if((mainwin = getWin())){
                    DBG("change image");
                    change_displayed_image(mainwin, &ima);
                    while((mainwin = getWin())){ // test paused state & grabbing custom frames
                        if((mainwin->winevt & WINEVT_PAUSE) == 0) break;
                        if(mainwin->winevt & WINEVT_GETIMAGE){
                            mainwin->winevt &= ~WINEVT_GETIMAGE;
                            grabImage(&ima, dev);
                            change_displayed_image(mainwin, &ima);
                        }
                        usleep(10000);
                    }
                }
            }
#endif
            if(GP->pause_len && j != (GP->nframes - 1)){
                double delta, time1 = dtime() + GP->pause_len;
                while((delta = time1 - dtime()) > 0.){
                    // %d секунд до окончания паузы\n
                    printf(_("%d seconds till pause ends\n"), (int)delta);
                    TRYFUNC(FLIGetTemperature, dev, &t_int);
                    TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
                    if(curtime(tm_buf)){
                        // дата/время
                        verbose(1, "%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
                    }
                    else verbose(1, "curtime() error");
                    if(delta > 10) sleep(10);
                    else sleep((int)delta);
                }
            }
        }
#ifdef IMAGEVIEW
        if(GP->showimage){
            mainwin->winevt |= WINEVT_PAUSE;
            DBG("Waiting");
            while((mainwin = getWin())){
                if(mainwin->killthread) break;
                if(mainwin->winevt & WINEVT_GETIMAGE){
                    DBG("GRAB");
                    mainwin->winevt &= ~WINEVT_GETIMAGE;
                    grabImage(&ima, dev);
                    change_displayed_image(mainwin, &ima);
                }
            }
            DBG("Close window");
            usleep(10000);
        }
#endif
        FREE(camera);
        FREE(img);
        TRYFUNC(FLIClose, dev);
    }
    for(int i = 0; i < num; i++)
        FREE(cam[i].name);
    FREE(cam);
}

void saveImages(IMG *img, char *filename){
    inline void WRITEIMG(int (*writefn)(char*,int,int,void*), char *ext){
        char buff[BUFF_SIZ];
        if(filename == NULL) return;
        if(!check_filename(buff, filename, ext) && !GP->rewrite)
            // Не могу сохранить файл
            WARNX(_("Can't save file"));
        else{
            if(GP->rewrite){
                DBG("REW");
                char *p = "";
                if(strcmp(ext, "fit") == 0) p = "!";
                snprintf(buff, BUFF_SIZ, "%s%s.%s", p, filename, ext);
            }
            TRYFUNC(writefn, buff, img->w, img->h, (void*)img->data);
            // Файл записан в '%s'
            if (fli_err == 0) verbose(1, _("File saved as '%s'"), buff);
        }
    }
    #ifdef USERAW
    WRITEIMG(writeraw, "raw");
    #endif
    WRITEIMG(writefits, "fit");
    #ifdef USEPNG
    WRITEIMG(writepng, "png");
    #endif
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
    if(GP->instrument){
        WRITEKEY(fp, TSTRING, "INSTRUME", GP->instrument, "Instrument");
    }else
        WRITEKEY(fp, TSTRING, "INSTRUME", "direct imaging", "Instrument");
    snprintf(buf, 80, "%.g x %.g", pixX, pixY);
    // PXSIZE / pixel size
    WRITEKEY(fp, TSTRING, "PXSIZE", buf, "Pixel size in m");
    WRITEKEY(fp, TSTRING, "VIEWFLD", viewfield, "Camera field of view");
    // CRVAL1, CRVAL2 / Offset in X, Y
    if(GP->X0) WRITEKEY(fp, TINT, "X0", &GP->X0, "Subframe left border");
    if(GP->Y0) WRITEKEY(fp, TINT, "Y0", &GP->Y0, "Subframe upper border");
    if(GP->exptime < 2.*DBL_EPSILON) sprintf(buf, "bias");
    else if(frametype == FLI_FRAME_TYPE_DARK) sprintf(buf, "dark");
    else if(GP->objtype) strncpy(buf, GP->objtype, FLEN_CARD-1);
    else sprintf(buf, "object");
    // IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
    WRITEKEY(fp, TSTRING, "IMAGETYP", buf, "Image type");
    // DATAMAX, DATAMIN / Max,min pixel value
    int itmp = 0;
    WRITEKEY(fp, TINT, "DATAMIN", &itmp, "Min pixel value");
    //itmp = GP->fast ? 255 : 65535;
    itmp = 65535;
    WRITEKEY(fp, TINT, "DATAMAX", &itmp, "Max pixel value");
    WRITEKEY(fp, TUSHORT, "STATMAX", &max, "Max data value");
    WRITEKEY(fp, TUSHORT, "STATMIN", &min, "Min data value");
    WRITEKEY(fp, TDOUBLE, "STATAVR", &avr, "Average data value");
    WRITEKEY(fp, TDOUBLE, "STATSTD", &std, "Std. of data value");
    WRITEKEY(fp, TDOUBLE, "TEMP0", &GP->temperature, "Camera temperature at exp. start (degr C)");
    WRITEKEY(fp, TDOUBLE, "TEMP1", &t_int, "Camera temperature at exp. end (degr C)");
    WRITEKEY(fp, TDOUBLE, "TEMPBODY", &t_ext, "Camera body temperature at exp. end (degr C)");
    tmp = (GP->temperature + t_int) / 2. + 273.15;
    // CAMTEMP / Camera temperature (K)
    WRITEKEY(fp, TDOUBLE, "CAMTEMP", &tmp, "Camera temperature (K)");
    // WHEEL & FOCUSER positions:
    tmp = (double)focuserpos / FOCSCALE;
    WRITEKEY(fp, TDOUBLE, "FOCUS", &tmp, "Current focuser position, mm");
    if(filterpos > -1)
        WRITEKEY(fp, TINT, "FILTER", &filterpos, "Current filter number");
    // EXPTIME / actual exposition time (sec)
    tmp = (double)GP->exptime / 1000.;
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
    if(GP->objname){
        WRITEKEY(fp, TSTRING, "OBJECT", GP->objname, "Object name");
    }
    // BINNING / Binning
    if(GP->hbin != 1 || GP->vbin != 1){
        snprintf(buf, 80, "%d x %d", GP->hbin, GP->vbin);
        WRITEKEY(fp, TSTRING, "BINNING", buf, "Binning (hbin x vbin)");
    }
    // OBSERVER / Observers
    if(GP->observers){
        WRITEKEY(fp, TSTRING, "OBSERVER", GP->observers, "Observers");
    }
    // PROG-ID / Observation program identifier
    if(GP->prog_id){
        WRITEKEY(fp, TSTRING, "PROG-ID", GP->prog_id, "Observation program identifier");
    }
    // AUTHOR / Author of the program
    if(GP->author){
        WRITEKEY(fp, TSTRING, "AUTHOR", GP->author, "Author of the program");
    }
    if(GP->addhdr){ // add records from files
        char **nxtfile = GP->addhdr;
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


