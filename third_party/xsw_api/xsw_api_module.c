/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Module API
 *
 * Filename: xsw_api_module.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW module API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <stdarg.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

#ifdef __cplusplus
extern "C" {
#endif
extern long delete_module(const char *, unsigned int);
extern long init_module(void *, unsigned long, const char *);
#ifdef __cplusplus
}
#endif

/****************************************************************************/
/*                                                                          */
/* Function    : XSWModuleDebug                                             */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWModuleDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_MODULE)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWUnloadModule                                            */
/* Scope       : Public API                                                 */
/* Description : Unloads a module (Same functionality as rmmod).            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWUnloadModule(char *module)
{
int retCode;

	// Remove module
	if ((retCode = delete_module(module, O_NONBLOCK | O_TRUNC))) {
		XSWModuleDebug("XSWUnloadModule('%s') delete_module() failed to unload with error %d\n", 
			module, retCode);
	}
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWModuleCheckOpen                                         */
/* Scope       : Private API                                                */
/* Description : Check if a module file exists                              */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWModuleCheckOpen(char *path)
{
FILE *fp;

	if ((fp=fopen(path, "r"))) {
		fclose(fp);
		return(1);
	}
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWModuleFind                                              */
/* Scope       : Private API                                                */
/* Description : Find a module using the file/path passed in.               */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWModuleFind(char *filename, char *module_full_path)
{
FILE *fp;
char line[1024];
char module_name[256];
char modules_dep[256];
int module_name_len;
int len,i;
struct utsname utsname;

	strcpy(module_full_path, "");

	sprintf(module_name, "%s", filename);

	if (!strstr(module_name, ".ko"))
		strcat(module_name, ".ko");

	if (!XSWModuleCheckOpen(module_name)) {

		module_name_len = strlen(module_name);

		uname(&utsname);

		sprintf(modules_dep, "/lib/modules/%s/modules.dep", utsname.release);

		fp = fopen(modules_dep, "r");

		if (fp) {
			while(fgets(line, sizeof(line)-1, fp)) {

				len = strlen(line);

				for (i=0; i<len; i++) {
					if (line[i] == ':') {
						line[i] = 0;
						break;
					}
				}

				if (i >= module_name_len) {
					if (!strcmp(&line[i-module_name_len], module_name)) {
						sprintf(module_full_path, "/lib/modules/%s/%s", utsname.release, line);
						break;
					}
				}
			}
			fclose(fp);
		}
		else
			XSWModuleDebug("Unable to open '%s'\n", modules_dep);
	}
	else
		strcpy(module_full_path, module_name);

	return(XSWModuleCheckOpen(module_full_path));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWLoadModule                                              */
/* Scope       : Public API                                                 */
/* Description : Load a module (Same functionality as insmod).              */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWLoadModule(char *module)
{
FILE *fp;
int retCode=-1;
char *data;
unsigned long len;
char filename[256];

	if (XSWModuleFind(module, filename)) {

		if ((fp = fopen(filename, "rb"))) {

			// Seek to the end of the file.
			fseek(fp, 0, SEEK_END);

			// Get file size
			len = ftell(fp);

			// Seek back to the beginning
			fseek(fp, 0, SEEK_SET);

			// Allocate 
			data = XSWInternalMalloc(len);

			if (fread(data, 1, len, fp) != len) {
				XSWModuleDebug("XSWLoadModule('%s') failed to read module\n", module);
				fclose(fp);
				return(-1);
			}

			fclose(fp);

			retCode = init_module(data, len, "");

			XSWInternalFree(data);
		}
	}

	if (retCode)
		XSWModuleDebug("XSWLoadModule('%s') init_module() failed %d\n", module, retCode);

	return(retCode);
}


