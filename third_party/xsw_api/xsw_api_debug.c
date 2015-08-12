/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW debug API
 *
 * Filename: xsw_api_debug.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW debug API interface module.
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <time.h>
#include <execinfo.h>


#include "xsw_apis.h"
#include "xsw_internal.h"

static int _XSWDebugEnableFlags = 0;

static char _outputFile[XSW_MAX_PATH];

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugSetOutput                                          */
/* Scope       : Public API                                                 */
/* Description : Setup debug output file.                                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWDebugSetOutput(char *filename)
{
	if (filename) {
		// Setup output filename
		strcpy(_outputFile, filename);
	} else {
		// Send output to stdout
		_outputFile[0] = 0;
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugEnable                                             */
/* Scope       : Public API                                                 */
/* Description : Enable/Disable stderr error output.                        */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDebugEnable(int select, int enable)
{
int oldFlags = _XSWDebugEnableFlags;

	if (enable)
		_XSWDebugEnableFlags |= select;
	else
		_XSWDebugEnableFlags &= ~select;

	return(oldFlags);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugEnabled                                            */
/* Scope       : Public API                                                 */
/* Description : Test if debug is enabled for the selected module.          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDebugEnabled(int select)
{
	return(_XSWDebugEnableFlags & select);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugOut                                                */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWDebugOut(char *string)
{
int fd;
time_t t;
struct tm *currentTime;
char asciiTime[128];

	strcpy(asciiTime, "");

	if (time(&t) != ((time_t)-1)) {
		currentTime = localtime(&t);

		sprintf(asciiTime, "%02d:%02d:%02d %02d/%02d/%04d: ",
			currentTime->tm_hour,
			currentTime->tm_min,
			currentTime->tm_sec,   
			1+currentTime->tm_mon,
			currentTime->tm_mday,
			1900+currentTime->tm_year);
	}

	if (_outputFile[0]) {
		if ((fd = open(_outputFile, O_WRONLY|O_CREAT|O_APPEND, 0777)) >= 0) {
			write(fd, asciiTime, strlen(asciiTime));
			write(fd, string, strlen(string));
			close(fd);
		}
	}
	else
		XSWSysPrintf("%s%s", asciiTime, string);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugKernel                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWDebugKernel(const char *fmt, ...)
{
va_list args;
char string[512];
int fd;

	// Build argument list
	va_start(args,fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	printf("%s", string);

	if ((fd = open("/dev/kmsg", O_WRONLY, 0777)) >= 0) {
		write(fd, string, strlen(string));
		close(fd);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDebugKernelStack                                        */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWDebugKernelStack(void)
{
void *buffer[50];
char **strings;
int i;
int nptrs;

	XSWDebugKernel("Stack Dump:\n");

	if ((nptrs = backtrace(buffer, 50))) {
		if ((strings = backtrace_symbols(buffer, nptrs))) {
			for (i=0; i<nptrs; i++) 
				XSWDebugKernel("[%d]: %s\n", i, strings[i]);
			free(strings);
		}
	}
	XSWDebugKernel("End Stack Dump:\n");
}

