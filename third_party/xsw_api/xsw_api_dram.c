/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW DRAM BIO API
 *
 * Filename: xsw_api_dram.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW DRAM BIO API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/22/14 Adrian      Created.
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
#include <xsw/drivers/xsw_dram.h>

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramDebug                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWDramDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_DRAM)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramUnloadModule                                        */
/* Scope       : Public API                                                 */
/* Description : Unload the XtremSW DRAM BIO kernel module.                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramUnloadModule(void)
{
	// Remove module
	return(XSWUnloadModule((char*)"xsw_dram"));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramClose                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWDramClose(XSW_BIO *bio)
{
int retCode = 0;

	XSWDramDebug("XSWDramClose(bio=0x%p) called\n", bio);

	// Flush data
	fsync(bio->fd);

	// Close the handle to the file BIO driver
	close(bio->fd);

	// Free BIO
	XSWFreeBIO(bio);

	XSWDramDebug("XSWDramClose() success\n");

	// Return current error state.
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramInfo                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWDramInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW DRAM BIO (virt_addr=%p, size=%llu)\n", bio->mmap_vaddr, bio->size);
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramNumDevices                                          */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramNumDevices(void)
{
int ctrl_fd;
int num_devices = 0;

	// Open dra, ctrl device
	if ((ctrl_fd = open(XSWDRAM_CTRL_DEV, O_RDWR)) >= 0) {
		// Destroy the dram block device
		num_devices = ioctl(ctrl_fd, XSWDRAM_GET_NUM_DEVICES, 0);
		// Close ctrl device
		close(ctrl_fd);
	}
	// Return number of devices
	return(num_devices);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramDestroy                                             */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramDestroy(int device_id)
{
int ctrl_fd;
int retcode;

	// Open dram ctrl device
	if ((ctrl_fd = open(XSWDRAM_CTRL_DEV, O_RDWR)) < 0) {
		XSWDramDebug("XSWDramDestroy(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Destroy the dram block device
	retcode = ioctl(ctrl_fd, XSWDRAM_DESTROY_DRAM, device_id);

	// Close ctrl device
	close(ctrl_fd);

	// Return success
	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramQueryInternal                                       */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWDramQueryInternal(XSWDRAM_IOCTL_INFO *dram_info, XSW_DRAM_INFO *info)
{
int retcode;
int ctrl_fd;

	// Open dram ctrl device
	if ((ctrl_fd = open(XSWDRAM_CTRL_DEV, O_RDWR)) < 0) {
		XSWDramDebug("XSWDramQuery(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Query this device_id
	retcode = ioctl(ctrl_fd, XSWDRAM_INFO_DRAM, dram_info);

	// Did we succeed? If so, fill in the info
	if (!retcode) {
		// Did the use really request info?
		if (info) {
			// Copy in device_id
			info->device_id = dram_info->device_id;
			// Copy phys addr
			info->phys_addr = dram_info->phys_addr;
			// Copy virt addr
			info->virt_addr = dram_info->virt_addr;
			// Copy in size
			info->size = dram_info->size;
			// Copy in refcnt
			info->refcnt = dram_info->refcnt;
			// Copy in full device/rename name
			strcpy(info->node, dram_info->node);
		}
	}

	// Close ctrl device
	close(ctrl_fd);

	// Return success
	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramQueryNode                                          */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramQueryNode(char *node, XSW_DRAM_INFO *info)
{
XSWDRAM_IOCTL_INFO dram_info;

	// Setup request id for node query
	dram_info.device_id = -1;

	// Setup node
	strcpy(dram_info.node, node);

	// Query dram device by node name
	return(XSWDramQueryInternal(&dram_info, info));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramQuery                                               */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramQuery(int device_id, XSW_DRAM_INFO *info)
{
XSWDRAM_IOCTL_INFO dram_info;

	// Setup request id for device id
	dram_info.device_id = device_id;

	// Clear node
	strcpy(dram_info.node, "");

	// Query dram device by device ID
	return(XSWDramQueryInternal(&dram_info, info));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramOpen                                                */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWDramOpen(char *device)
{
int retcode;
XSW_BIO *bio = 0;
int ctrl_fd;
XSWDRAM_IOCTL_INFO dram_info;

	// Open dram ctrl device
	if ((ctrl_fd = open(XSWDRAM_CTRL_DEV, O_RDWR)) < 0) {
		XSWDramDebug("XSWDramQuery(): driver open failed %d\n", ctrl_fd);
		return(0);
	}

	// Invalidate request id
	dram_info.device_id = -1;

	// Copy in node name
	strcpy(dram_info.node, device);

	// Query this device_id
	retcode = ioctl(ctrl_fd, XSWDRAM_INFO_DRAM, &dram_info);

	// Close ctrl device
	close(ctrl_fd);

	// Did we succeed? If so, fill in the info
	if (!retcode) {
		// Open the raw device
		if ((bio = XSWRawOpen(device))) {
			// Change the BIO type to DRAM
			bio->type = XSW_BIO_TYPE_DRAM;
			// Setup virt_addr
			bio->mmap_vaddr = (void*)dram_info.virt_addr;
			// Change info
			bio->info = XSWDramInfo;
			// Change close
			bio->close = XSWDramClose;
		}
	}

	// Return BIO for DRAM device, or NULL if error
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWDramCreate                                              */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWDramCreate(u64 phys_addr, u64 size)
{
int device_id;
int ctrl_fd;
XSWDRAM_IOCTL_CREATE create_dram;

	XSWDramDebug("XSWDramCreate(phys_addr=%p, size=%p) called\n", (void*)phys_addr, (void*)size);

	// Open dram ctrl device
	if ((ctrl_fd = open(XSWDRAM_CTRL_DEV, O_RDWR)) < 0) {
		XSWDramDebug("XSWDramCreate(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Setup create request
	create_dram.phys_addr = phys_addr;
	create_dram.size      = size;
	create_dram.blksize   = 4096;
	create_dram.virt_addr = 0;

	// Create a dram block device
	device_id = ioctl(ctrl_fd, XSWDRAM_CREATE_DRAM, &create_dram);

	// Close the handle to the dram ctrl device
	close(ctrl_fd);

	// Check if create failed or not.	
	if (device_id < 0) {
		XSWDramDebug("XSWDramCreate(): ioctl XSWDRAM_CREATE_DRAM failed %d\n", device_id);
		// Return error
		return(-1);
	}

	// Return success
	return(device_id);
}

