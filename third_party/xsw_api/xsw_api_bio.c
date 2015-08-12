/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW block I/O API
 *
 * Filename: xsw_api_bio.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW block I/O API interface module.
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
/* Function    : XSWBioDebug                                                */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWBioDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_BIO)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWSync                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWSync(XSW_BIO *bio, u64 offset, u64 size, int sync)
{
int retCode = 0;

	if (bio->fsync) {
		if ((retCode = bio->fsync(bio, offset, size, sync))) 
			XSWBioDebug("XSWSync() failed %d\n", retCode);
	}

	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWTrim                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWTrim(XSW_BIO *bio, u64 offset, u64 size)
{
int retCode = 0;

	if (bio->trim) {
		if ((retCode = bio->trim(bio, offset, size))) 
			XSWBioDebug("XSWTrim() failed %d\n", retCode);
	}

	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDiscard                                                 */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDiscard(XSW_BIO *bio, u64 offset, u64 size)
{
int retCode = 0;

	if (bio->discard) {
		if ((retCode = bio->discard(bio, offset, size))) 
			XSWBioDebug("XSWDiscard() failed %d\n", retCode);
	}

	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWSetPrivate                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWSetPrivate(XSW_BIO *bio, void *data)
{
	// Assign private data to BIO
	bio->client_data = (u64)data;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGetPrivate                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void *XSWGetPrivate(XSW_BIO *bio)
{
	// Return private data
	return((void*)bio->client_data);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWZero                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWZero(XSW_BIO *bio, u64 offset, u64 size)
{
unsigned char zero_data[512];
u64 remaining = size;
u64 count;
u64 total = 0;
ssize_t written;

	// Zero out temp buffer
	memset(zero_data, 0, sizeof(zero_data));
	// Loop writing zeros
	while(remaining) {
		// Calculate amount of data to write
		count = remaining > sizeof(zero_data) ? sizeof(zero_data) : remaining;
		// Break out of there is a write error
		if ((written = XSWWrite(bio, offset, count, zero_data) <= 0))
			break;
		// Increase written
		remaining -= written;
		// Increase offset
		offset += written;
		// Increase total
		total += written;
	}
	// return total
	return(total);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWClose                                                   */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWClose(XSW_BIO *bio)
{
int retCode = 0;

	// If BIO has children, close them
	while(bio->child) {
		// Call close for child
		if ((retCode=XSWClose(bio->child))) {
			// Debug output
			XSWBioDebug("XSWClose() failed %d\n", retCode);
			// Return status
			return(retCode);
		}
	}
	// Is there a close callback?
	if (bio->close) {
		// Call close callback
		return(bio->close(bio));
	}
	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRead                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
ssize_t XSWRead(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
	// Is there a read callback?
	if (bio->read) {
		// Call read callback
		return(bio->read(bio, offset, size, buf));
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWWrite                                                   */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
ssize_t XSWWrite(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
	// Is there a write callback?
	if (bio->write) {
		// Call write callback
		return(bio->write(bio, offset, size, buf));
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWInsertChild                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWInsertChild(XSW_BIO *parent, XSW_BIO *child)
{
	// Setup child's sibling list
	child->sibling_next = parent->child;
	child->sibling_prev = 0;
	// If there is already a child, connect them. 
	if (parent->child) {
		// Setup prev
		parent->child->sibling_prev = child;
	}
	// Insert child into parent's child list.
	parent->child = child;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRemoveChild                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWRemoveChild(XSW_BIO *parent, XSW_BIO *child)
{
	// Disconnect next siblings link
	if (child->sibling_next) {
		// Disconnect prev
		child->sibling_next->sibling_prev = child->sibling_prev;
	}
	// Disconnect prev siblings link
	if (child->sibling_prev) {
		// Disconnect next
		child->sibling_prev->sibling_next = child->sibling_next;
	}
	// Disconnect parent link if pointing to us
	if (parent->child == child) {
		// Disconnect us
		parent->child = child->sibling_next;
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWOrphan                                                  */
/* Scope       : Public API                                                 */
/* Description : Disconnect parent.                                         */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWOrphan(XSW_BIO *bio)
{
	// If there is a parent, remove child from parent's child list.
	if (bio->parent) {
		// Remove child from linked list
		XSWRemoveChild(bio->parent, bio);
		// Return success
		return(0);
	}
	// Did not have a parent
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAllocBIO                                                */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWAllocBIO(XSW_BIO *parent)
{
XSW_BIO *bio;

	// Allocate a BIO
	bio = XSWInternalMalloc(sizeof(XSW_BIO));
	// Zero out the BIO
	memset(bio, 0, sizeof(XSW_BIO));
	// Setup default close
	bio->close = XSWFreeBIO;
	// Setup parent
	bio->parent = parent;
	// If parent exists, add child to parent's child list.
	if (parent) {
		// Insert child in parent's linked list
		XSWInsertChild(parent, bio);
	}
	// Return new handle
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWFreeBIO                                                 */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWFreeBIO(XSW_BIO *bio)
{
	// If there is a parent, remove child from parent's child list.
	if (bio->parent) {
		// Remove child from linked list
		XSWRemoveChild(bio->parent, bio);
	}
	// Free BIO
	XSWInternalFree(bio);
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWInfo                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWInfo(XSW_BIO *bio)
{
	// Is there an info callback?
	if (bio->info) {
		// Call info callback
		bio->info(bio);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDump                                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWDump(XSW_BIO *bio)
{
	XSWSysPrintf("type            = %d  \n", bio->type);
	XSWSysPrintf("parent          = %p  \n", bio->parent);
	XSWSysPrintf("child           = %p  \n", bio->child);
	XSWSysPrintf("(*info)         = %p  \n", bio->info);
	XSWSysPrintf("(*fsync)        = %p  \n", bio->fsync);
	XSWSysPrintf("(*close)        = %p  \n", bio->close);
	XSWSysPrintf("(*read)         = %p  \n", bio->read);
	XSWSysPrintf("(*write)        = %p  \n", bio->write);
	XSWSysPrintf("(*trim)         = %p  \n", bio->trim);
	XSWSysPrintf("fd              = %d  \n", bio->fd);
	XSWSysPrintf("offset          = %llu\n", bio->offset);
	XSWSysPrintf("size            = %llu\n", bio->size);
	XSWSysPrintf("blksize         = %u  \n", bio->blksize);
	XSWSysPrintf("user            = %p  \n", bio->user);
	XSWSysPrintf("mmap_vaddr      = %p  \n", bio->mmap_vaddr);
	XSWSysPrintf("sibling_next    = %p  \n", bio->sibling_next);
	XSWSysPrintf("sibling_prev    = %p  \n", bio->sibling_prev);
	XSWSysPrintf("group_link      = %p  \n", bio->group_link);
	XSWSysPrintf("\n");
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCreate                                                  */
/* Scope       : Public API                                                 */
/* Description : Create a device/region, or file.                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWCreate(char *pathname, u64 size, u32 flags)
{
XSW_STAT xswstat;
XSW_BIO *bio;
 
	// Stat the XSW pathname
	if (XSWStat(pathname, &xswstat)) {
		// Return error
		return(0);
	}
	// Is this a file? If so, we can resize
	if (xswstat.flags & XSW_STAT_FLAGS_FILE) {
		// Resize file
		return(XSWFileCreate(xswstat.pathname, size));
	}
	// Open device
	if ((bio = XSWRawOpen(xswstat.device))) {
		// Is this a region? If so, create a region
		if (xswstat.flags & XSW_STAT_FLAGS_REGION) {
			// Create a region
			if (!XSWContainerCreateRegion(bio, xswstat.pathname, size, bio->blksize, flags)) {
				// Open new region
				return(XSWContainerResolveRegion(bio, xswstat.pathname));
			}
		}
		// Is this a device?
		if (xswstat.flags & XSW_STAT_FLAGS_DEVICE) {
			// Is this device big enough?
			if (size <= bio->size) {
				// Truncate size
				bio->size = size;
				// Return bio
				return(bio);
			}
		}
		// Close device
		XSWClose(bio);
	}
	// Return error
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWOpenEx                                                  */
/* Scope       : Public API                                                 */
/* Description : Open raw device, device/region, or file.                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWOpenEx(int fd, u64 offset, u64 size) 
{
struct stat stat;

	// Stat file handle
	if (!fstat(fd, &stat)) {
		// Is this a block device?
		if (S_ISBLK(stat.st_mode)) {
			// Create a BIO for raw device.
			return(XSWRawOpenEx(fd, offset, size));
		}
		// Is this a regular file?
		if (S_ISREG(stat.st_mode)) {
			// Create a BIO for a regular file.
			return(XSWFileOpenEx(fd, offset, size));
		}
	}
	// Return error
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWOpenStat                                                */
/* Scope       : Public API                                                 */
/* Description : Open from stat.                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWOpenStat(XSW_STAT *xswstat)
{
XSW_BIO *bio;

	// Make sure pathname exists
	if (xswstat->flags & XSW_STAT_FLAGS_EXISTS) {
		// Is this a device?
		if (xswstat->flags & XSW_STAT_FLAGS_DEVICE) {
			// Try to open a raw device.
			return(XSWRawOpen(xswstat->device));
		}
		// Is this a standard file?
		if (xswstat->flags & XSW_STAT_FLAGS_FILE) {
			// Try to open a standard file
			return(XSWFileOpen(xswstat->pathname));
		}
		// Is this an XSW region?
		if (xswstat->flags & XSW_STAT_FLAGS_REGION) {
			// Try to open device
			if ((bio=XSWRawOpen(xswstat->device))) {
				// Try to open region
				if (XSWContainerResolveRegion(bio, xswstat->pathname)) {
					// Return region
					return(bio);
				}
				// Close the BIO
				XSWClose(bio);
			}
		}
	}
	// Return error
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWOpen                                                    */
/* Scope       : Public API                                                 */
/* Description : Open raw device, device/region, or file.                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWOpen(char *pathname)
{
XSW_STAT xswstat;

	// Stat the XSW pathname
	if (!XSWStat(pathname, &xswstat)) {
		// Open file from stat
		return(XSWOpenStat(&xswstat));
	}
	// Return error
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWStat                                                    */
/* Scope       : Public API                                                 */
/* Description : Stat a device/region/file.                                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWStat(char *pathname, XSW_STAT *xswstat)
{
int fd;
int i,len;
char *device;
XSW_BIO *bio;
struct stat filestat;
XSW_BLOCK_REGION region;

	// Clear stats
	memset(xswstat, 0, sizeof(XSW_STAT));
	// Check if pathname points to a device
	if ((fd = open(pathname, O_RDWR)) >= 0) {
		// Try and stat a file
		if (!fstat(fd, &filestat)) {
			// Is this a block device?
			if (S_ISBLK(filestat.st_mode)) {
				// Setup flags
				xswstat->flags = XSW_STAT_FLAGS_DEVICE|XSW_STAT_FLAGS_EXISTS;
				// Copy in device
				strcpy(xswstat->device, pathname);
				// Close file
				close(fd);
				// Return success
				return(0);
			}
			// Is this a regular file?
			if (S_ISREG(filestat.st_mode)) {
				// Setup flags
				xswstat->flags = XSW_STAT_FLAGS_FILE|XSW_STAT_FLAGS_EXISTS;
				// Copy in pathname name
				strcpy(xswstat->pathname, pathname);
				// Close file
				close(fd);
				// Return success
				return(0);
			}
		}
		// Close file
		close(fd);
		// Return error
		return(-1);
	}
	// Check if this is a device, or region
	if (!strncmp(pathname, "/dev/", 5)) {
		// Get length of path
		len = strlen(pathname);
		// Look for container
		for (i=5; i<len; i++) {
			// Did we find the posibility of a region?
			if (pathname[i] == '/') {
				// Duplicate pathname
				device = XSWInternalStrdup(pathname);
				// Place null here
				device[i] = 0;
				// Copy in device
				strcpy(xswstat->device, device);
				// Copy in pathname
				strcpy(xswstat->pathname, &device[i+1]);
				// Free device string
				XSWInternalFree(device);
				// Try to open device
				if ((bio=XSWRawOpen(xswstat->device))) {
					// Setup flags
					xswstat->flags = XSW_STAT_FLAGS_REGION;
					// Try to open region
					if (!XSWContainerGetRegion(bio, xswstat->pathname, &region)) {
						// Enable exists flag
						xswstat->flags |= XSW_STAT_FLAGS_EXISTS;
					}
					// Close the BIO
					XSWClose(bio);
					// Return success
					return(0);
				}
				// Break out
				break;
			}
		}
		// Return error
		return(-1);
	}
	// Setup flags
	xswstat->flags = XSW_STAT_FLAGS_FILE;
	// Copy in pathname name
	strcpy(xswstat->pathname, pathname);
	// Return success
	return(0);
}
