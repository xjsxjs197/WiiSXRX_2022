/***************************************************************************
                          cfg.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2007/10/27 - Pete
// - added SSSPSX frame limit mode and MxC stretching modes
//
// 2005/04/15 - Pete
// - changed user frame limit to floating point value
//
// 2004/02/08 - Pete
// - added Windows zn config file handling (no need to change it for Linux version)
//
// 2002/11/06 - Pete
// - added 2xSai, Super2xSaI, SuperEagle cfg stuff
//
// 2002/10/04 - Pete
// - added Win debug mode & full vram view key config
//
// 2002/09/27 - linuzappz
// - separated linux gui to conf.c
//
// 2002/06/09 - linuzappz
// - fixed linux about dialog
//
// 2002/02/23 - Pete
// - added capcom fighter special game fix
//
// 2002/01/06 - lu
// - Connected the signal "destroy" to gtk_main_quit() in the ConfDlg, it
//   should fix a possible weird behaviour
//
// 2002/01/06 - lu
// - now fpse for linux has a configurator, some cosmetic changes done.
//
// 2001/12/25 - linuzappz
// - added gtk_main_quit(); in linux config
//
// 2001/12/20 - syo
// - added "Transparent menu" switch
//
// 2001/12/18 - syo
// - added "wait VSYNC" switch
// - support refresh rate change
// - modified key configuration (added toggle wait VSYNC key)
//   (Pete: fixed key buffers and added "- default"
//    refresh rate (=0) for cards not supporting setting the
//    refresh rate)
//
// 2001/12/18 - Darko Matesic
// - added recording configuration
//
// 2001/12/15 - lu
// - now fpsewp has his save and load routines in fpsewp.c
//
// 2001/12/05 - syo
// - added  "use system memory" switch
// - The bug which fails in the change in full-screen mode from window mode is corrected.
// - added  "Stop screen saver" switch
//
// 2001/11/20 - linuzappz
// - added WriteConfig and rewrite ReadConfigFile
// - added SoftDlgProc and AboutDlgProc for Linux (under gtk+-1.2.5)
//
// 2001/11/11 - lu
// - added some ifdef for FPSE layer
//
// 2001/11/09 - Darko Matesic
// - added recording configuration
//
// 2001/10/28 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#include "../Gamecube/wiiSXconfig.h"

#define _IN_CFG

#include <sys/stat.h>
#undef FALSE
#undef TRUE
#define MAKELONG(low,high)     ((unsigned long)(((unsigned short)(low)) | (((unsigned long)((unsigned short)(high))) << 16)))

#include "externals.h"
#include "cfg.h"
#include "gpu.h"

/////////////////////////////////////////////////////////////////////////////
// CONFIG FILE helpers.... used in (non-fpse) Linux and ZN Windows
/////////////////////////////////////////////////////////////////////////////

char * pConfigFile=NULL;

#include <sys/stat.h>

// some helper macros:

#define GetValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') var = atoi(p); \
 }

#define GetFloatValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') var = (float)atof(p); \
 }

#define SetValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') { \
   len = sprintf(t1, "%d", var); \
   strncpy(p, t1, len); \
   if (p[len] != ' ' && p[len] != '\n' && p[len] != 0) p[len] = ' '; \
  } \
 } \
 else { \
  size+=sprintf(pB+size, "%s = %d\n", name, var); \
 }

#define SetFloatValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') { \
   len = sprintf(t1, "%.1f", (float)var); \
   strncpy(p, t1, len); \
   if (p[len] != ' ' && p[len] != '\n' && p[len] != 0) p[len] = ' '; \
  } \
 } \
 else { \
  size+=sprintf(pB+size, "%s = %.1f\n", name, (float)var); \
 }

/////////////////////////////////////////////////////////////////////////////

void ReadConfigFile()
{
 struct stat buf;
 FILE *in;char t[256];
 int size;
 //int len;
 char * pB, * p;

 if(pConfigFile)
      strcpy(t,pConfigFile);
 else
  {
   strcpy(t,"cfg/gpuPeopsSoftX.cfg");
   in = fopen(t,"rb");
   if (!in)
    {
     strcpy(t,"gpuPeopsSoftX.cfg");
     in = fopen(t,"rb");
     if(!in) sprintf(t,"%s/gpuPeopsSoftX.cfg",getenv("HOME"));
     else    fclose(in);
    }
   else     fclose(in);
  }

 if (stat(t, &buf) == -1) return;
 size = buf.st_size;

 in = fopen(t,"rb");
 if (!in) return;

 pB=(char *)malloc(size);
 memset(pB,0,size);

 //len = 
 fread(pB, 1, size, in);
 fclose(in);

 GetValue("ResX", iResX);
 if(iResX<20) iResX=20;
 iResX=(iResX/4)*4;

 GetValue("ResY", iResY);
 if(iResY<20) iResY=20;
 iResY=(iResY/4)*4;

 iWinSize=MAKELONG(iResX,iResY);

 GetValue("NoStretch", iUseNoStretchBlt);

 GetValue("Dithering", iUseDither);

 GetValue("FullScreen", iWindowMode);
 if(iWindowMode!=0) iWindowMode=0;
 else               iWindowMode=1;

 GetValue("ShowFPS", iShowFPS);
 if(iShowFPS<0) iShowFPS=0;
 if(iShowFPS>1) iShowFPS=1;

 GetValue("SSSPSXLimit", bSSSPSXLimit);
 if(iShowFPS<0) iShowFPS=0;
 if(iShowFPS>1) iShowFPS=1;

 GetValue("ScanLines", iUseScanLines);
 if(iUseScanLines<0) iUseScanLines=0;
 if(iUseScanLines>1) iUseScanLines=1;

 GetValue("UseFrameLimit", UseFrameLimit);
 if(UseFrameLimit<0) UseFrameLimit=0;
 if(UseFrameLimit>1) UseFrameLimit=1;

 GetValue("UseFrameSkip", UseFrameSkip);
 if(UseFrameSkip<0) UseFrameSkip=0;
 if(UseFrameSkip>1) UseFrameSkip=1;

 GetValue("FPSDetection", iFrameLimit);
 if(iFrameLimit<1) iFrameLimit=1;
 if(iFrameLimit>2) iFrameLimit=2;

 GetFloatValue("FrameRate", fFrameRate);
 if(fFrameRate<10.0f)   fFrameRate=10.0f;
 if(fFrameRate>1000.0f) fFrameRate=1000.0f;

 GetValue("CfgFixes", dwCfgFixes);

 GetValue("UseFixes", iUseFixes);
 if(iUseFixes<0) iUseFixes=0;
 if(iUseFixes>1) iUseFixes=1;

 free(pB);

}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void ExecCfg(char *arg) {
	char cfg[256];
	struct stat buf;

	strcpy(cfg, "./cfgPeopsSoft");
	if (stat(cfg, &buf) != -1) {
		strcat(cfg, " ");
		strcat(cfg, arg);
		system(cfg); return;
	}

	strcpy(cfg, "./cfg/cfgPeopsSoft");
	if (stat(cfg, &buf) != -1) {
		strcat(cfg, " ");
		strcat(cfg, arg);
		system(cfg); return;
	}

	sprintf(cfg, "%s/cfgPeopsSoft", getenv("HOME"));
	if (stat(cfg, &buf) != -1) {
		strcat(cfg, " ");
		strcat(cfg, arg);
		system(cfg); return;
	}

	printf("cfgPeopsSoft file not found!\n");
}

void SoftDlgProc(void)
{
	ExecCfg("configure");
}

extern unsigned char revision;
extern unsigned char build;
#define RELEASE_DATE "01.05.2008"

void AboutDlgProc(void)
{
	char args[256];

	sprintf(args, "about %d %d %s", revision, build, RELEASE_DATE);
	ExecCfg(args);
}


////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

void ReadConfig(void)
{
 // defaults
 iResX=640;iResY=480;
 iWinSize=MAKELONG(iResX,iResY);
 iColDepth=16;
 iWindowMode=1;
 iUseScanLines=0;
 if (frameLimit == FRAMELIMIT_AUTO)
 {
	 UseFrameLimit=1;
	 iFrameLimit=2;
 }
 else
 {
	 UseFrameLimit=0;
	 iFrameLimit=0;
 }
 UseFrameSkip = frameSkip;
 fFrameRate=60.0f;
 dwCfgFixes=0;
 iUseFixes=0;
 iUseNoStretchBlt=1;
 iUseDither=0;
 iShowFPS=0;
 bSSSPSXLimit=FALSE;

 // read sets
 ReadConfigFile();

 // additional checks
 if(!iColDepth)       iColDepth=32;
 if(iUseFixes)        dwActFixes=dwCfgFixes;
 SetFixes();
}

void WriteConfig(void) {

 struct stat buf;
 FILE *out;char t[256];int len, size;
 char * pB, * p; char t1[8];

 if(pConfigFile)
      strcpy(t,pConfigFile);
 else
  {
   strcpy(t,"cfg/gpuPeopsSoftX.cfg");
   out = fopen(t,"rb");
   if (!out)
    {
     strcpy(t,"gpuPeopsSoftX.cfg");
     out = fopen(t,"rb");
     if(!out) sprintf(t,"%s/gpuPeopsSoftX.cfg",getenv("HOME"));
     else     fclose(out);
    }
   else     fclose(out);
  }

 if (stat(t, &buf) != -1) size = buf.st_size;
 else size = 0;

 out = fopen(t,"rb");
 if (!out) {
  // defaults
  iResX=640;iResY=480;
  iColDepth=16;
  iWindowMode=1;
  iUseScanLines=0;
  UseFrameLimit=0;
  UseFrameSkip=0;
  iFrameLimit=2;
  fFrameRate=200.0f;
  dwCfgFixes=0;
  iUseFixes=0;
  iUseNoStretchBlt=1;
  iUseDither=0;
  iShowFPS=0;
  bSSSPSXLimit=FALSE;

  size = 0;
  pB=(char *)malloc(4096);
  memset(pB,0,4096);
 }
 else {
  pB=(char *)malloc(size+4096);
  memset(pB,0,size+4096);

  len = fread(pB, 1, size, out);
  fclose(out);
 }

 SetValue("ResX", iResX);
 SetValue("ResY", iResY);
 SetValue("NoStretch", iUseNoStretchBlt);
 SetValue("Dithering", iUseDither);
 SetValue("FullScreen", !iWindowMode);
 SetValue("ShowFPS", iShowFPS);
 SetValue("SSSPSXLimit", bSSSPSXLimit);
 SetValue("ScanLines", iUseScanLines);
 SetValue("UseFrameLimit", UseFrameLimit);
 SetValue("UseFrameSkip", UseFrameSkip);
 SetValue("FPSDetection", iFrameLimit);
 SetFloatValue("FrameRate", fFrameRate);
 SetValue("CfgFixes", (int)dwCfgFixes);
 SetValue("UseFixes", iUseFixes);

 out = fopen(t,"wb");
 if (!out) {
	 if(pB) free(pB);
	 return;
 }

 fwrite(pB, 1, size, out);
 fclose(out);

 free(pB);

}





