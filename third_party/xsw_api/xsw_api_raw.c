/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW RAW Block I/O API
 *
 * Filename: xsw_api_raw.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW RAW block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
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

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawDebug                                                */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWRawDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_RAW)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawCloseDevice                                          */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRawCloseDevice(XSW_BIO *bio)
{
	XSWRawDebug("XSWRawCloseDevice(bio=0x%p) called\n", bio);

	// Close file handle
	close(bio->fd);

	// Free BIO
	XSWFreeBIO(bio);

	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawReadDevice                                           */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWRawReadDevice(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	XSWRawDebug("XSWRawReadDevice(bio=0x%p, offset=%llu, size=%llu, buf=0x%p)\n",
		bio, offset, size, buf);

	// Read from the log via pread
	if ((u64)(bytes = pread(bio->fd, buf, size, bio->offset + offset)) != size)
		XSWRawDebug("XSWRawReadDevice() failed to read at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	XSWRawDebug("XSWRawReadDevice() returning %lu bytes\n", bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawDiscardDevice                                        */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRawDiscardDevice(XSW_BIO *bio, u64 offset, u64 size)
{
int retcode = 0;

	XSWRawDebug("XSWRawDiscardDevice(bio=0x%p, offset=%llu, size=%llu)\n",
		bio, offset, size);

#if defined(POSIX_FADV_DONTNEED)
	// Discard pages in page cache
	retcode = posix_fadvise(bio->fd, bio->offset+offset, size, POSIX_FADV_DONTNEED);
#endif

	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawSyncDevice                                           */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRawSyncDevice(XSW_BIO *bio, u64 offset, u64 size, int sync)
{
	(void)offset; // Offset not used for raw device
	(void)size;   // Size not used for raw device
	(void)sync;   // Sunc not used for raw device

	XSWRawDebug("XSWRawSyncDevice(bio=0x%p, offset=%llu, size=%llu, sync=%d) called\n", bio, offset, size, sync);

	return(fsync(bio->fd));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawWriteDevice                                          */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWRawWriteDevice(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	XSWRawDebug("XSWRawWriteDevice(bio=0x%p, offset=%llu, size=%llu, buf=0x%p)\n",
		bio, offset, size, buf);

	// Read from the log via pread
	if ((u64)(bytes = pwrite(bio->fd, buf, size, bio->offset + offset)) != size)
		XSWRawDebug("XSWRawWriteDevice() failed to write at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	XSWRawDebug("XSWRawWriteDevice() returning %lu bytes\n", bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawTrim                                                 */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRawTrimDevice(XSW_BIO *bio, u64 offset, u64 size)
{
	XSWRawDebug("XSWRawTrimDevice(bio=0x%p, offset=%llu, size=%llu)\n", bio, offset, size);

	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawInfo                                                 */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRawInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW Raw Device '%s'", bio->pathname);
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawOpenEx                                               */
/* Scope       : Public API                                                 */
/* Description : Open raw device.                                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWRawOpenEx(int fd, u64 offset, u64 size)
{
XSW_BIO *bio;
u64 real_size;
unsigned int blksize;

	// Debug output
	XSWRawDebug("XSWRawOpenEx(fd=%d, offset=%llu, size=%llu) called\n", fd, offset, size);

	// Read device size
	if(ioctl(fd,BLKGETSIZE64,&real_size)<0) {
		// Debug output
		XSWRawDebug("XSWRawOpenEx() failed: BLKGETSIZE64 failed\n");
		// Return error
		return(0);
	}

	// Use the full device?
	if (size == 0) {
		// Use full size
		size = real_size;
	}

	// Make sure requested offset + size fits within the device. 
	if ((offset + size) > real_size) {
		// Debug output
		XSWRawDebug("XSWRawOpenEx() offset/size out of bounds for device\n");
		// Return error
		return(0);
	}

	// Read physical block size
	if(ioctl(fd,BLKPBSZGET,&blksize)<0) {
		// Debug output
		XSWRawDebug("XSWRawOpenEx() failed: BLKPBSZGET failed\n");
		// Return error
		return(0);
	}

	// Allocate BIO
	bio = XSWAllocBIO(0);

	// Setup RAW BIO
	bio->type           = XSW_BIO_TYPE_RAW;
	bio->info           = XSWRawInfo;          // Info for raw device
	bio->fsync          = XSWRawSyncDevice;    // sync device
	bio->close          = XSWRawCloseDevice;   // Close for raw device
	bio->write          = XSWRawWriteDevice;   // Write device
	bio->read           = XSWRawReadDevice;    // Read device
	bio->trim           = XSWRawTrimDevice;    // Trim device
	bio->discard        = XSWRawDiscardDevice; // Discard
	bio->offset         = offset;              // Physical offset
	bio->size           = size;                // Physical size 
	bio->blksize        = blksize;             // Physical block size
	bio->fd             = fd;                  // Raw device handle

	// Return new BIO
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRawOpen                                                 */
/* Scope       : Public API                                                 */
/* Description : Open raw device.                                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWRawOpen(char *device_path)
{
int fd;
XSW_BIO *bio = 0;

	// Check if pathname points to a device
	if ((fd = open(device_path, O_RDWR)) >= 0) {
		// Create a BIO for raw device.
		if ((bio = XSWRawOpenEx(fd, 0, 0))) {
			// Copy in the device name
			strcpy(bio->pathname, device_path);
		} else {
			// Close handle
			close(fd);
		}
	}
	// Return BIO or NULL if error
	return(bio);
}

