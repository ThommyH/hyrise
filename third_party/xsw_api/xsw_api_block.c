/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2015  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Block BIO API
 *
 * Filename: xsw_api_block.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Block BIO API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 03/12/15 Adrian      Created.
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
#include <xsw/drivers/xsw_block.h>

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockDebug                                              */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWBlockDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_BLOCK)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockUnloadModule                                       */
/* Scope       : Public API                                                 */
/* Description : Unload the XtremSW BLOCK BIO kernel module.                */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockUnloadModule(void)
{
	// Remove module
	return(XSWUnloadModule((char*)"xsw_block"));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockNumDevices                                         */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockNumDevices(void)
{
int ctrl_fd;
int num_devices = 0;

	// Open dra, ctrl device
	if ((ctrl_fd = open(XSWBLOCK_CTRL_DEV, O_RDWR)) >= 0) {
		// Destroy the block block device
		num_devices = ioctl(ctrl_fd, XSWBLOCK_GET_NUM_DEVICES, 0);
		// Close ctrl device
		close(ctrl_fd);
	}
	// Return number of devices
	return(num_devices);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockDestroy                                            */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockDestroy(int device_id)
{
int ctrl_fd;
int retcode;

	// Open block ctrl device
	if ((ctrl_fd = open(XSWBLOCK_CTRL_DEV, O_RDWR)) < 0) {
		XSWBlockDebug("XSWBlockDestroy(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Destroy the block block device
	retcode = ioctl(ctrl_fd, XSWBLOCK_DESTROY_BLOCK, device_id);

	// Close ctrl device
	close(ctrl_fd);

	// Return success
	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockQueryInternal                                      */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWBlockQueryInternal(XSWBLOCK_IOCTL_INFO *block_info, XSW_BLOCK_INFO *info)
{
int retcode;
int ctrl_fd;

	// Open block ctrl device
	if ((ctrl_fd = open(XSWBLOCK_CTRL_DEV, O_RDWR)) < 0) {
		XSWBlockDebug("XSWBlockQuery(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Query this device_id
	retcode = ioctl(ctrl_fd, XSWBLOCK_INFO_BLOCK, block_info);

	// Did we succeed? If so, fill in the info
	if (!retcode) {
		// Did the use really request info?
		if (info) {
			// Copy in device_id
			info->device_id = block_info->device_id;
			// Copy in size
			info->size = block_info->size;
			// Copy in refcnt
			info->refcnt = block_info->refcnt;
			// Copy virt addr
			info->virt_addr = block_info->virt_addr;
			// Copy type
			info->type = block_info->type;
			// Copy in full device/rename name
			strcpy(info->node, block_info->node);
		}
	}

	// Close ctrl device
	close(ctrl_fd);

	// Return success
	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockQueryNode                                          */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockQueryNode(char *node, XSW_BLOCK_INFO *info)
{
XSWBLOCK_IOCTL_INFO block_info;

	// Setup request id for node query
	block_info.device_id = -1;

	// Setup node
	strcpy(block_info.node, node);

	// Query block device by node name
	return(XSWBlockQueryInternal(&block_info, info));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockQuery                                              */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockQuery(int device_id, XSW_BLOCK_INFO *info)
{
XSWBLOCK_IOCTL_INFO block_info;

	// Setup request id for device id
	block_info.device_id = device_id;

	// Clear node
	strcpy(block_info.node, "");

	// Query block device by device ID
	return(XSWBlockQueryInternal(&block_info, info));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWBlockCreate                                             */
/* Scope       : Puplic API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWBlockCreate(XSW_BIO *bio)
{
int device_id;
int ctrl_fd;
XSWBLOCK_IOCTL_CREATE create_block;

	XSWBlockDebug("XSWBlockCreate(bio=%p) called\n", bio);

	// Open block ctrl device
	if ((ctrl_fd = open(XSWBLOCK_CTRL_DEV, O_RDWR)) < 0) {
		XSWBlockDebug("XSWBlockCreate(): driver open failed %d\n", ctrl_fd);
		return(-1);
	}

	// Setup create request
	create_block.type      = bio->type;
	create_block.fd        = bio->fd;
	create_block.size      = bio->size;
	create_block.blksize   = bio->blksize;
	create_block.offset    = bio->offset;
	create_block.virt_addr = bio->mmap_vaddr;

	// Create a block block device
	device_id = ioctl(ctrl_fd, XSWBLOCK_CREATE_BLOCK, &create_block);

	// Close the handle to the block ctrl device
	close(ctrl_fd);

	// Check if create failed or not.	
	if (device_id < 0) {
		XSWBlockDebug("XSWBlockCreate(): ioctl XSWBLOCK_CREATE_BLOCK failed %d\n", device_id);
		// Return error
		return(-1);
	}

	// Return success
	return(device_id);
}

