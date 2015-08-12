/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Anonymous Block I/O API
 *
 * Filename: xsw_api_anon.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Anonymous block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 06/26/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <linux/fs.h>

#include "xsw_apis.h"
#include "xsw_internal.h"
#include <xsw/drivers/xsw_anon.h>

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonDebug                                               */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWAnonDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_ANON)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonUnloadModule                                        */
/* Scope       : Public API                                                 */
/* Description : Unload the XtremSW Anonymous kernel module.                */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWAnonUnloadModule(void)
{
	// Remove module
	return(XSWUnloadModule((char*)"xsw_anon"));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonClose                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWAnonClose(XSW_BIO *bio)
{
	XSWAnonDebug("XSWAnonClose(bio=0x%p) called\n", bio);

	// Close anonymous handle
	close((int)((u64)bio->user));

	// Free BIO
	XSWFreeBIO(bio);

	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonInfo                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWAnonInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW Anonymous Device '%s'", bio->pathname);
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonAllocRegion                                         */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWAnonAllocRegion(XSW_BIO *bio, u64 size, u64 *offset)
{
int handle;
int rc;
XSWANON_IOCTL_ALLOC anon_alloc;

	// Open ckpt ctrl device
	if ((handle = open(XSWANON_CTRL_DEV, O_RDWR)) < 0) {
		XSWAnonDebug("XSWAnonAllocRegion(): driver open failed %d\n", handle);
		// Return error
		return(handle);
	}

	// Setup device.
	anon_alloc.fd = bio->fd;
	// Setup device offset
	anon_alloc.offset = bio->offset;
	// Setup device size
	anon_alloc.size = bio->size;
	// Setup requested size
	anon_alloc.request = size;

	// Issue device ioctl
	if ((rc=ioctl(handle, XSWANON_ALLOC, &anon_alloc)) < 0) {
		// Close device
		close(handle);
		// Return error
		return(rc);
	}

	// Save allocated address
	*offset = anon_alloc.alloc;

	return(handle);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAnonOpen                                                */
/* Scope       : Public API                                                 */
/* Description : Open anonymous device.                                     */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWAnonOpen(XSW_BIO *bio, u64 size)
{
XSW_BIO *child;
u64 offset = 0;
int anon_fd;

	XSWAnonDebug("XSWAnonOpen(bio=%p, size='%llu') called\n", bio, size);

	// Allocate BIO
	if (!(child = XSWAllocBIO(bio))) {
		XSWAnonDebug("XSWAnonOpen() out of memory\n");
		// Return error
		return(0);
	}

	// Load region
	if ((anon_fd = XSWAnonAllocRegion(bio, size, &offset)) < 0) {
		// Free BIO
		XSWFreeBIO(child);
		// Return error
		return(0);
	}

	// Copy in the device name
	sprintf(child->pathname, "%s/Anon@0x%llx/0x%llx", bio->pathname, offset, size);

	child->user           = (void*)((u64)anon_fd); // Anonymous handle
	child->type           = bio->type;             // Copy type
	child->close          = XSWAnonClose;          // Custom close
	child->info           = XSWAnonInfo;           // Info for anonymous device
	child->fsync          = bio->fsync;            // sync device
	child->write          = bio->write;            // Write device
	child->read           = bio->read;             // Read device
	child->trim           = bio->trim;             // Trim device
	child->offset         = bio->offset+offset;    // Physical offset
	child->size           = size;                  // Physical size 
	child->blksize        = bio->blksize;          // Physical block size
	child->fd             = bio->fd;               // Same device handle
	child->mmap_vaddr     = bio->mmap_vaddr;       // Copy vaddr

	return(child);
}

