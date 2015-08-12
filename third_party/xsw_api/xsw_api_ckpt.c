/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Checkpoint API
 *
 * Filename: xsw_api_ckpt.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Checkpoint API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 05/05/14 Adrian      Created.
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
#include <pthread.h>

#include "xsw_apis.h"
#include "xsw_internal.h"
#include <xsw/drivers/xsw_ckpt.h>

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDebug                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWCkptDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_CKPT)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptCalculateSize                                       */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptCalculateSize(u64 user_size, u32 bio_blksize, u64 *bio_size)
{
u64 user_blocks;
u64 bitmap_blocks;
u64 bitmap_blksize = XSW_CKPT_BITMAP_BLOCK_SIZE;

	// Round user_size up to a 64M boundary
	user_size = ((user_size + (XSW_CKPT_BITMAP_SIZE_ALIGN-1)) / XSW_CKPT_BITMAP_SIZE_ALIGN) *
		XSW_CKPT_BITMAP_SIZE_ALIGN;

	// If requested BIO size is 0, use the bitmap block size
	if (bio_blksize == 0)
		bio_blksize = bitmap_blksize;

	// Calculate number of user blocks
	if ((user_blocks = (user_size + (bio_blksize-1)) / bio_blksize)) {
		// Calculate number of bitmap blocks
		if ((bitmap_blocks = (user_blocks + (XSWCKPT_PAGES_PER_BLOCK-1)) / XSWCKPT_PAGES_PER_BLOCK)) {
			// Return required size of BIO
			*bio_size = (1 + (user_blocks * 2) + (bitmap_blocks * 2)) * (u64)bio_blksize;
			// Return success
			return(0);
		}
	}
	// An invalid argument was used
	XSWCkptDebug("XSWCkptCalculateSize() invalid argument\n");

	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptRead                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWCkptRead(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	// Read from the log via pread
	if ((u64)(bytes = pread(bio->fd, buf, size, offset)) != size)
		XSWCkptDebug("XSWCkptRead() failed to read at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptSync                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWCkptSync(XSW_BIO *bio, u64 offset, u64 size, int sync)
{
	(void)offset; // Offset not used for raw device
	(void)size;   // Size not used for raw device
	(void)sync;   // Sunc not used for raw device

	return(fsync(bio->fd));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptWrite                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWCkptWrite(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes;

	// Read from the log via pread
	if ((u64)(bytes = pwrite(bio->fd, buf, size, offset)) != size)
		XSWCkptDebug("XSWCkptWrite() failed to write at offset %llu, size=%llu, bytes=%llu\n", offset,
			size, (u64)bytes);

	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceIoctl                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWCkptDeviceIoctl(unsigned long int cmd, unsigned long arg)
{
int rc;
int ctrl_fd;

	// Open ckpt ctrl device
	if ((ctrl_fd = open(XSWCKPT_CTRL_DEV, O_RDWR)) < 0) {
		XSWCkptDebug("XSWCkptDeviceIoctl(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Issue device ioctl
	rc = ioctl(ctrl_fd, cmd, arg);

	// Close ctrl device
	close(ctrl_fd);

	// Return status
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceCheckpoint                                    */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceCheckpoint(int device_id)
{
	// Issue a checkpoint ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_CHECKPOINT, device_id)); 
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptCheckpoint                                          */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptCheckpoint(XSW_BIO *bio)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Flush data
		fsync(bio->fd);
		// Checkpoint device
		return(XSWCkptDeviceCheckpoint((int)((u64)bio->user)));
	}

	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceRollBackward                                  */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceRollBackward(int device_id)
{
	// Issue a rollback ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_ROLL_BACKWARD, device_id)); 
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptRollBackward                                        */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptRollBackward(XSW_BIO *bio)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Rollback device
		return(XSWCkptDeviceRollBackward((int)((u64)bio->user)));
	}
	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceDiscard                                       */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceDiscard(int device_id)
{
	// Issue a discard ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_DISCARD, device_id)); 
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDiscard                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDiscard(XSW_BIO *bio)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Rollback device
		return(XSWCkptDeviceDiscard((int)((u64)bio->user)));
	}
	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceRollForward                                   */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceRollForward(int device_id)
{
	// Issue a roll forward ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_ROLL_FORWARD, device_id)); 
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptRollForward                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptRollForward(XSW_BIO *bio)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Rollback device
		return(XSWCkptDeviceRollForward((int)((u64)bio->user)));
	}
	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDevicesQueryNum                                     */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDevicesQueryNum(void)
{
int num_devices;

	// Issue num devices query ioctl
	num_devices = XSWCkptDeviceIoctl(XSWCKPT_GET_NUM_DEVICES, 0);

	// Return number of devices or 0 if error
	return(num_devices < 0 ? 0 : num_devices);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptCopyInfo                                            */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWCkptCopyInfo(XSWCKPT_IOCTL_INFO *ckpt_info, XSW_CKPT_INFO *info)
{
	// Copy device node
	memcpy(info->node, ckpt_info->node, sizeof(info->node));
	// Copy in size
	info->size = ckpt_info->size;
	// Copy in blksize
	info->blksize = ckpt_info->blksize;
	// Copy in options
	info->options = ckpt_info->options;
	// Copy in status;
	info->status = ckpt_info->status;
	// Copy in refcnt;
	info->refcnt = ckpt_info->refcnt;
	// Copy in users;
	info->users = ckpt_info->users;
	// Copy in number of checkpoints
	info->num_checkpoints = ckpt_info->num_checkpoints;
	// Copy in user data
	memcpy(&info->user_data[0], &ckpt_info->user_data[0], sizeof(ckpt_info->user_data));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceQuery                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceQuery(int device_id, XSW_CKPT_INFO *info)
{
XSWCKPT_IOCTL_INFO ckpt_info;
int rc;

	// Setup request id
	ckpt_info.device_id = device_id;

	// Issue info ioctl
	if (!(rc=XSWCkptDeviceIoctl(XSWCKPT_INFO, (unsigned long)&ckpt_info))) {
		// Did user request info?
		if (info) {
			// Copy checkpoing info
			XSWCkptCopyInfo(&ckpt_info, info);
		}
	}

	// Return error status
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptSetupOpen(                                          */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWCkptSetupOpen(XSW_BIO *bio, XSWCKPT_IOCTL_OPEN *open, int options)
{
	// Setup open request
	open->options   = options;
	open->type      = bio->type;
	open->fd        = bio->fd;
	open->offset    = bio->offset;
	open->size      = bio->size;
	open->blksize   = bio->blksize;
	open->virt_addr = bio->mmap_vaddr;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptQuery                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptQuery(XSW_BIO *bio, XSW_CKPT_INFO *info)
{
XSWCKPT_IOCTL_PEEK_INFO peek;
int rc;

	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Query the device
		return(XSWCkptDeviceQuery((int)((u64)bio->user), info));
	}

	// Setup open info
	XSWCkptSetupOpen(bio, &peek.open, 0);

	// Issue peek info ioctl
	if (!(rc=XSWCkptDeviceIoctl(XSWCKPT_PEEK_INFO, (unsigned long)&peek))) { 
		// Did user request info?
		if (info) {
			// Copy checkpoing info
			XSWCkptCopyInfo(&peek.info, info);
		}
	}

	// Return status code
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceSetUserData                                   */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceSetUserData(int device_id, int index, u64 data)
{
XSWCKPT_IOCTL_USER user;

	// Setup request id
	user.device_id = device_id;
	// Setup data index
	user.index = index;
	// Setup data
	user.data = data;

	// Issue set user ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_SET_USER, (unsigned long)&user));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptSetUserData                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptSetUserData(XSW_BIO *bio, int index, u64 data)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Set user device data
		return(XSWCkptDeviceSetUserData((int)((u64)bio->user), index, data));
	}

	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceGetUserData                                   */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceGetUserData(int device_id, int index, u64 *data)
{
int rc;
XSWCKPT_IOCTL_USER user;

	// Setup request id
	user.device_id = device_id;
	// Setup data index
	user.index = index;

	// Issue get user ioctl
	if (!(rc = XSWCkptDeviceIoctl(XSWCKPT_GET_USER, (unsigned long)&user))) {
		// Get user data
		*data = user.data;
	}

	// Return error status
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptGetUserData                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptGetUserData(XSW_BIO *bio, int index, u64 *data)
{
	// Make sure this is a checkpoint BIO
	if (bio->type == XSW_BIO_TYPE_CKPT) {
		// Get user data
		return(XSWCkptDeviceGetUserData((int)((u64)bio->user), index, data));
	}

	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptUnloadModule                                        */
/* Scope       : Public API                                                 */
/* Description : Unload the checkpoint kernel module.                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptUnloadModule(void)
{
	// Issue shutdown ioctl
	XSWCkptDeviceIoctl(XSWCKPT_SHUTDOWN, 0); 

	// Remove module
	return(XSWUnloadModule((char*)"xsw_ckpt"));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceClose                                         */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceClose(int device_id)
{
	// Issue a close ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_CLOSE, device_id)); 
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptClose                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWCkptClose(XSW_BIO *bio)
{
	// Debug output
	XSWCkptDebug("XSWCkptClose(bio=0x%p) called\n", bio);
	// Flush data
	fsync(bio->fd);
	// Close the handle to the file BIO driver
	close(bio->fd);
	// Free BIO
	XSWFreeBIO(bio);
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptInfo                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWCkptInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW CKPT BIO, device_id=%d\n", (int)((u64)bio->user));
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptDeviceOpen                                          */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWCkptDeviceOpen(XSW_BIO *bio, int options)
{
XSWCKPT_IOCTL_OPEN open;

	// Setup open info
	XSWCkptSetupOpen(bio, &open, options);

	// Issue a open ioctl
	return(XSWCkptDeviceIoctl(XSWCKPT_OPEN, (unsigned long)&open));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptCreate                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWCkptCreate(char *device_path, u64 size)
{
XSW_BIO *bio;
XSW_BIO *ckpt = 0;

	// Calculate size of region required to create this checkpoint device
	if (XSWCkptCalculateSize(size, XSW_CKPT_DEFAULT_BLKSIZE, &size)) {
		// Return error
		return(0);
	}
	// Create the device/region/file.
	if ((bio = XSWCreate(device_path, size, REGION_FLAGS_CKPT))) {
		// Create checkpoint
		if ((ckpt = XSWCkptOpen(bio, XSWCKPT_OPTION_FORCE_CREATE))) {
			// Disconnect from parent
			XSWOrphan(ckpt);
		}
		// Close device/region/file handle.
		XSWClose(bio);
	}
	// Return checkpoint
	return(ckpt);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWCkptOpen                                                */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWCkptOpen(XSW_BIO *bio, int options)
{
int device_id;
XSW_CKPT_INFO info;
XSW_BIO *ckpt;
int rc;
int fd;

	// Open/Create the device
	if ((device_id = XSWCkptDeviceOpen(bio, options)) >= 0) {
		// Did we fail to query the device?
		if ((rc=XSWCkptDeviceQuery(device_id, &info))) {
			// Debug output
			XSWCkptDebug("XSWCkptOpen(): device query failed %d\n", rc);
			// Close the device
			XSWCkptDeviceClose(device_id);
			// Return error
			return(0);
		}
		// Open device
		if ((fd = open(info.node, O_RDWR)) < 0) {
			// Debug output
			XSWCkptDebug("XSWCkptOpen(): driver open failed %d\n", fd);
			// Close the device
			XSWCkptDeviceClose(device_id);
			// Return NULL for error
			return(0);
		}
		// Notify device that user has opened it.
		if ((rc = XSWCkptDeviceIoctl(XSWCKPT_USER_SIGNAL, device_id))) {
			// Debug output
			XSWCkptDebug("XSWCkptOpen(): failed to signal device %d\n", rc);
			// Close handle
			close(fd);
			// Close the device
			XSWCkptDeviceClose(device_id);
			// Return NULL for error
			return(0);
		}
		// Allocate a BIO
		ckpt = XSWAllocBIO(bio);
		// Copy device node into pathname
		strcpy(ckpt->pathname, info.node);
		// Setup checkpoint BIO
		ckpt->type           = XSW_BIO_TYPE_CKPT;
		ckpt->fd             = fd;
		ckpt->user           = (void*)((u64)device_id);
		ckpt->info           = XSWCkptInfo;
		ckpt->fsync          = XSWCkptSync;
		ckpt->close          = XSWCkptClose;
		ckpt->write          = XSWCkptWrite;
		ckpt->read           = XSWCkptRead;
		ckpt->trim           = 0;
		ckpt->offset         = 0;
		ckpt->size           = info.size;
		ckpt->blksize        = info.blksize;
		// Return device
		return(ckpt);
	}
	// Return error
	return(0);
}

