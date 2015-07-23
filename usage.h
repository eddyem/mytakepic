#ifndef __USAGE_H__
#define __USAGE_H__

#include "takepic.h"
#include <getopt.h>
#include <stdarg.h>

extern int
		 exptime		// время экспозиции (в мс)
		,pics			// кол-во снимаемых кадров
		,hbin			// горизонтальный биннинг
		,vbin			// вертикальный биннинг
		,X0, X1, Y0, Y1 // координаты выбранного окна
		,flushes		// кол-во сбросов
		,pause_len		// продолжительность паузы (в с) между кадрами
;
extern double temperature;	// температура (которую задать, потом - на начало эксп.)

extern fliframe_t frametype;	// тип фрейма (обычный или темновой)

extern bool
	 only_T			// только отобразить температуру - и выйти
	,set_T			// задать нужную температуру
	,save_Tlog		// сохранять журнал температур
	,save_image		// сохранять изображение
	,stat_logging	// полное логгирование статистики
;
extern char *objname	// имя объекта
	,*outfile			// префикс имени файла для записи raw/fits
	,*objtype			// тип изображения
	,*instrument		// название прибора (по умолчанию - "direct imaging")
	,*observers			// наблюдатели
	,*prog_id			// имя программы
	,*author			// автор
;
void usage(char *fmt, ...);
void parse_args(int argc, char **argv);

#endif // __USAGE_H__
