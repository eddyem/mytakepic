#ifndef __USAGE_H__
#define __USAGE_H__

#include "takepic.h"
#include <getopt.h>
#include <stdarg.h>

extern int
		 exptime		// ����� ���������� (� ��)
		,pics			// ���-�� ��������� ������
		,hbin			// �������������� �������
		,vbin			// ������������ �������
		,X0, X1, Y0, Y1 // ���������� ���������� ����
		,flushes		// ���-�� �������
		,pause_len		// ����������������� ����� (� �) ����� �������
;
extern double temperature;	// ����������� (������� ������, ����� - �� ������ ����.)

extern fliframe_t frametype;	// ��� ������ (������� ��� ��������)

extern bool
	 only_T			// ������ ���������� ����������� - � �����
	,set_T			// ������ ������ �����������
	,save_Tlog		// ��������� ������ ����������
	,save_image		// ��������� �����������
	,stat_logging	// ������ ������������ ����������
;
extern char *objname	// ��� �������
	,*outfile			// ������� ����� ����� ��� ������ raw/fits
	,*objtype			// ��� �����������
	,*instrument		// �������� ������� (�� ��������� - "direct imaging")
	,*observers			// �����������
	,*prog_id			// ��� ���������
	,*author			// �����
;
void usage(char *fmt, ...);
void parse_args(int argc, char **argv);

#endif // __USAGE_H__
