/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Filename: xsw_internal.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This provices misc internal utilities
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 10/27/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

/****************************************************************************/
/*                                                                          */
/* Function    : XSWInternalMalloc                                          */
/* Scope       : Internal API                                               */
/* Description : Allocate memory and check for error.                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void *XSWInternalMalloc(size_t size)
{
void *data;

	// Did we fail to allocate memory?
	if (!(data = XSWMemMalloc(size))) {
		// Display fatal error
		XSWDebugKernel("Out of memory error (size=%lu)\n", size);
		// Dump stack to help debug
		XSWDebugKernelStack();
		// Exit the process
		exit(0);
	}
	// return data
	return(data);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWInternalFree                                            */
/* Scope       : Internal API                                               */
/* Description : Free memory.                                               */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWInternalFree(void *ptr)
{
	// Was memory allocated?
	if (ptr) {
		// Free this memory
		XSWMemFree(ptr);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWInternalStrdup                                          */
/* Scope       : Internal API                                               */
/* Description : Free memory from our private heap                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
char *XSWInternalStrdup(char *string)
{
char *copy;

	// Allocate a new string based on string length + NULL
	copy = XSWInternalMalloc(strlen(string)+1);
	// copy string
	strcpy(copy, string);
	// Return the new string
	return(copy);
}


