#include "takepic.h"
#include "usage.h"
#ifdef USE_BTA
#include "bta_print.h"
#endif
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data);
#endif /* USEPNG */

#define BUFF_SIZ 4096

#define TMBUFSIZ 40 // длина буфера для строки с временем
char tm_buf[TMBUFSIZ];	// буфер для строки с временем

u_int16_t max=0, min=65535; // экстремальные значения текущего изображения
double avr, std; // среднее значение и среднеквадратическое отклонение текущего изобр.
char *camera = NULL, viewfield[80];
double pixX, pixY; // размер пикселя

int numcams = 0;

void print_stat(u_int16_t *img, long size, FILE* f);

int itime(){ // усл. время в секундах
	struct timeval ct;
	gettimeofday(&ct, NULL);
	return (ct.tv_sec);
}
int time0;
int ltime(){ // время, прошедшее с момента запуска программы
	return(itime()-time0);
}

size_t curtime(char *s_time){ // строка - текущее время/дата
	time_t tm = time(NULL);
	return strftime(s_time, TMBUFSIZ, "%d/%m/%Y,%H:%M:%S", localtime(&tm));
}

double t_ext, t_int;	// внешняя т., т. камеры (+ на конец экспозиции)
time_t expStartsAt;		// время начала экспозиции (time_t)

int check_filename(char *buff, char *outfile, char *ext){
	struct stat filestat;
	int num;
	for(num = 1; num < 10000; num++){
		if(snprintf(buff, BUFF_SIZ, "%s_%04d.%s", outfile, num, ext) < 1)
			return 0;
		if(stat(buff, &filestat)) // файла не существует
			return 1;
	}
	return 0;
}

int main(int argc, char **argv){
	int i;			// для циклов
	FILE *f_tlog = NULL;// файл, в который будет записываться журнал температур
	FILE *f_statlog = NULL; // файл для статистики в режиме "только статистика"
	char libver[LIBVERSIZ];	// буфер для версии библиотеки fli
	cam_t *cam = NULL;		// список камер
	setlocale(LC_ALL, getenv("LC_ALL"));
	setlocale(LC_NUMERIC, "C");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	//bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	parse_args(argc, argv);
	//TRYFUNC(FLISetDebugLevel, NULL /* "NO HOST" */, FLIDEBUG_ALL);
	TRYFUNC(FLISetDebugLevel, NULL, FLIDEBUG_NONE);
	TRYFUNC(FLIGetLibVersion, libver, LIBVERSIZ);
	// Версия библиотеки '%s'
	info(_("Library version '%s'"), libver);
	findcams(FLIDOMAIN_USB, &cam);
	if(save_Tlog){
		f_tlog = fopen("temp_log", "a");
		// Не могу открыть файл журнала температур
		if(!f_tlog) err(1, _("Can't open temperature log file"));
		fprintf(f_tlog, "\n\n\n");
	}
	if(stat_logging){
		struct stat s;
		char print_hdr = 1;
		if(stat("stat_log", &s) == 0) print_hdr = 0;
		f_statlog = fopen("stat_log", "a");
		// Не могу открыть файл журнала статистики
		if(!f_statlog) err(1, _("Can't open statistics log file"));
		if(print_hdr)
			fprintf(f_statlog, "Time\t\t\tUnix time\tTexp\tTint\tText\tImax\tImin\tIavr\tIstd\tNover\tN>3std\tIavr3\t\tIstd3\n");
	}
	i = 0;
	for (i = 0; i < numcams; i++){
		long x0,x1, y0,y1, row, img_rows, row_width, ltmp, img_size;
		flidev_t dev;
		char buff[BUFF_SIZ];
		u_int16_t *img;
		int j;
		// Камера '%s' из домена %s
		info(_("Camera '%s', domain %s"), cam[i].name, cam[i].dname);
		TRYFUNC(FLIOpen, &dev, cam[i].name, FLIDEVICE_CAMERA | cam[i].domain);
		if(r) continue;
		TRYFUNC(FLIGetModel, dev, buff, BUFF_SIZ);
		// Модель:\t\t%s
		info(_("Model:\t\t%s"), buff);
		camera = strdup(buff);
		TRYFUNC(FLIGetHWRevision, dev, &ltmp);
		// Апп. версия: %ld
		info(_("HW revision: %ld"), ltmp);
		TRYFUNC(FLIGetFWRevision, dev, &ltmp);
		// Прогр. версия: %ld
		info(_("SW revision: %ld"), ltmp);
		TRYFUNC(FLIGetPixelSize, dev, &pixX, &pixY);
		// Размер пикселя: %g x %g
		info(_("Pixel size: %g x %g"), pixX, pixY);
		TRYFUNC(FLIGetVisibleArea, dev, &x0, &y0, &x1, &y1);
		snprintf(viewfield, 79, "(%ld, %ld)(%ld, %ld)", x0, y0, x1, y1);
		// Видимое поле: %s
		info(_("Field of view: %s"), viewfield);
		if(X1 > x1) X1 = x1;
		if(Y1 > y1) Y1 = y1;
		TRYFUNC(FLIGetArrayArea, dev, &x0, &y0, &x1, &y1);
		// Поле изображения: (%ld, %ld)(%ld, %ld)
		info(_("Array field: (%ld, %ld)(%ld, %ld)"), x0, y0, x1, y1);
		TRYFUNC(FLISetHBin, dev, hbin);
		TRYFUNC(FLISetVBin, dev, vbin);
		if(X0 == -1) X0 = x0; // задаем значения по умолчанию
		if(Y0 == -1) Y0 = y0;
		if(X1 == -1) X1 = x1;
		if(Y1 == -1) Y1 = y1;
		row_width = (X1 - X0) / hbin;
		img_rows = (Y1 - Y0) / vbin;
		//TRYFUNC(FLISetImageArea, dev, ltmp, tmp2, tmp3, tmp4);
		TRYFUNC(FLISetImageArea, dev, X0, Y0,
			X0 + (X1 - X0) / hbin, Y0 + (Y1 - Y0) / vbin);
		TRYFUNC(FLISetNFlushes, dev, flushes);
		if(set_T) TRYFUNC(FLISetTemperature, dev, temperature);
		TRYFUNC(FLIGetTemperature, dev, &t_int);
		// Температура (внутр.): %f
		info(_("Inner temperature: %f"), t_int);
		TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
		// Температура (внешн.): %f
		info(_("Outern temperature: %f"), t_ext);
		if(only_T) continue;
		TRYFUNC(FLISetExposureTime, dev, exptime);
		TRYFUNC(FLISetFrameType, dev, frametype);
		//TRYFUNC(FLISetBitDepth, dev, FLI_MODE_16BIT);
		img_size = img_rows * row_width * sizeof(u_int16_t);
		if((img = malloc(img_size)) == NULL) err(1, "malloc() failed");
		for (j = 0; j < pics; j ++){
			TRYFUNC(FLIGetTemperature, dev, &temperature); // температура до начала экспозиции
			printf("\n\n");
			// Захват кадра %d\n
			printf(_("Capture frame %d\n"), j);
			TRYFUNC(FLIExposeFrame, dev);
			expStartsAt = time(NULL); // время начала экспозиции
			if(save_Tlog) if(curtime(tm_buf))
				// Начинаю экспозицию %dмс, время: %s, файл: %s.%d.%d\n
				fprintf(f_tlog, _("Begin exposition %dms, exptime: %s, filename: %s.%d.%d\n"),
					exptime, tm_buf, outfile, i, j);
			do{
				TRYFUNC(FLIGetExposureStatus, dev, &ltmp);
				if(r) break;
				// %.3f секунд до окончания экспозиции\n
				printf(_("%.3f seconds till exposition ends\n"), ((float)ltmp) / 1000.);
				TRYFUNC(FLIGetTemperature, dev, &t_int);
				TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
				if(curtime(tm_buf)){
					// дата/время
					printf("%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
					if(save_Tlog) fprintf(f_tlog, "%s\t\t%.5f\t\t%.5f\n", tm_buf, t_ext, t_int);
				}
				else info("curtime() error");
				if(ltmp > 10000) sleep(10);
				else usleep(ltmp * 1000);
			}while(ltmp);
			// Считывание изображения: 
			printf(_("Read image: "));
			int portion = 0;
			for (row = 0; row < img_rows; row++){
				TRYFUNC(FLIGrabRow, dev, &img[row * row_width], row_width);
				if(r) break;
				int progress = (int)(((float)row / (float)img_rows) * 100.);
				if(progress/5 > portion){
					if((++portion)%2) printf("..");
					else printf("%d%%", portion*5);
					fflush(stdout);
				}
			}
			printf("100%%\n");
			curtime(tm_buf);
			if(f_statlog)
				fprintf(f_statlog, "%s\t%ld\t%g\t%.2f\t%.2f\t", tm_buf, time(NULL), exptime/1000., t_int, t_ext);
			print_stat(img, row_width * img_rows, f_statlog);
			inline void WRITEIMG(int (*writefn)(char*,int,int,void*), char *ext){
				if(!check_filename(buff, outfile, ext))
					// Не могу сохранить файл
					err(1, _("Can't save file"));
				else{
					TRYFUNC(writefn, buff, row_width, img_rows, img);
					// Файл записан в '%s'
					if (r == 0) info(_("File saved as '%s'"), buff);
				}
			}
			if(save_image){
				#ifdef USERAW
				WRITEIMG(writeraw, "raw");
				#endif // USERAW
				WRITEIMG(writefits, "fit");
				#ifdef USEPNG
				WRITEIMG(writepng, "png");
				#endif /* USEPNG */
			}
			if(pause_len){
				int delta;
				time0 = itime();
				while((delta = pause_len - ltime()) > 0){
				// %d секунд до окончания паузы\n
				printf(_("%d seconds till pause ends\n"), delta);
				TRYFUNC(FLIGetTemperature, dev, &t_int);
				TRYFUNC(FLIReadTemperature, dev, FLI_TEMPERATURE_EXTERNAL, &t_ext);
				if(curtime(tm_buf)){
					// дата/время
					printf("%s: %s\tText=%.2f\tTint=%.2f\n", _("date/time"), tm_buf, t_ext, t_int);
					if(save_Tlog) fprintf(f_tlog, "%s\t\t%.5f\t\t%.5f\n", tm_buf, t_ext, t_int);
				}
				else info("curtime() error");
				if(delta > 10) sleep(10);
				else sleep(delta);
				}
			}
		}
		free(camera);
		free(img);
		TRYFUNC(FLIClose, dev);
	}
	for (i = 0; i < numcams; i++)
	free(cam[i].name);
	free(cam);
	if(f_tlog) fclose(f_tlog);
	if(f_statlog) fclose(f_statlog);
	exit(0);
}

void findcams(flidomain_t domain, cam_t **cam){
	long r;
	char **tmplist;
	TRYFUNC(FLIList, domain | FLIDEVICE_CAMERA, &tmplist);
	if (tmplist != NULL && tmplist[0] != NULL){
		int i, cams = 0;
		for (i = 0; tmplist[i] != NULL; i++) cams++;
		if ((*cam = realloc(*cam, (numcams + cams) * sizeof(cam_t))) == NULL)
			err(1, "realloc() failed");
		for (i = 0; tmplist[i] != NULL; i++)    {
			int j;
			cam_t *tmpcam = *cam + i;
			for (j = 0; tmplist[i][j] != '\0'; j++)
				if (tmplist[i][j] == ';'){
					tmplist[i][j] = '\0';
					break;
				}
			tmpcam->domain = domain;
			switch (domain){
			case FLIDOMAIN_PARALLEL_PORT:
				tmpcam->dname = "parallel port";
				break;
			case FLIDOMAIN_USB:
				tmpcam->dname = "USB";
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
			tmpcam->name = strdup(tmplist[i]);
		}
		numcams += cams;
	}
	// Камеры не найдены!\n
	else info(_("No cameras found!\n"));
	TRYFUNC(FLIFreeList, tmplist);
	return;
}

#ifdef USERAW
int writeraw(char *filename, int width, int height, void *data){
	int fd, size, err;
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )) == -1){
		warn("open(%s) failed", filename);
		return -errno;
	}
	size = width * height * sizeof(u_int16_t);
	if ((err = write(fd, data, size)) != size){
		warn("write() failed");
		err = -errno;
	}
	else err = 0;
	close(fd);
	return err;
}
#endif // USERAW

/*
 * В шапке должны быть:
 * RATE / Readout rate (KPix/sec)
 * GAIN / gain, e/ADU
 * SEEING / Seeing ('')
 * IMSCALE / Image scale (''/pix x ''/pix)
 * CLOUDS / Clouds (%)
 */


int writefits(char *filename, int width, int height, void *data){
	long naxes[2] = {width, height}, startTime;
	double tmp = 0.0;
	struct tm *tm_starttime;
	char buf[80];
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
	if(instrument){
		WRITEKEY(fp, TSTRING, "INSTRUME", instrument, "Instrument");
	}else
		WRITEKEY(fp, TSTRING, "INSTRUME", "direct imaging", "Instrument");
	// BZERO / zero point in scaling equation
	//WRITEKEY(fp, TDOUBLE, "BZERO", &tmp, "zero point in scaling equation");
	// BSCALE / linear factor in scaling equation
	//tmp = 1.0; WRITEKEY(fp, TDOUBLE, "BSCALE", &tmp, "linear factor in scaling equation");
	snprintf(buf, 79, "%.g x %.g", pixX, pixY);
	// PXSIZE / pixel size
	WRITEKEY(fp, TSTRING, "PXSIZE", buf, "Pixel size in m");
	WRITEKEY(fp, TSTRING, "VIEW_FIELD", viewfield, "Camera field of view");
	// CRVAL1, CRVAL2 / Offset in X, Y
	if(X0) WRITEKEY(fp, TINT, "CRVAL1", &X0, "Offset in X");
	if(Y0) WRITEKEY(fp, TINT, "CRVAL2", &Y0, "Offset in Y");
	if(exptime == 0) sprintf(buf, "bias");
	else if(frametype == FLI_FRAME_TYPE_DARK) sprintf(buf, "dark");
	else if(objtype) strncpy(buf, objtype, 79);
	else sprintf(buf, "object");
	// IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
	WRITEKEY(fp, TSTRING, "IMAGETYP", buf, "Image type");
	// DATAMAX, DATAMIN / Max,min pixel value
	WRITEKEY(fp, TUSHORT, "DATAMAX", &max, "Max pixel value");
	WRITEKEY(fp, TUSHORT, "DATAMIN", &min, "Min pixel value");
	WRITEKEY(fp, TDOUBLE, "DATAAVR", &avr, "Average pixel value");
	WRITEKEY(fp, TDOUBLE, "DATASTD", &std, "Standart deviation of pixel value");
	WRITEKEY(fp, TDOUBLE, "TEMP0", &temperature, "Camera temperature at exp. start (degr C)");
	WRITEKEY(fp, TDOUBLE, "TEMP1", &t_int, "Camera temperature at exp. end (degr C)");
	WRITEKEY(fp, TDOUBLE, "TEMPBODY", &t_ext, "Camera body temperature at exp. end (degr C)");
	tmp = (temperature + t_int) / 2. + 273.15;
	// CAMTEMP / Camera temperature (K)
	WRITEKEY(fp, TDOUBLE, "CAMTEMP", &tmp, "Camera temperature (K)");
	tmp = (double)exptime / 1000.;
	// EXPTIME / actual exposition time (sec)
	WRITEKEY(fp, TDOUBLE, "EXPTIME", &tmp, "actual exposition time (sec)");
	// DATE / Creation date (YYYY-MM-DDThh:mm:ss, UTC)
	strftime(buf, 79, "%Y-%m-%dT%H:%M:%S", gmtime(&savetime));
	WRITEKEY(fp, TSTRING, "DATE", buf, "Creation date (YYYY-MM-DDThh:mm:ss, UTC)");
	startTime = (long)expStartsAt;
	tm_starttime = localtime(&expStartsAt);
	strftime(buf, 79, "exposition starts at %d/%m/%Y, %H:%M:%S (local)", tm_starttime);
	WRITEKEY(fp, TLONG, "UNIXTIME", &startTime, buf);
	strftime(buf, 79, "%Y/%m/%d", tm_starttime);
	// DATE-OBS / DATE (YYYY/MM/DD) OF OBS.
	WRITEKEY(fp, TSTRING, "DATE-OBS", buf, "DATE OF OBS. (YYYY/MM/DD, local)");
	strftime(buf, 79, "%H:%M:%S", tm_starttime);
	// START / Measurement start time (local) (hh:mm:ss)
	WRITEKEY(fp, TSTRING, "START", buf, "Measurement start time (hh:mm:ss, local)");
	// OBJECT  / Object name
	if(objname){
		WRITEKEY(fp, TSTRING, "OBJECT", objname, "Object name");
	}
	// BINNING / Binning
	if(hbin != 1 || vbin != 1){
		snprintf(buf, 79, "%d x %d", hbin, vbin);
		WRITEKEY(fp, TSTRING, "BINNING", buf, "Binning (hbin x vbin)");
	}
	// OBSERVER / Observers
	if(observers){
		WRITEKEY(fp, TSTRING, "OBSERVER", observers, "Observers");
	}
	// PROG-ID / Observation program identifier
	if(prog_id){
		WRITEKEY(fp, TSTRING, "PROG-ID", prog_id, "Observation program identifier");
	}
	// AUTHOR / Author of the program
	if(author){
		WRITEKEY(fp, TSTRING, "AUTHOR", author, "Author of the program");
	}
#ifdef USE_BTA
	write_bta_data(fp);
#endif
	TRYFITS(fits_write_img, fp, TUSHORT, 1, width * height, data);
	TRYFITS(fits_close_file, fp);
	return 0;
}

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data){
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

void print_stat(u_int16_t *img, long size, FILE *f){
	long i, Noverld = 0L, N = 0L;
	double pv, sum=0., sum2=0., sz = (double)size, tres;
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
	// Статистика по изображению\n
	printf(_("Image stat:\n"));
	avr = sum/sz;
	printf("avr = %.1f, std = %.1f, Noverload = %ld\n", avr,
		std = sqrt(fabs(sum2/sz - avr*avr)), Noverld);
	printf("max = %u, min = %u, size = %ld\n", max, min, size);
	// полная статистика, если у нас режим сохранения статистики:
	if(!f) return;
	// Iсреднее, Iсигма, Nперекоп
	fprintf(f, "%d\t%d\t%.3f\t%.3f\t%ld\t", max, min, avr, std, Noverld);
	Noverld = 0L;
	ptr = img; sum = 0.; sum2 = 0.;
	tres = avr + 3. * std; // порог по максимуму - 3 сигмы
	for(i = 0; i < size; i++, ptr++){
		val = *ptr;
		pv = (double) val;
		if(pv > tres){
			Noverld++; // теперь это - кол-во слишком ярких пикселей
			continue;
		}
		sum += pv;
		sum2 += (pv * pv);
		N++;
	}
	if(N == 0L){
		fprintf(f, "err\n");
		return;
	}
	sz = (double)N;
	avr = sum/sz; std = sqrt(fabs(sum2/sz - avr*avr));
	// Nбольше3сигм, Iсреднее в пределах 3сигм, Iсигма в пределах 3сигм
	fprintf(f, "%ld\t%.3f\t%.3f\n", Noverld, avr, std);
	fflush(f);
	printf("Novr3 = %ld\n", Noverld);
}
