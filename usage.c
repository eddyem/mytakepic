#include "usage.h"

char
	 *objname = NULL	// ��� �������
	,*outfile = NULL	// ������� ����� ����� ��� ������ raw/fits
	,*objtype = NULL	// ��� ����� (�� ��������� - object, ��� -d - dark)
	,*instrument = NULL	// �������� ������� (�� ��������� - "direct imaging")
	,*observers = NULL	// �����������
	,*prog_id = NULL	// ��� ���������
	,*author = NULL		// �����
;
int
		 exptime = 500	// ����� ���������� (� ��)
		,pics = 1		// ���-�� ��������� ������
		,hbin = 1		// �������������� �������
		,vbin = 1		// ������������ �������
		,X0=-1,Y0=-1	// ���������� �������� ������ ���� ������������ �����������
		// -1 - ��� �������, � �.�. "��������"
		,X1=-1,Y1=-1	// ���������� ������� ������� ���� ������������ �����������
		,flushes = 1	// ���-�� �������
		,pause_len = 0	// ����������������� ����� (� �) ����� �������
;
double temperature = -20.;	// ����������� (������� ������, ����� - �� ������ ����.)

fliframe_t frametype = FLI_FRAME_TYPE_NORMAL;	// ��� ������ (������� ��� ��������)

bool
	 only_T			= FALSE	// ������ ���������� ����������� - � �����
	,set_T			= FALSE	// ������ ������ �����������
	,save_Tlog		= FALSE	// ��������� ������ ����������
	,save_image		= TRUE	// ��������� �����������
	,stat_logging		= FALSE	// ������ ������������ ����������
;
int myatoi(int *num, const char *str){ // "����������" atoi
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
		// "�������� ������ �������: %s"
		usage(_("Wrong lower border: %s"), pair);
	}
	if(a){
		if(myatoi(X1, a) || *X1 < 0 || *X1 <= *X0){
			// "�������� ������� �������: %s"
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
	// "�������������:\t%s [�����] <������� �������� ������>\n"
	printf(_("Usage:\t%s [options] <output files prefix>\n"),
		__progname);
	// "\t�����:\n"
	printf(_("\tOptions:\n"));
	printf("\t-A,\t--author=author\t\t%s\n",
		// "����� ���������"
		_("program author"));
	printf("\t-d,\t--dark\t\t\t%s\n",
		// "�� ��������� ������ ��� ���������� (\"��������\")"
		_("not open shutter, when exposing (\"dark frames\")"));
	printf("\t-f,\t--flushes=N\t\t%s\n",
		// "N ������� ����� �����������"
		_("N flushes before exposing"));
	printf("\t-h,\t--hbin=N\t\t%s\n",
		// "������� N �������� �� �����������"
		_("horizontal binning to N pixels"));
	printf("\t-I,\t--image-type=type\t%s\n",
		// "��� �����������"
		_("image type"));
	printf("\t-i,\t--instrument=instr\t%s\n",
		// "�������� �������"
		_("instrument name"));
	printf("\t-L,\t--log-only\t\t%s\n",
		// "�� ��������� �����������, ���� ����� ������ ���������"
		_("don't save images, only make all-stat log"));
	printf("\t-l,\t--tlog\t\t\t%s\n",
		// "����� ������ ������� ���������� � ���� temp_log"
		_("make temperatures logging to file temp_log"));
	printf("\t-n,\t--nframes=N\t\t%s\n",
		// "N ������ � �����"
		_("make series of N frames"));
	printf("\t-O,\t--object=obj\t\t%s\n",
		// "�������� �������"
		_("object name"));
	printf("\t-o,\t--observer=obs\t\t%s\n",
		// "����� ������������"
		_("observers' names"));
	printf("\t-P,\t--prog-id=prname\t%s\n",
		// "�������� ��������� ����������"
		_("observing program name"));
	printf("\t-p,\t--pause-len=ptime\t%s\n",
		// "��������� ptime ������ ����� ������������"
		_("make pause for ptime seconds between expositions"));
	printf("\t-s,\t--only-stat\t\t%s\n",
		// "�� ��������� �����������, � ������ ���������� ����������"
		_("not save image, just show statistics"));
	printf("\t-T,\t--only-temp\t\t%s\n",
		// "������ ������/�������� �����������"
		_("only set/get temperature"));
	printf("\t-t,\t--set-temp=degr\t\t%s\n",
		// "������ ������� ����������� degr ��������"
		_("set work temperature to degr C"));
	printf("\t-v,\t--vbin=N\t\t%s\n",
		// "������� N �������� �� ���������"
		_("vertical binning to N pixels"));
	printf("\t-x,\t--exp=exptime\t\t%s\n",
		// "����� ���������� exptime ��"
		_("set exposure time to exptime ms"));
	printf("\t-X,\t--xclip=X0[,X1]\t\t%s [X0:X1]\n",
		// "������� �������� ��� ����������"
		_("select clip region"));
	printf("\t-Y,\t--xclip=Y0[,Y1]\t\t%s [Y0:Y1]\n",
		// "������� �������� ��� ����������"
		_("select clip region"));
	exit(0);
}

void parse_args(int argc, char **argv){
	int i;
	char short_options[] = "A:df:h:I:i:Lln:O:o:P:p:sTt:v:x:X:Y:";
	struct option long_options[] = {
/*		{ name, has_arg, flag, val }, ���:
 * name - ��� "��������" ���������
 * has_arg = 0 - ��� ���������, 1 - ������������ ��������, 2 - �������������� ��������
 * flag = NULL ��� �������� val, ��������� �� ���������� int - ��� ���������� ��
 * 		�������� val (� ���� ������ ������� ���������� 0)
 * val - ������������ �������� getopt_long ��� ��������, ���������� ��������� flag
 * !!! ��������� ������ - ������ ����
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
/*	"�������" � "��������" ��������� getopt_long �� ������ ���������
 * (���� ���� "�������" ������ ������� �� ��, ��� "��������", ��� ����
 * ������ ���� flag = NULL, val = �������� ���������
 */
	while (1){
		int opt;
		if((opt = getopt_long(argc, argv, short_options,
			long_options, NULL)) == -1) break;
		switch(opt){
		case 'A':
			author = strdup(optarg);
			// "����� ���������: %s"
			info(_("Program author: %s"), author);
			break;
		case 'd':
			frametype = FLI_FRAME_TYPE_DARK;
			// "������ ��������"
			info(_("Dark frames"));
			break;
		case 'f':
			if (myatoi(&flushes, optarg) || flushes < 0){
				// "�������� ���-�� �������: %s"
				usage("Wrong flushes number: %s", optarg);
			}
			// "���-�� �������: %d"
			info("Flushes number: %d", flushes);
			break;
		case 'h':
			if (myatoi(&hbin, optarg) || hbin < 1 || hbin > 16){
				// "��������"
				usage("%s hbin: %s", _("Wrong"), optarg);
			}
			// "���. �������: %d"
			info(_("Horisontal binning: %d"), hbin);
			break;
		case 'I':
			objtype = strdup(optarg);
			// "��� ����������� - %s"
			info(_("Image type - %s"), objtype);
			break;
		case 'i':
			instrument = strdup(optarg);
			// "�������� ������� - %s"
			info(_("Instrument name - %s"), instrument);
			break;
		case 'L':
			stat_logging = TRUE;
			save_image = FALSE;
			// ������ �������������� ���������� ��� ���������� �����������
			info(_("Full statistics logging without saving images"));
			break;
		case 'l':
			save_Tlog = TRUE;
			// "���������� ������� ����������"
			info(_("Save temperature log"));
			break;
		case 'n':
			if (myatoi(&pics, optarg) || pics <= 0){
				// "�������� ���-�� ������: %s"
				usage(_("Wrong frames number in series: %s"), optarg);
			}
			// "����� �� %d ������"
			info(_("Series of %d frames"), pics);
			break;
		case 'O':
			objname = strdup(optarg);
			// "��� ������� - %s"
			info(_("Object name - %s"), objname);
			break;
		case 'o':
			observers = strdup(optarg);
			// "�����������: %s"
			info(_("Observers: %s"), observers);
			break;
		case 'P':
			prog_id = strdup(optarg);
			// "�������� ���������: %s"
			info(_("Program name: %s"), prog_id);
			break;
		case 'p':
			if (myatoi(&pause_len, optarg) || pause_len < 0){
				// "�������� �����: %s"
				usage(_("Wrong pause length: %s"), optarg);
			}
			// "�����: %d�"
			info(_("Pause: %ds"), pause_len);
			break;
		case 's':
			save_image = FALSE;
			break;
		case 'T':
			only_T = TRUE;
			save_image = FALSE;
			// "������ ����������/������ �����������"
			info(_("only set/get temperature"));
			break;
		case 't':
			temperature = atof(optarg);
			if(temperature < -55. || temperature > 30.){
				// "�������� �������� �����������: %s (������ ���� �� -55 �� 30)"
				usage(_("Wrong temperature: %s (must be from -55 to 30)"), optarg);
			}
			set_T = TRUE;
			// "���������� �����������: %.3f"
			info(_("Set temperature: %.3f"), temperature);
			break;
		case 'v':
			if (myatoi(&vbin, optarg) || vbin < 1 || vbin > 16){
				// "��������"
				usage("%s vbin: %s", _("Wrong"), optarg);
			}
			// "����. �������: %d"
			info(_("Vertical binning: %d"), vbin);
			break;
		case 'x':
			if (myatoi(&exptime, optarg) || exptime < 0){
				// "�������� ����� ����������: %s"
				usage(_("Wrong exposure time: %s"), optarg);
			}
			// "����� ����������: %d��"
			info(_("Exposure time: %dms"), exptime);
			break;
		case 'X':
			getrange(&X0, &X1, optarg);
			// "�������� �� X: [%d, %d]"
			info(_("X range: [%d, %d]"), X0, X1);
			break;
		case 'Y':
			getrange(&Y0, &Y1, optarg);
			// "�������� �� Y: [%d, %d]"
			info(_("Y range: [%d, %d]"), Y0, Y1);
			break;
		default:
		usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;
	if(argc == 0 && save_image){
		// "��� �������� ����� �������� ������"
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
		// "��������� ��������[�]:\n"
		printf(_("Ignore argument[s]:\n"));
	}
	for (i = 0; i < argc; i++)
		warnx("%s ", argv[i]);
}
