/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW MMAP API
 *
 * Filename: xsw_api_mmap.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW MMAP API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
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

#define XSWMMAP_DEV_NODE "/dev/xswmmapctrl"

#define XSW_USER(user) ((u16)((unsigned long)(user)))

static int _mmap_fd_driver = -1;

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapDebug                                               */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWMmapDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_MMAP)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapDeviceIoctl                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapDeviceIoctl(unsigned long int cmd, void *arg)
{
int rc;

	// Check if driver handle is not already open
	if (_mmap_fd_driver < 0) {
		// Open ckpt ctrl device
		if ((_mmap_fd_driver = open(XSWMMAP_DEV_NODE, O_RDWR)) < 0) {
			XSWDebugKernel("XSWMmapDeviceIoctl(): driver open failed %d\n", _mmap_fd_driver);
			// Return error
			return(_mmap_fd_driver);
		}
	}
	// Issue device ioctl
	rc = ioctl(_mmap_fd_driver, cmd, (unsigned long)arg);
	// Return status
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapUnloadModule                                        */
/* Scope       : Public API                                                 */
/* Description : Unload the XtremSW MMAP kernel module.                     */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapUnloadModule(void)
{
	// Close MMAP driver (if open)
	if (_mmap_fd_driver >= 0) {
		// Close driver
		close(_mmap_fd_driver);
		// Poison handle
		_mmap_fd_driver = -1;
	}

	// Remove module
	return(XSWUnloadModule((char*)"xsw_mmap"));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSetupBacking                                        */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapSetupBacking(XSW_BACKING_DEVICE *backing, XSW_BIO *bio)
{
	// Confirm a backing device is passed in
	if (bio) {
		// Setup backing type
		backing->type = bio->type;
		// Setup phys_addr for DRAM type BIO
		backing->virt_addr = bio->mmap_vaddr;
		// Copy in the file descriptor.
		backing->fd = bio->fd;
		// Setup offset
		backing->offset = bio->offset;
		// Setup phys_size for device
		backing->size = bio->size;
		// Copy in pathname
		strcpy(backing->pathname, bio->pathname);
	} else {
		// Zero out backing device
		memset(backing, 0, sizeof(XSW_BACKING_DEVICE));
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapInfo                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWMmapInfo(XSW_BIO *bio)
{
	XSWMmapDebug("XSWMmapInfo(bio=0x%p) called\n", bio);

	XSWSysPrintf("XtremSW MMAP");

	if (bio->parent) {
		if (bio->parent->info) {
			XSWSysPrintf(" of ");
			bio->parent->info(bio->parent);
		}
	}
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapMsyncGroup                                          */
/* Scope       : Internal API                                               */
/* Description : Bulk msync                                                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWMmapMsyncGroup(XSW_BIO *group, u64 offset, u64 size, int mode)
{
int rc = 0;
XSW_MMAP_MSYNC_GROUP msync_ioctl_group;
XSW_BIO *bio = group->group_link;

	// Make sure group size is not zero
	if (!group->size) {
		XSWMmapDebug("XSWMmapMsyncGroup(group=0x%p, offset=%llu, size=%llu, sync=%d) Error: group empty\n", 
			group, offset, size, mode);
		return(-1);
	}

	// Setup msync.
	msync_ioctl_group.count  = 0;
	msync_ioctl_group.offset = offset;
	msync_ioctl_group.size   = size;
	msync_ioctl_group.sync   = mode;
	msync_ioctl_group.ids    = XSWInternalMalloc(group->size * sizeof(void*));

	// Walk though BIOs
	while(bio) {
		// Make sure this BIO is an MMAP
		if (bio->type == XSW_BIO_TYPE_MMAP) {
			// Save this user into group list
			msync_ioctl_group.ids[msync_ioctl_group.count++] = XSW_USER(bio->user);
		}
		// Move to next
		bio = bio->group_link;
	}

	// Check if we have a group sync
	if (msync_ioctl_group.count) {
		// Issue group MSYNC
		if ((rc = XSWMmapDeviceIoctl(XSWMMAP_MSYNC_GROUP, &msync_ioctl_group))) {
			XSWMmapDebug("XSWMmapMsyncGroup(bio=0x%p, offset=%llu, size=%llu, sync=%d) Failed %d\n", 
				group, offset, size, sync, rc);
		}
	}

	// Free the user array
	XSWInternalFree(msync_ioctl_group.ids);

	// Return ioctl status
	return(rc);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapMsync                                               */
/* Scope       : Public API                                                 */
/* Description : Flush a MMAP mapping.                                      */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapMsync(XSW_BIO *bio, u64 offset, u64 size, int mode)
{
int retCode;
XSW_MMAP_MSYNC msync_ioctl;

	// Handle group operation
	if (XSW_BIO_GROUP(bio)) 
		return(XSWMmapMsyncGroup(bio, offset, size, mode));

	XSWMmapDebug("XSWMmapMsync(bio=0x%p, offset=%llu, size=%llu, mode=%d) called\n", bio, offset, size, mode);

	// Setup msync.
	msync_ioctl.offset = offset;
	msync_ioctl.size   = size;
	msync_ioctl.sync   = mode;
	msync_ioctl.id     = XSW_USER(bio->user);

	// Issue MSYNC ioctl
	if ((retCode = XSWMmapDeviceIoctl(XSWMMAP_MSYNC, &msync_ioctl))) {
		XSWMmapDebug("XSWMmapMsync(bio=0x%p, offset=%llu, size=%llu, mode=%d) Failed %d\n", 
			bio, offset, size, mode, retCode);
	}

	// Return ioctl status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSync                                                */
/* Scope       : Private API                                                */
/* Description : Flush a MMAP mapping.                                      */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWMmapSync(XSW_BIO *bio, u64 offset, u64 size, int mode)
{
	return(XSWMmapMsync(bio, offset, size, mode == XSW_FSYNC_SYNC ? XSW_MSYNC_SYNC : XSW_MSYNC_ASYNC));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapTrim                                                */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWMmapTrim(XSW_BIO *bio, u64 offset, u64 size)
{
XSW_MMAP_ADVISE madvise;

	XSWMmapDebug("XSWMmapTrim(bio=0x%p, offset=%llu, size=%llu) called\n", bio,
		offset, size);

	// Setup advise.
	madvise.offset = offset;
	madvise.size   = size;
	madvise.advise = XSW_ADVISE_EVICT;
	madvise.id     = XSW_USER(bio->user);

	// Return ioctl status
	return(XSWMmapDeviceIoctl(XSWMMAP_ADVISE, &madvise));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapIO                                                  */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapIO(XSW_BIO *bio, void *page, u32 index, XSW_COLOR color, unsigned long flags, int operation)
{
XSW_MMAP_IO io;

	io.page      = page;
	io.color     = color;
	io.flags     = flags;
	io.index     = index;
	io.operation = operation;
	io.id        = XSW_USER(bio->user);

	// Return ioctl status
	return(XSWMmapDeviceIoctl(XSWMMAP_IO, &io));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapMincore                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapMincore(void *addr)
{
	return(XSWMmapDeviceIoctl(XSWMMAP_MINCORE, addr));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSetupPdflush                                        */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapSetupPdflush(XSW_BIO *bio, u64 freq_ms, u64 max, int enable)
{
XSW_MMAP_PDFLUSH pdflush;

	XSWMmapDebug("XSWMmapSetupPdflush(bio=0x%p, freq_ms=%llu, max=%llu, enable=%d) called\n", bio,
		freq_ms, max, enable);

	// Setup PDflush.
	pdflush.freq_ms = freq_ms;
	pdflush.max     = max;
	pdflush.enable  = enable;
	pdflush.id      = XSW_USER(bio->user);

	return(XSWMmapDeviceIoctl(XSWMMAP_SETUP_PDFLUSH, &pdflush));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSetColor                                            */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapSetColor(XSW_BIO *bio, u64 offset, u64 size, XSW_COLOR color)
{
XSW_MMAP_COLOR hint;

	XSWMmapDebug("XSWMmapSetColor(bio=0x%p, offset=%llu, size=%llu, color=%u) called\n", bio,
		offset, size, color);

	// Setup hint.
	hint.offset = offset;
	hint.size   = size;
	hint.weight = color;
	hint.id     = XSW_USER(bio->user);

	return(XSWMmapDeviceIoctl(XSWMMAP_SET_HINT, &hint));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapResetQuery                                          */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapResetQuery(void)
{
	// Issue reset stands command
	return(XSWMmapDeviceIoctl(XSWMMAP_RESET_MMAP_STATUS, 0));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapQuery                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapQuery(XSW_MMAP_INFO *info, int *count)
{
int rc;
XSW_MMAP_GET_STATUS status;
int i;

	// Setup version
	status.version   = XSWMMAP_VERSION;
	// Setup alloc size
	status.max_alloc = *count;
	// Allocate status
	status.status = XSWInternalMalloc(*count * sizeof(XSW_MMAP_STATUS));

	// Query mmap status.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_GET_MMAP_STATUS, &status) < 0)) {
		// Display debug output
		XSWMmapDebug("XSWMmapQuery(): ioctl(XSWMMAP_GET_MMAP_STATUS) failed %d\n", rc);
		// Free status array
		XSWInternalFree(status.status);
		// Zero out count
		*count = 0;
		// Return error
		return(rc);
	}
	// Copy kernel data into user data
	for (i=0; i<status.returned; i++) {
		info[i].pid             = status.status[i].pid;                 
		info[i].pcid            = status.status[i].pcid;                 
		info[i].size            = status.status[i].size;                
		info[i].mapped          = status.status[i].mapped;              
		info[i].num_writes      = status.status[i].num_writes;          
		info[i].num_syncs       = status.status[i].num_syncs;          
		info[i].num_zerofills   = status.status[i].num_zerofills;           
		info[i].num_reads       = status.status[i].num_reads;           
		info[i].flags           = status.status[i].flags;               
		info[i].pending_flushes = status.status[i].pending_flushes;     
		info[i].pending_updates = status.status[i].pending_updates;     
		info[i].major_faults    = status.status[i].major_faults;        
		info[i].minor_faults    = status.status[i].minor_faults;        
		info[i].min_ra          = status.status[i].min_ra;              
		info[i].max_ra          = status.status[i].max_ra;              
		info[i].ra_misses       = status.status[i].ra_misses;           
		info[i].ra_hits         = status.status[i].ra_hits;             
		// Copy backing info
		info[i].backing.virt_addr = status.status[i].backing.virt_addr;
		info[i].backing.size      = status.status[i].backing.size;
		info[i].backing.offset    = status.status[i].backing.offset;
		info[i].backing.type      = status.status[i].backing.type;
		// Copy swap info
		info[i].swap.virt_addr = status.status[i].swap.virt_addr;
		info[i].swap.size      = status.status[i].swap.size;
		info[i].swap.offset    = status.status[i].swap.offset;
		info[i].swap.type      = status.status[i].swap.type;
		// Copy paths
		strcpy(info[i].backing.pathname, status.status[i].backing.pathname);
		strcpy(info[i].swap.pathname,    status.status[i].swap.pathname);
	}
	// Free status array
	XSWInternalFree(status.status);
	// Return how many were actually returned
	*count = status.returned;
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSetupMaxBurst                                       */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapSetupMaxBurst(XSW_BIO *bio, int max_burst)
{
XSW_MMAP_BURST burst;
int retCode;

	XSWMmapDebug("XSWMmapSetupMaxBurst(bio=0x%p, max_burst=%d)\n", bio, max_burst);

	// Setup user
	burst.id = XSW_USER(bio->user);
	// Setup max burst
	burst.max_burst = max_burst;

	// Call into driver to setup max burst
	retCode = XSWMmapDeviceIoctl(XSWMMAP_SET_MAX_BURST, &burst);

	// Check for an error
	if (retCode)
		XSWDebugKernel("XSWMmapSetupMaxBurst(bio=0x%p) failed\n", bio);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapSetupReadAhead                                      */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapSetupReadAhead(XSW_BIO *bio, int minimum, int maximum)
{
XSW_MMAP_RA ra;
int retCode;

	XSWMmapDebug("XSWMmapSetupReadAhead(bio=0x%p, minimum=%d, maximum=%d)\n", bio, minimum, maximum);

	// Setup min
	ra.min = minimum;
	// Setup max
	ra.max = maximum;
	// Setup type (not used right now)
	ra.type = 0;
	// Setup user
	ra.id = XSW_USER(bio->user);

	// Call into driver to setup read ahead
	retCode = XSWMmapDeviceIoctl(XSWMMAP_SET_READ_AHEAD, &ra);

	// Check for an error
	if (retCode)
		XSWDebugKernel("XSWMmapSetupReadAhead(bio=0x%p) failed to set read ahead\n", bio);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapAdvise                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapAdvise(XSW_BIO *bio, u64 offset, u64 size, int advise)
{
XSW_MMAP_ADVISE mad;
int retCode;

	XSWMmapDebug("XSWMmapAdvise(bio=0x%p, offset=%llu, size=%llu, advise=%d) called\n", bio,
		offset, size, advise);

	// Setup address offset
	mad.offset = offset;
	// Setup size
	mad.size   = size;
	// Setup advise
	mad.advise = advise;
	// Setup user
	mad.id = XSW_USER(bio->user);

	// Call into driver to setup advise
	retCode = XSWMmapDeviceIoctl(XSWMMAP_ADVISE, &mad);

	// Check for an error
	if (retCode)
		XSWDebugKernel("XSWMmapAdvise(bio=0x%p) failed\n", bio);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapClose                                               */
/* Scope       : Private API                                                */
/* Description : Close a MMAP mapping.                                      */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWMmapClose(XSW_BIO *bio)
{
	XSWMmapDebug("XSWMmapClose(bio=0x%p) called\n", bio);
	// Unmap memory
	munmap(bio->mmap_vaddr, bio->size);
	// Free bio
	XSWFreeBIO(bio);
	// Return current error state.
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapOpen                                                */
/* Scope       : Public API                                                 */
/* Description : MMAP a block IO device                                     */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWMmapOpen(void *addr, u64 size, XSW_BIO *device, XSW_BIO *swap, XSW_ID pcid, int flags)
{
XSW_BIO *bio;
XSW_MMAP_CREATE create;

	XSWMmapDebug("XSWMmapOpen(addr=%p, size=%p, device=%p, swap=%p, pcid=%d, flags=%d) called\n", 
		addr, (void*)size, device, swap, pcid, flags);

	// Setup version
	create.version = XSWMMAP_VERSION;
	// Setup MMAP addr
	create.addr = (unsigned long)addr;
	// Setup MMAP size
	create.size = size;
	// Setup flags for MMAP.
	create.flags = flags;
	// Copy in pcid
	create.pcid = pcid;

	// Setup device backing
	if (XSWMmapSetupBacking(&create.device, device)) {
		// Display error
		XSWDebugKernel("XSWMmapOpen() setup device backing failed\n");
		// Return failure
		return(0);
	}
	// Setup swap backing
	if (XSWMmapSetupBacking(&create.swap, swap)) {
		// Display error
		XSWDebugKernel("XSWMmapOpen() setup swap backing failed\n");
		// Return failure
		return(0);
	}

	// Bind device and return a binding index used to open device
	if (XSWMmapDeviceIoctl(XSWMMAP_MMAP_CREATE, &create)) {
		// Display error
		XSWDebugKernel("XSWMmapOpen() ioctl(XSWMMAP_MMAP) failed\n");
		// Return failure
		return(0);
	}

	// Allocate a bio
	bio = XSWAllocBIO(device);
	// Get user
	bio->user = (void*)((unsigned long)create.id);
	// Get virtual address
	bio->mmap_vaddr = (void*)create.mmap_vaddr;
	// Setup misc BIO info
	bio->type           = XSW_BIO_TYPE_MMAP;
	bio->info           = XSWMmapInfo;
	bio->fsync          = XSWMmapSync;
	bio->close          = XSWMmapClose;
	bio->write          = 0;
	bio->read           = 0;
	bio->trim           = XSWMmapTrim;
	bio->size           = size;

	// Is this a device mapping?
	if (device) {
		bio->offset  = device->offset;
		bio->blksize = device->blksize;
		bio->fd      = device->fd;
	}
	// Return new bio
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapOpenMincore                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWMmapOpenMincore(XSW_BIO *mmap)
{
XSW_BIO *bio = 0;
XSW_MMAP_MINCORE mincore;

	XSWMmapDebug("XSWMmapOpenMincore(bio=0x%p)\n", mmap);

	// Setup version
	mincore.version = XSWMMAP_VERSION;
	// Setup user
	mincore.id = XSW_USER(mmap->user);

	// Call into driver to setup read ahead
	if (!XSWMmapDeviceIoctl(XSWMMAP_OPEN_MINCORE, &mincore)) {
		// Allocate a bio
		bio = XSWAllocBIO(mmap);
		// Get virtual address
		bio->mmap_vaddr = (void*)mincore.mmap_vaddr;
		// Setup misc BIO info
		bio->type  = XSW_BIO_TYPE_MMAP;
		bio->close = XSWMmapClose;
		bio->size  = mincore.size;
	}

	// Return new bio
	return(bio);
}

