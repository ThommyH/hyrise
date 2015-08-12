/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW MMAP API
 *
 * Filename: xsw_api_xke.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW XKE API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 11/10/14 Adrian      Created.
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
#include <sys/utsname.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "xsw_apis.h"
#include "xsw_internal.h"
#include <xsw/drivers/xsw_mmap_if.h>
#include <xsw/drivers/xsw_page_cache_if.h>

#define XKE_SYMBOL          " R sys_call_table"
#define XKE_SYMBOL_LEN      17
#define XSW_SYMBOL_ADDR_LEN 16

static int _xke_enabled = 0;

int XSWMmapDeviceIoctl(unsigned long int cmd, void *arg);
int XSWMmapSetupBacking(XSW_BACKING_DEVICE *backing, XSW_BIO *bio);

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeDebug                                                */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWXkeDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_XKE)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeInstall                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWXkeInstall(unsigned long sys_call_table)
{
struct utsname utsname;
char system_map[256];
int fd;
struct stat info;
char *data;
int i;

	// Make sure XKE is not already enabled
	if (_xke_enabled) {
		// Return success
		return(0);
	}
	// If user didn't pass in the address, try to find it using 
	// the system map file in the /boot directory.
	if (!sys_call_table) {
		// Get kernel info
		uname(&utsname);
		// Create a path to the kernel's system map file using release name
		sprintf(system_map, "/boot/System.map-%s", utsname.release);
		// Open system map file
		if ((fd = open(system_map, O_RDONLY)) < 0) {
			// Debug output
			XSWDebugKernel("XSWXkeInstall(): failed to open '%s'\n", system_map);
			// Return error
			return(-1);
		}
		// Get file info
		if (fstat(fd, &info)) {
			// Debug output
			XSWDebugKernel("XSWXkeInstall(): failed to fstat '%s'\n", system_map);
			// Close fd
			close(fd);
			// Return error
			return(-1);
		}
		// Does file have size?
		if (info.st_size > (XKE_SYMBOL_LEN+XSW_SYMBOL_ADDR_LEN)) {
			// Allocate buffer to hold file data
			data = XSWInternalMalloc(info.st_size);
			// Read in data
			if (pread(fd, data, info.st_size, 0) != info.st_size) {
				// Close file
				close(fd);
				// Free data
				XSWInternalFree(data);
				// Error reading
				XSWDebugKernel("XSWXkeInstall(): error reading '%s'\n", system_map);
				// Return error
				return(-1);
			}
			// Find sys_call_table
			for (i=0; i<info.st_size-(XKE_SYMBOL_LEN+XSW_SYMBOL_ADDR_LEN); i++) {
				// Did we find the string?
				if (!bcmp(XKE_SYMBOL, &data[i], XKE_SYMBOL_LEN)) {
					// Get value
					sys_call_table = strtoul(&data[i-XSW_SYMBOL_ADDR_LEN], 0, 16);
				}
			}
			// Free data
			XSWInternalFree(data);
		}
		// Close fd
		close(fd);
		// Did we fail to find the sys_call_table address?
		if (!sys_call_table) {
			// Debug output
			XSWDebugKernel("XSWXkeInstall(): failed to find sys_call_table in '%s'\n", system_map);
			// Return error
			return(-1);
		}
	}
	// Enable XKE using the sys_call_table address
	if (!XSWMmapDeviceIoctl(XSWMMAP_XKE_ENABLE, (void*)sys_call_table)) {
		// set enabled flag
		_xke_enabled = 1;
		// Return success
		return(0);
	}
	// Debug output
	XSWDebugKernel("XSWXkeInstall(): failed\n");
	// Return error
	return(-1);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeEnable                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWXkeEnable(char *process, char *search, XSW_BIO *swap, XSW_ID pcid)
{
XSW_MMAP_XKE xke;

	// Try to enable XKE
	if (XSWXkeInstall(0)) {
		// Debug output
		XSWXkeDebug("XSWXkeEnable(): XKE not enabled\n");
		// Return error
		return(-1);
	}
	// Setup version
	xke.version = XSWMMAP_VERSION;
	// Setup backing device
	if (XSWMmapSetupBacking(&xke.swap, swap)) {
		// Debug output
		XSWDebugKernel("XSWXkeEnable(): failed to setup swap backing\n");
		// Return error
		return(-1);
	}
	// Copy in process name
	strncpy(xke.comm, process, XSW_MAX_PATH);
	// Copy in search (if any)
	strncpy(xke.search, search ? search : "", MAX_XKE_SEARCH);
	// Copy in pcid
	xke.pcid = pcid;
	// Enable XKE using the sys_call_table address
	if (XSWMmapDeviceIoctl(XSWMMAP_XKE_ENABLE_PROC, &xke)) {
		// Debug output
		XSWXkeDebug("XSWXkeEnable(): failed to enable '%s'\n", process);
		// Return error
		return(-1);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeDisable                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWXkeDisable(char *process, char *search)
{
XSW_MMAP_XKE xke;

	// Setup version
	xke.version = XSWMMAP_VERSION;
	// Copy in process name
	strncpy(xke.comm, process, XSW_MAX_PATH);
	// Copy in search (if any)
	strncpy(xke.search, search ? search : "", MAX_XKE_SEARCH);
	// Query list of available mmap policies.
	if (XSWMmapDeviceIoctl(XSWMMAP_XKE_DISABLE_PROC, &xke)) {
		// Display debug output
		XSWXkeDebug("XSWXkeDisable(): ioctl(XSWMMAP_XKE_DISABLE_PROC) failed\n");
		// Return error
		return(-1);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeQuery                                                */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWXkeQuery(XSW_XKE_INFO *info, int *count)
{
int rc;
XSW_MMAP_GET_XKE xke;
int i;

	// Setup version
	xke.version = XSWMMAP_VERSION;
	// Setup alloc size
	xke.max_alloc = *count;
	// Allocate status
	xke.status = XSWInternalMalloc(*count * sizeof(XSW_XKE_STATUS));

	// Query mmap status.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_XKE_QUERY, &xke) < 0)) {
		// Display debug output
		XSWXkeDebug("XSWXkeQuery(): ioctl(XSWMMAP_XKE_QUERY) failed %d\n", rc);
		// Free status array
		XSWInternalFree(xke.status);
		// Zero out count
		*count = 0;
		// Return error
		return(rc);
	}
	// Copy kernel data into user data
	for (i=0; i<xke.returned; i++) {
		info[i].pcid = xke.status[i].pcid;

		strcpy(info[i].process,   xke.status[i].comm);
		strcpy(info[i].search,    xke.status[i].search);
		strcpy(info[i].swap,      xke.status[i].swap.pathname);
	}
	// Free status array
	XSWInternalFree(xke.status);
	// Return how many were actually returned
	*count = xke.returned;
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWXkeRemove                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWXkeRemove(void)
{
	// Clear enabled flag
	_xke_enabled = 0;
	// Disable XKE if it's really enabled
	return(XSWMmapDeviceIoctl(XSWMMAP_XKE_DISABLE, 0));
}


