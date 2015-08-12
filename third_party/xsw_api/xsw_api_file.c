/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW File BIO API
 *
 * Filename: xsw_api_file.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW File BIO API interface module.
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

#include "xsw_apis.h"
#include "xsw_internal.h"

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileDebug                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWFileDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_FILE)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileInfo                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWFileInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW File BIO ('%s', size=%llu)", bio->pathname, bio->size);

	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileClose                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWFileClose(XSW_BIO *bio)
{
	XSWFileDebug("XSWFileClose(bio=0x%p) called\n", bio);

	// Close file handle
	close(bio->fd);

	// Free BIO
	XSWFreeBIO(bio);

	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileDiscard                                             */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWFileDiscard(XSW_BIO *bio, u64 offset, u64 size)
{
int retcode = 0;

	XSWFileDebug("XSWFileDiscard(bio=0x%p, offset=%llu, size=%llu)\n",
		bio, offset, size);

#if defined(POSIX_FADV_DONTNEED)
	// Discard pages in page cache
	retcode = posix_fadvise(bio->fd, bio->offset+offset, size, POSIX_FADV_DONTNEED);
#endif

	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileRead                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWFileRead(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	XSWFileDebug("XSWFileRead(bio=0x%p, offset=%llu, size=%llu, buf=0x%p)\n",
		bio, offset, size, buf);

	// Read from the log via pread
	if ((u64)(bytes = pread(bio->fd, buf, size, bio->offset + offset)) != size)
		XSWFileDebug("XSWFileRead() failed to read at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	XSWFileDebug("XSWFileRead() returning %lu bytes\n", bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileSync                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWFileSync(XSW_BIO *bio, u64 offset, u64 size, int sync)
{
	(void)offset; // Offset not used for raw device
	(void)size;   // Size not used for raw device
	(void)sync;   // Sunc not used for raw device

	XSWFileDebug("XSWFileSync(bio=0x%p, offset=%llu, size=%llu, sync=%d) called\n", bio, offset, size, sync);

	return(fsync(bio->fd));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileWrite                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWFileWrite(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	XSWFileDebug("XSWFileWrite(bio=0x%p, offset=%llu, size=%llu, buf=0x%p)\n",
		bio, offset, size, buf);

	// Read from the log via pread
	if ((u64)(bytes = pwrite(bio->fd, buf, size, bio->offset + offset)) != size)
		XSWFileDebug("XSWFileWrite() failed to write at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	XSWFileDebug("XSWFileWrite() returning %lu bytes\n", bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileTrim                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWFileTrim(XSW_BIO *bio, u64 offset, u64 size)
{
	XSWFileDebug("XSWFileTrim(bio=0x%p, offset=%llu, size=%llu)\n", bio, offset, size);

	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileOpenEx                                              */
/* Scope       : Public API                                                 */
/* Description : Open file.                                                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWFileOpenEx(int fd, u64 offset, u64 size)
{
XSW_BIO *bio;
struct stat stat;

	XSWFileDebug("XSWFileOpenEx(fd=%d, offset=%llu, size=%llu) called\n", fd, offset, size);

	// Get file info
	if (fstat(fd, &stat)) {
		// Return error
		return(0);
	}

	// Is this a regular file?
	if (!S_ISREG(stat.st_mode)) {
		// Display error
		XSWFileDebug("XSWFileOpenEx(): Not a regular file\n");
		// Return error
		return(0);
	}

	// Use the full device?
	if (size == 0) {
		// Use full size
		size = stat.st_size;
	}

	// Make sure file size is page granular 
	if (size & XSW_PAGE_MASK) {
		// Display error
		XSWFileDebug("XSWFileOpenEx(): file size not page granular\n");
		// Return error
		return(0);
	}

	// Make sure requested offset + size fits within the file.
	if ((offset + size) > (u64)stat.st_size) {
		// Display error
		XSWFileDebug("XSWFileOpenEx() offset/size out of bounds for file\n");
		// Return error
		return(0);
	}

	// Allocate BIO
	bio = XSWAllocBIO(0);

	bio->type           = XSW_BIO_TYPE_FILE;
	bio->user           = 0;
	bio->fsync          = XSWFileSync;    // sync device
	bio->close          = XSWFileClose;   // Close for raw device
	bio->write          = XSWFileWrite;   // Write device
	bio->read           = XSWFileRead;    // Read device
	bio->trim           = XSWFileTrim;    // Trim device
	bio->discard        = XSWFileDiscard; // Discard
	bio->info           = XSWFileInfo;
	bio->offset         = offset;
	bio->size           = size;
	bio->blksize        = stat.st_blksize;
	bio->fd             = fd;

	// Return BIO
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileCreate                                              */
/* Scope       : Public API                                                 */
/* Description : Create a file.                                             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWFileCreate(char *pathname, u64 size)
{
int fd;
XSW_BIO *bio = 0;

	// Open/Create file
	if ((fd = open(pathname, O_RDWR | O_CREAT, 0777)) >= 0) {
		// Truncate to requested size
		if (!ftruncate(fd, size)) {
			// Get BIO
			bio = XSWFileOpenEx(fd, 0, size);
		}
		// Make sure BIO was created, otherwise close file
		if (!bio) {
			// Display error
			XSWFileDebug("XSWFileCreate() error creating/opening '%s'\n", pathname);
			// Close file
			close(fd);
		}
	}
	// Return BIO or NULL if error
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFileOpen                                                */
/* Scope       : Public API                                                 */
/* Description : Open file.                                                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWFileOpen(char *pathname)
{
int fd;
XSW_BIO *bio = 0;

	XSWFileDebug("XSWFileOpen(pathname='%s') called\n", pathname);

	// Check if pathname points to a device
	if ((fd = open(pathname, O_RDWR)) >= 0) {
		// Create a BIO for file.
		if ((bio = XSWFileOpenEx(fd, 0, 0))) {
			// Copy in the device name
			strcpy(bio->pathname, pathname);
		} else {
			// Close handle
			close(fd);
		}
	}
	// Return BIO or NULL if error
	return(bio);
}
