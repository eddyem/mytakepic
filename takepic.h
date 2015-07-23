#ifndef __TAKEPIC_H__
#define __TAKEPIC_H__
#define _XOPEN_SOURCE 501
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <fitsio.h>
#include <libintl.h>

#ifdef USEPNG
#include <png.h>
#endif /* USEPNG */

#include "libfli.h"

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "takepic"
#endif
#ifndef LOCALEDIR
#define LOCALEDIR "./locale"
#endif

#define _(String)				gettext(String)
#define gettext_noop(String)	String
#define N_(String)				gettext_noop(String)

// режим отладки, -DEBUG
#ifdef EBUG
	#define RED			"\033[1;32;41m"
	#define GREEN		"\033[5;30;42m"
	#define OLDCOLOR	"\033[0;0;0m"
	#define FNAME() fprintf(stderr, "\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__)
	#define DBG(...) do{fprintf(stderr, "%s (%s, line %d): ", __func__, __FILE__, __LINE__); \
					fprintf(stderr, __VA_ARGS__);			\
					fprintf(stderr, "\n");} while(0)
#else
	#define FNAME()	 do{}while(0)
	#define DBG(...) do{}while(0)
#endif //EBUG

extern const char *__progname;
#define info(format, args...)	do{		\
	printf("%s: ", __progname);		\
	printf(format,  ## args);		\
	printf("\n");}while(0)

#define warnc(c, format, args...)	\
	warnx(format ": %s", ## args, strerror(c))

long r;
#define TRYFUNC(f, ...)				\
do{	if((r = f(__VA_ARGS__)))		\
		warnc(-r, #f "() failed");	\
}while(0)

#define LIBVERSIZ 1024

typedef struct{
	flidomain_t domain;
	char *dname;
	char *name;
}cam_t;

void findcams(flidomain_t domain, cam_t **cam);
#ifdef USERAW
int writeraw(char *filename, int width, int height, void *data);
#endif // USERAW

#define TRYFITS(f, ...)						\
do{	int status = 0;							\
	f(__VA_ARGS__, &status);				\
	if (status){							\
		fits_report_error(stderr, status);	\
		return -1;}							\
}while(0)
#define WRITEKEY(...)							\
do{ int status = 0;								\
	fits_write_key(__VA_ARGS__, &status);		\
	if(status) fits_report_error(stderr, status);\
}while(0)
int writefits(char *filename, int width, int height, void *data);


#endif // __TAKEPIC_H__
