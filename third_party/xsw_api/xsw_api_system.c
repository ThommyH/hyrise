/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW System API
 *
 * Filename: xsw_api_system.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW System API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 03/06/14 Adrian      Created.
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
#include <stdarg.h>
#include <pthread.h>
#include <time.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

static XSW_SYS_SETUP _xsw_system;

static pthread_mutex_t _locks[] = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
};

/*****************************************************************************/
/*                                                                           */
/* Function    : XSWSysLock                                                  */
/* Scope       :                                                             */
/* Description : Acquire a lock                                              */
/* Arguments   :                                                             */
/* Returns     :                                                             */
/*                                                                           */
/*****************************************************************************/
void XSWSysLock(int index)
{
	// Acquire the requested mutex
	pthread_mutex_lock(&_locks[index]);
}

/*****************************************************************************/
/*                                                                           */
/* Function    : XSWSysUnlock                                                */
/* Scope       :                                                             */
/* Description : Release a lock                                              */
/* Arguments   :                                                             */
/* Returns     :                                                             */
/*                                                                           */
/*****************************************************************************/
void XSWSysUnlock(int index)
{
	// Release the requested mutex
	pthread_mutex_unlock(&_locks[index]);
}

/*****************************************************************************/
/*                                                                           */
/* Function    : XSWSysLockAll                                               */
/* Scope       :                                                             */
/* Description : Acquire all mutexes                                         */
/* Arguments   :                                                             */
/* Returns     :                                                             */
/*                                                                           */
/*****************************************************************************/
static void XSWSysLockAll(void)
{
int i;

	// Acquire all locks
	for (i=0; i<(int)(sizeof(_locks)/sizeof(_locks[0])); i++) {
		// Acquire mutex[i]
		pthread_mutex_lock(&_locks[i]);
	}
}

/*****************************************************************************/
/*                                                                           */
/* Function    : XSWSysUnlockAll                                             */
/* Scope       :                                                             */
/* Description : Release all mutexes                                         */
/* Arguments   :                                                             */
/* Returns     :                                                             */
/*                                                                           */
/*****************************************************************************/
static void XSWSysUnlockAll(void)
{
int i;

	// Release all locks in reverse order
	for (i = (sizeof(_locks)/sizeof(_locks[0])); i>0; i--) {
		// Release mutex[i-1]
		pthread_mutex_unlock(&_locks[i-1]);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWSysEnableFork                                           */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWSysEnableFork(void)
{
	// Setup fork handler
	pthread_atfork(XSWSysLockAll, XSWSysUnlockAll, XSWSysUnlockAll);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWSysSetup                                                */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWSysSetup(XSW_SYS_SETUP *system)
{
	// Make sure caller passes in a valid pointer
	if (system) {
		// Copy new system configuration
		memcpy(&_xsw_system, system, sizeof(XSW_SYS_SETUP));
		// Return success
		return(0);
	}
	// Zero out current configuration which defaults everything.
	memset(&_xsw_system, 0, sizeof(XSW_SYS_SETUP));
	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWSysPrintf                                               */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWSysPrintf(const char *fmt, ...)
{
int retCode;
va_list args;
char string[512];

	// Build argument list
	va_start(args,fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	// Check of user supplied printf was provided.
	if (_xsw_system.xsw_printf) {
		// Call user printf
		retCode = _xsw_system.xsw_printf("%s", string);
	}
	else {
		// Call default CRT printf
		retCode = printf("%s", string);
	}

	return(retCode);
}

