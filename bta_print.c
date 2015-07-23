/* Print some BTA NewACS data (or write  to file)
 * Usage:
 *         bta_print [time_step] [file_name]
 * Where:
 *         time_step - writing period in sec., >=1.0
 *                      <1.0 - once and exit (default)
 *         file_name - name of file to write to,
 *                      "-" - stdout (default)
 */
#include "bta_print.h"
#include "bta_shdata.h"


static char buf[1024];
char *time_asc(double t){
	int h, m;
	double s;
	h   = (int)(t/3600.);
	m = (int)((t - (double)h*3600.)/60.);
	s = t - (double)h*3600. - (double)m*60.;
	h %= 24;
	if(s>59) s=59;
	sprintf(buf, "%dh:%02dm:%04.1fs", h,m,s);
	return buf;
}

char *angle_asc(double a){
	char s;
	int d, min;
	double sec;
	if (a >= 0.)
		s = '+';
	else{
		s = '-'; a = -a;
	}
	d   = (int)(a/3600.);
	min = (int)((a - (double)d*3600.)/60.);
	sec = a - (double)d*3600. - (double)min*60.;
	d %= 360;
	if(sec>59.9) sec=59.9;
	sprintf(buf, "%c%d:%02d':%04.1f''", s,d,min,sec);
	return buf;
}

#define CMNTSZ 79
#define CMNT(...) snprintf(comment, CMNTSZ, __VA_ARGS__)
#define FTKEY(...) WRITEKEY(fp, __VA_ARGS__, comment)

void write_bta_data(fitsfile *fp){
	char comment[CMNTSZ + 1];
	char *val;
	double dtmp;
	int i;
	get_shm_block(&sdat, ClientSide);
	if(!check_shm_block(&sdat)) return;
	// TELESCOP / Telescope name
	CMNT("Telescope name");
	FTKEY(TSTRING, "TELESCOP", "BTA 6m telescope");
	dtmp = S_time-EE_time;
	// ST / sidereal time (hh:mm:ss.ms)
	CMNT("Sidereal time: %s", time_asc(dtmp));
	FTKEY(TDOUBLE, "ST", &dtmp);
	// UT / universal time (hh:mm:ss.ms)
	CMNT("Universal time: %s", time_asc(M_time));
	FTKEY(TDOUBLE, "UT", &M_time);
	CMNT("Julian date");
	FTKEY(TDOUBLE, "JD", &JDate);

	switch(Tel_Focus){
		default:
		case Prime    :
			val = "Prime";
			// FOCUS / Focus of telescope (mm)
			CMNT("Focus of telescope (mm)");
			FTKEY(TDOUBLE, "VAL_F", &val_F);
		break;
		case Nasmyth1 :  val = "Nasmyth1";  break;
		case Nasmyth2 :  val = "Nasmyth2";  break;
	}
	CMNT("Observation focus");
	FTKEY(TSTRING, "FOCUS", val);
	// EPOCH / Epoch of RA & DEC
	time_t epoch = time(NULL);
	strftime(comment, CMNTSZ, "%Y", gmtime(&epoch));
	i = atoi(comment);
	CMNT("Epoch of RA & DEC");
	FTKEY(TINT, "EPOCH", &i);
	CMNT("Current object R.A.: %s", time_asc(CurAlpha));
	// RA / Right ascention (HH MM SS)
	FTKEY(TDOUBLE, "RA", &CurAlpha);
	// DEC / Declination (DD MM SS)
	CMNT("Current object Decl.: %s", angle_asc(CurDelta));
	FTKEY(TDOUBLE, "DEC", &CurDelta);
	CMNT("Source R.A.: %s", time_asc(SrcAlpha));
	FTKEY(TDOUBLE, "S_RA", &SrcAlpha);
	CMNT("Source Decl.: %s", angle_asc(SrcDelta));
	FTKEY(TDOUBLE, "S_DEC", &SrcDelta);
	CMNT("Telescope R.A: %s", time_asc(val_Alp));
	FTKEY(TDOUBLE, "T_RA", &val_Alp);
	CMNT("Telescope Decl.: %s", angle_asc(val_Del));
	FTKEY(TDOUBLE, "T_DEC", &val_Del);
	// A / Azimuth
	CMNT("Current object Azimuth: %s", angle_asc(tag_A));
	FTKEY(TDOUBLE, "A", &tag_A);
	// Z / Zenith distance
	CMNT("Current object Zenith: %s", angle_asc(tag_Z));
	FTKEY(TDOUBLE, "Z", &tag_Z);
	// ROTANGLE / Field rotation angle
	CMNT("Field rotation angle: %s", angle_asc(tag_P));
	FTKEY(TDOUBLE, "ROTANGLE", &tag_P);

	CMNT("Telescope A: %s", angle_asc(val_A));
	FTKEY(TDOUBLE, "VAL_A", &val_A);
	CMNT("Telescope Z: %s", angle_asc(val_Z));
	FTKEY(TDOUBLE, "VAL_Z", &val_Z);
	CMNT("Current P: %s", angle_asc(val_P));
	FTKEY(TDOUBLE, "VAL_P", &val_P);

	CMNT("Dome A: %s", angle_asc(val_D));
	FTKEY(TDOUBLE, "VAL_D", &val_D);
	// OUTTEMP / Out temperature (C)
	CMNT("Outern temperature, degC");
	FTKEY(TDOUBLE, "OUTTEMP", &val_T1);
	// DOMETEMP / Dome temperature (C)
	CMNT("In-dome temperature, degC");
	FTKEY(TDOUBLE, "DOMETEMP", &val_T2);
	// MIRRTEMP / Mirror temperature (C)
	CMNT("Mirror temperature, degC");
	// PRESSURE / Pressure (mmHg)
	FTKEY(TDOUBLE, "MIRRTEMP", &val_T3);
	CMNT("Pressure, mmHg");
	FTKEY(TDOUBLE, "PRESSURE", &val_B);
	// WIND / Wind speed (m/s)
	CMNT("Wind speed, m/s");
	FTKEY(TDOUBLE, "WIND", &val_Wnd);
	CMNT("Humidity, %%");
	FTKEY(TDOUBLE, "HUM", &val_Hmd);
}
