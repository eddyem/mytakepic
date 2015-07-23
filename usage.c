#include "usage.h"

char
	 *objname = NULL	// имя объекта
	,*outfile = NULL	// префикс имени файла для записи raw/fits
	,*objtype = NULL	// тип кадра (по умолчанию - object, при -d - dark)
	,*instrument = NULL	// название прибора (по умолчанию - "direct imaging")
	,*observers = NULL	// наблюдатели
	,*prog_id = NULL	// имя программы
	,*author = NULL		// автор
;
int
		 exptime = 500	// время экспозиции (в мс)
		,pics = 1		// кол-во снимаемых кадров
		,hbin = 1		// горизонтальный биннинг
		,vbin = 1		// вертикальный биннинг
		,X0=-1,Y0=-1	// координаты верхнего левого угла считываемого изображения
		// -1 - вся область, в т.ч. "оверскан"
		,X1=-1,Y1=-1	// координаты правого нижнего угла считываемого изображения
		,flushes = 1	// кол-во сбросов
		,pause_len = 0	// продолжительность паузы (в с) между кадрами
;
double temperature = -20.;	// температура (которую задать, потом - на начало эксп.)

fliframe_t frametype = FLI_FRAME_TYPE_NORMAL;	// тип фрейма (обычный или темновой)

bool
	 only_T			= FALSE	// только отобразить температуру - и выйти
	,set_T			= FALSE	// задать нужную температуру
	,save_Tlog		= FALSE	// сохранять журнал температур
	,save_image		= TRUE	// сохранять изображение
	,stat_logging		= FALSE	// полное логгирование статистики
;
int myatoi(int *num, const char *str){ // "аккуратный" atoi
	long tmp;
	char *endptr;
	tmp = strtol(str, &endptr, 0);
	if (*str == '\0' || *endptr != '\0' || tmp < INT_MIN || tmp > INT_MAX){
		return -1;
	}
	*num = (int)tmp;
	return 0;
}

void getrange(int *X0, int *X1, char *arg){
	char *a = NULL, *pair;
	pair = strdup(arg);
	if((a = strchr(pair, ','))){
		*a = 0;
		a++;
	}
	if(myatoi(X0, pair) || *X0 < 0){
		// "Неверная нижняя граница: %s"
		usage(_("Wrong lower border: %s"), pair);
	}
	if(a){
		if(myatoi(X1, a) || *X1 < 0 || *X1 <= *X0){
			// "Неверная верхняя граница: %s"
			usage(_("Wrong upper border: %s"), pair);
		}
	}
	free(pair);
}

void usage(char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	printf("\n");
	if (fmt != NULL){
		vprintf(fmt, ap);
		printf("\n\n");
	}
	va_end(ap);
	// "Использование:\t%s [опции] <префикс выходных файлов>\n"
	printf(_("Usage:\t%s [options] <output files prefix>\n"),
		__progname);
	// "\tОпции:\n"
	printf(_("\tOptions:\n"));
	printf("\t-A,\t--author=author\t\t%s\n",
		// "автор программы"
		_("program author"));
	printf("\t-d,\t--dark\t\t\t%s\n",
		// "не открывать затвор при экспозиции (\"темновые\")"
		_("not open shutter, when exposing (\"dark frames\")"));
	printf("\t-f,\t--flushes=N\t\t%s\n",
		// "N сбросов перед экспозицией"
		_("N flushes before exposing"));
	printf("\t-h,\t--hbin=N\t\t%s\n",
		// "биннинг N пикселей по горизонтали"
		_("horizontal binning to N pixels"));
	printf("\t-I,\t--image-type=type\t%s\n",
		// "тип изображения"
		_("image type"));
	printf("\t-i,\t--instrument=instr\t%s\n",
		// "название прибора"
		_("instrument name"));
	printf("\t-L,\t--log-only\t\t%s\n",
		// "не сохранять изображения, лишь вести запись статистки"
		_("don't save images, only make all-stat log"));
	printf("\t-l,\t--tlog\t\t\t%s\n",
		// "вести запись рабочих температур в файл temp_log"
		_("make temperatures logging to file temp_log"));
	printf("\t-n,\t--nframes=N\t\t%s\n",
		// "N кадров в серии"
		_("make series of N frames"));
	printf("\t-O,\t--object=obj\t\t%s\n",
		// "название объекта"
		_("object name"));
	printf("\t-o,\t--observer=obs\t\t%s\n",
		// "имена наблюдателей"
		_("observers' names"));
	printf("\t-P,\t--prog-id=prname\t%s\n",
		// "название программы наблюдений"
		_("observing program name"));
	printf("\t-p,\t--pause-len=ptime\t%s\n",
		// "выдержать ptime секунд между экспозициями"
		_("make pause for ptime seconds between expositions"));
	printf("\t-s,\t--only-stat\t\t%s\n",
		// "не сохранять изображение, а только отобразить статистику"
		_("not save image, just show statistics"));
	printf("\t-T,\t--only-temp\t\t%s\n",
		// "только задать/получить температуру"
		_("only set/get temperature"));
	printf("\t-t,\t--set-temp=degr\t\t%s\n",
		// "задать рабочую температуру degr градусов"
		_("set work temperature to degr C"));
	printf("\t-v,\t--vbin=N\t\t%s\n",
		// "биннинг N пикселей по вертикали"
		_("vertical binning to N pixels"));
	printf("\t-x,\t--exp=exptime\t\t%s\n",
		// "время экспозиции exptime мс"
		_("set exposure time to exptime ms"));
	printf("\t-X,\t--xclip=X0[,X1]\t\t%s [X0:X1]\n",
		// "выбрать диапазон для считывания"
		_("select clip region"));
	printf("\t-Y,\t--xclip=Y0[,Y1]\t\t%s [Y0:Y1]\n",
		// "выбрать диапазон для считывания"
		_("select clip region"));
	exit(0);
}

void parse_args(int argc, char **argv){
	int i;
	char short_options[] = "A:df:h:I:i:Lln:O:o:P:p:sTt:v:x:X:Y:";
	struct option long_options[] = {
/*		{ name, has_arg, flag, val }, где:
 * name - имя "длинного" параметра
 * has_arg = 0 - нет аргумента, 1 - обязательный аргумент, 2 - необязательный аргумент
 * flag = NULL для возврата val, указатель на переменную int - для присвоения ей
 * 		значения val (в этом случае функция возвращает 0)
 * val - возвращаемое значение getopt_long или значение, присваемое указателю flag
 * !!! последняя строка - четыре нуля
 */
		{"author",		1,	0,	'A'},
		{"dark",		0,	0,	'd'},
		{"flushes",		1,	0,	'f'},
		{"hbin",		1,	0,	'h'},
		{"image-type",	1,	0,	'I'},
		{"instrument",	1,	0,	'i'},
		{"log-only",	0,	0,	'L'},
		{"tlog",		0,	0,	'l'},
		{"nframes",		1,	0,	'n'},
		{"object",		1,	0,	'O'},
		{"observers",	1,	0,	'o'},
		{"prog-id",		1,	0,	'P'},
		{"pause-len",	1,	0,	'p'},
		{"only-stat",	0,	0,	's'},
		{"only-temp",	0,	0,	'T'},
		{"set-temp",	1,	0,	't'},
		{"vbin",		1,	0,	'v'},
		{"exp",			1,	0,	'x'},
		{"xclip",		1,	0,	'X'},
		{"yclip",		1,	0,	'Y'},
		{ 0, 0, 0, 0 }
	};
/*	"длинные" и "короткие" параметры getopt_long не должны совпадать
 * (лишь если "длинный" должен значить то же, что "короткий", для него
 * должно быть flag = NULL, val = значение короткого
 */
	while (1){
		int opt;
		if((opt = getopt_long(argc, argv, short_options,
			long_options, NULL)) == -1) break;
		switch(opt){
		case 'A':
			author = strdup(optarg);
			// "Автор программы: %s"
			info(_("Program author: %s"), author);
			break;
		case 'd':
			frametype = FLI_FRAME_TYPE_DARK;
			// "Съемка темновых"
			info(_("Dark frames"));
			break;
		case 'f':
			if (myatoi(&flushes, optarg) || flushes < 0){
				// "Неверное кол-во сбросов: %s"
				usage("Wrong flushes number: %s", optarg);
			}
			// "Кол-во сбросов: %d"
			info("Flushes number: %d", flushes);
			break;
		case 'h':
			if (myatoi(&hbin, optarg) || hbin < 1 || hbin > 16){
				// "Неверный"
				usage("%s hbin: %s", _("Wrong"), optarg);
			}
			// "Гор. биннинг: %d"
			info(_("Horisontal binning: %d"), hbin);
			break;
		case 'I':
			objtype = strdup(optarg);
			// "Тип изображения - %s"
			info(_("Image type - %s"), objtype);
			break;
		case 'i':
			instrument = strdup(optarg);
			// "Название прибора - %s"
			info(_("Instrument name - %s"), instrument);
			break;
		case 'L':
			stat_logging = TRUE;
			save_image = FALSE;
			// Полное журналирование статистики без сохранения изображений
			info(_("Full statistics logging without saving images"));
			break;
		case 'l':
			save_Tlog = TRUE;
			// "Сохранение журнала температур"
			info(_("Save temperature log"));
			break;
		case 'n':
			if (myatoi(&pics, optarg) || pics <= 0){
				// "Неверное кол-во кадров: %s"
				usage(_("Wrong frames number in series: %s"), optarg);
			}
			// "Серия из %d кадров"
			info(_("Series of %d frames"), pics);
			break;
		case 'O':
			objname = strdup(optarg);
			// "Имя объекта - %s"
			info(_("Object name - %s"), objname);
			break;
		case 'o':
			observers = strdup(optarg);
			// "Наблюдатели: %s"
			info(_("Observers: %s"), observers);
			break;
		case 'P':
			prog_id = strdup(optarg);
			// "Название программы: %s"
			info(_("Program name: %s"), prog_id);
			break;
		case 'p':
			if (myatoi(&pause_len, optarg) || pause_len < 0){
				// "Неверная пауза: %s"
				usage(_("Wrong pause length: %s"), optarg);
			}
			// "Пауза: %dс"
			info(_("Pause: %ds"), pause_len);
			break;
		case 's':
			save_image = FALSE;
			break;
		case 'T':
			only_T = TRUE;
			save_image = FALSE;
			// "только отобразить/задать температуру"
			info(_("only set/get temperature"));
			break;
		case 't':
			temperature = atof(optarg);
			if(temperature < -55. || temperature > 30.){
				// "Неверное значение температуры: %s (должно быть от -55 до 30)"
				usage(_("Wrong temperature: %s (must be from -55 to 30)"), optarg);
			}
			set_T = TRUE;
			// "Установить температуру: %.3f"
			info(_("Set temperature: %.3f"), temperature);
			break;
		case 'v':
			if (myatoi(&vbin, optarg) || vbin < 1 || vbin > 16){
				// "Неверный"
				usage("%s vbin: %s", _("Wrong"), optarg);
			}
			// "Верт. биннинг: %d"
			info(_("Vertical binning: %d"), vbin);
			break;
		case 'x':
			if (myatoi(&exptime, optarg) || exptime < 0){
				// "Неверное время экспозиции: %s"
				usage(_("Wrong exposure time: %s"), optarg);
			}
			// "Время экспозиции: %dмс"
			info(_("Exposure time: %dms"), exptime);
			break;
		case 'X':
			getrange(&X0, &X1, optarg);
			// "Диапазон по X: [%d, %d]"
			info(_("X range: [%d, %d]"), X0, X1);
			break;
		case 'Y':
			getrange(&Y0, &Y1, optarg);
			// "Диапазон по Y: [%d, %d]"
			info(_("Y range: [%d, %d]"), Y0, Y1);
			break;
		default:
		usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;
	if(argc == 0 && save_image){
		// "Нет префикса имени выходных файлов"
		usage(_("Output file names prefix is absent"));
	}
	else{ if(argc != 0){
		outfile = argv[0];
		argc--;
		argv++;
		}
		else outfile = strdup("nofile");
	}
	if(argc > 0){
		// "Игнорирую аргумент[ы]:\n"
		printf(_("Ignore argument[s]:\n"));
	}
	for (i = 0; i < argc; i++)
		warnx("%s ", argv[i]);
}
