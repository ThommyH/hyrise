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
 * Filename: xsw_api_pagecache.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Page Cache API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 11/19/14 Adrian      Created.
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

int XSWMmapDeviceIoctl(unsigned long int cmd, void *arg);
int XSWMmapSetupBacking(XSW_BACKING_DEVICE *backing, XSW_BIO *bio);

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
/* Function    : XSWMmapCopyInfo                                            */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWMmapCopyInfo(XSW_PAGE_CACHE_INFO *info, XSW_MMAP_PAGE_CACHE *page_cache)
{
	// Did user request info?
	if (info) {
		// Copy data from driver struct into user struct.
		info->pcid          = page_cache->pcid;
		info->cache_size    = page_cache->cache_size;
		info->low_water     = page_cache->low_water;
		info->eviction_size = page_cache->eviction_size;
		info->type          = page_cache->type;
		info->num_free      = page_cache->num_free;
		info->num_lru       = page_cache->num_lru;
		info->busy          = page_cache->busy;
		info->highest       = page_cache->highest;
		info->lowest        = page_cache->lowest;
		info->ioctid        = page_cache->ioctid;
		info->pctid         = page_cache->pctid;
		// Copy name
		strncpy(info->name, page_cache->name, XSWMMAP_MAX_CACHE_TAG_NAME);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapQueryPageCache                                      */
/* Scope       : Public API                                                 */
/* Description : Query a named page cache.                                  */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapQueryPageCache(XSW_ID pcid, XSW_PAGE_CACHE_INFO *info)
{
int rc;
XSW_MMAP_PAGE_CACHE page_cache;

	// Zero out info (if requested)
	if (info) {
		// Zero out info
		memset(info, 0, sizeof(XSW_PAGE_CACHE_INFO));
	}
	// Zero out request
	memset(&page_cache, 0, sizeof(page_cache));
	// Setup version
	page_cache.version = XSWMMAP_VERSION;
	// Setup the page id to read.
	page_cache.pcid = pcid;
	// Query list of available mmap policies.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_GET_PAGE_CACHE, &page_cache))) {
		// Display debug output
		XSWMmapDebug("XSWMmapQueryPageCache(): ioctl(XSWMMAP_GET_PAGE_CACHE) failed %d\n", rc);
		// Return error
		return(rc);
	}
	// Copy info
	XSWMmapCopyInfo(info, &page_cache);
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapGetPageCache                                        */
/* Scope       : Public API                                                 */
/* Description : Get a page cache.                                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapGetPageCaches(XSW_PAGE_CACHE_INFO *info, int *count)
{
int i;
int rc;
XSW_MMAP_GET_PAGE_CACHE request;

	// Setup version
	request.version = XSWMMAP_VERSION;
	// Setup alloc size
	request.max_alloc = *count;
	// Allocate status
	request.page_cache = XSWInternalMalloc(*count * sizeof(XSW_MMAP_PAGE_CACHE));
	// Query mmap status.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_GET_PAGE_CACHES, &request))) {
		// Display debug output
		XSWMmapDebug("XSWMmapGetPageCaches(): ioctl(XSWMMAP_GET_PAGE_CACHES) failed %d\n", rc);
		// Free status array
		XSWInternalFree(request.page_cache);
		// Zero out count
		*count = 0;
		// Return error
		return(rc);
	}
	// Copy kernel data into user data
	for (i=0; i<request.returned; i++) {
		// Copy info
		XSWMmapCopyInfo(&info[i], &request.page_cache[i]);
	}
	// Free status array
	XSWInternalFree(request.page_cache);
	// Return how many were actually returned
	*count = request.returned;
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapCreatePageCache                                     */
/* Scope       : Public API                                                 */
/* Description : Create a page cache.                                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_ID XSWMmapCreatePageCache(char *name, u64 cache_size, u64 low_water, u64 eviction_size, int type) 
{
XSW_ID pcid;
XSW_MMAP_PAGE_CACHE page_cache;

	// Setup Request
	page_cache.version       = XSWMMAP_VERSION;
	page_cache.cache_size    = cache_size;
	page_cache.low_water     = low_water;
	page_cache.eviction_size = eviction_size;
	page_cache.type          = (unsigned char)type;
	// Copy name
	strncpy(page_cache.name, name, XSWMMAP_MAX_CACHE_TAG_NAME);
	// Create a new page cache if this one doesn't already exist.
	if (!(pcid=XSWMmapDeviceIoctl(XSWMMAP_NEW_PAGE_CACHE, &page_cache))) {
		// Debug output
		XSWMmapDebug("XSWMmapCreatePageCache(): ioctl(XSWMMAP_NEW_PAGE_CACHE) failed\n");
	}
	// Return the page cache id
	return(pcid);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapResizePageCache                                     */
/* Scope       : Public API                                                 */
/* Description : Resize a page cache.                                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapResizePageCache(XSW_ID pcid, u64 cache_size, u64 low_water, u64 eviction_size) 
{
int rc;
XSW_MMAP_PAGE_CACHE page_cache;

	// Setup Request
	page_cache.version       = XSWMMAP_VERSION;
  	page_cache.cache_size    = cache_size;
  	page_cache.low_water     = low_water;
	page_cache.eviction_size = eviction_size;
	page_cache.pcid          = pcid;
	// Query list of available mmap policies.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_RESIZE_PAGE_CACHE, &page_cache))) {
		// Debug output
		XSWMmapDebug("XSWMmapResizePageCache(): ioctl(XSWMMAP_RESIZE_PAGE_CACHE) failed %d\n", rc);
		// Return error
		return(rc);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapGetPageCache                                        */
/* Scope       : Public API                                                 */
/* Description : Get a page cache.                                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_ID XSWMmapGetPageCache(char *name)
{
int rc;
XSW_MMAP_PAGE_CACHE page_cache;

	// Zero out request
	memset(&page_cache, 0, sizeof(page_cache));
	// Setup Request
	page_cache.version = XSWMMAP_VERSION;
	// Copy name
	strncpy(page_cache.name, name, XSWMMAP_MAX_CACHE_TAG_NAME);
	// Get page cache.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_GET_PAGE_CACHE, &page_cache))) {
		// Debug output
		XSWMmapDebug("XSWMmapGetPageCache(): ioctl(XSWMMAP_GET_PAGE_CACHE) failed %d\n", rc);
		// Return error
		return(0);
	}
	// Return page cache id
	return(page_cache.pcid);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapDeletePageCache                                     */
/* Scope       : Public API                                                 */
/* Description : Delete a page cache.                                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapDeletePageCache(XSW_ID pcid)
{
int rc;
XSW_MMAP_PAGE_CACHE page_cache;

	// Setup Request
	page_cache.version = XSWMMAP_VERSION;
	page_cache.pcid    = pcid;
	// Query list of available mmap policies.
	if ((rc=XSWMmapDeviceIoctl(XSWMMAP_DEL_PAGE_CACHE, &page_cache))) {
		// Debug output
		XSWMmapDebug("XSWMmapDeletePageCache(): ioctl(XSWMMAP_DEL_PAGE_CACHE) failed %d\n", rc);
		// Return error
		return(rc);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapInsertPageCacheTier                                 */
/* Scope       : Public API                                                 */
/* Description : Insert a page cache tier.                                  */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_ID XSWMmapInsertPageCacheTier(XSW_ID pcid, int type, XSW_ID insert, XSW_BIO *bio)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;

	// Setup Request
	page_cache_tier.version  = XSWMMAP_VERSION;
	page_cache_tier.type     = type;
	page_cache_tier.lru_type = XSW_PAGE_CACHE_LRU;
	page_cache_tier.pcid     = pcid;
	page_cache_tier.next_id  = insert;

	// Setup backing device
	if (XSWMmapSetupBacking(&page_cache_tier.backing, bio)) {
		// Debug output
		XSWDebugKernel("XSWMmapInsertPageCacheTier(): failed to setup bio\n");
		// Return error
		return(0);
	}
	// Insert Page cache tier
	if (XSWMmapDeviceIoctl(XSWMMAP_INSERT_PCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapInsertPageCacheTier(): ioctl(XSWMMAP_INSERT_PCT) failed\n");
		// Return error
		return(0);
	}
	// Return tier ID
	return(page_cache_tier.id);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapRemovePageCacheTier                                 */
/* Scope       : Public API                                                 */
/* Description : Remove a page cache tier.                                  */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapRemovePageCacheTier(XSW_ID pct)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;

	// Setup Request
	page_cache_tier.version = XSWMMAP_VERSION;
	page_cache_tier.id      = pct;
	// Remove page cache tier
	if (XSWMmapDeviceIoctl(XSWMMAP_REMOVE_PCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapRemovePageCacheTier(): ioctl(XSWMMAP_REMOVE_PCT) failed\n");
		// Return error
		return(-1);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapInsertIOCacheTier                                   */
/* Scope       : Public API                                                 */
/* Description : Insert a I/O cache tier.                                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_ID XSWMmapInsertIOCacheTier(XSW_ID pcid, int type, XSW_ID insert, XSW_BIO *bio)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;

	// Setup Request
	page_cache_tier.version  = XSWMMAP_VERSION;
	page_cache_tier.type     = type;
	page_cache_tier.lru_type = XSW_PAGE_CACHE_LRU;
	page_cache_tier.pcid     = pcid;
	page_cache_tier.next_id  = insert;

	// Setup backing device
	if (XSWMmapSetupBacking(&page_cache_tier.backing, bio)) {
		// Debug output
		XSWDebugKernel("XSWMmapInsertIOCacheTier(): failed to setup bio\n");
		// Return error
		return(0);
	}
	// Insert Page cache tier
	if (XSWMmapDeviceIoctl(XSWMMAP_INSERT_IOCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapInsertIOCacheTier(): ioctl(XSWMMAP_INSERT_IOCT) failed\n");
		// Return error
		return(0);
	}
	// Return tier ID
	return(page_cache_tier.id);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapRemoveIOCacheTier                                   */
/* Scope       : Public API                                                 */
/* Description : Remove a I/O cache tier.                                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapRemoveIOCacheTier(XSW_ID ioct)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;

	// Setup Request
	page_cache_tier.version = XSWMMAP_VERSION;
	page_cache_tier.id      = ioct;
	// Remove I/O tier
	if (XSWMmapDeviceIoctl(XSWMMAP_REMOVE_IOCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapRemoveIOCacheTier(): ioctl(XSWMMAP_REMOVE_IOCT) failed\n");
		// Return error
		return(-1);
	}
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapQueryIOCacheTier                                    */
/* Scope       : Public API                                                 */
/* Description : Query a I/O cache tier.                                    */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapQueryIOCacheTier(XSW_ID ioct, XSW_PAGE_CACHE_TIER *tier)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;
int i;

	// Setup Request
	page_cache_tier.version = XSWMMAP_VERSION;
	page_cache_tier.id      = ioct;
	// Query I/O tier
	if (XSWMmapDeviceIoctl(XSWMMAP_QUERY_IOCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapQueryIOCacheTier(): ioctl(XSWMMAP_QUERY_IOCT) failed\n");
		// Return error
		return(-1);
	}
	// Copy in page cache tier info
	tier->type     = page_cache_tier.type;
	tier->pcid     = page_cache_tier.pcid;
	tier->id       = page_cache_tier.id;
	tier->next_id  = page_cache_tier.next_id;
	// Copy stats
	for (i=0; i<XSW_IOC_NUM_STATS; i++)
		tier->stats[i] = page_cache_tier.stats[i];
	// Copy backing info
	tier->backing.virt_addr = page_cache_tier.backing.virt_addr;
	tier->backing.size      = page_cache_tier.backing.size;
	tier->backing.offset    = page_cache_tier.backing.offset;
	tier->backing.type      = page_cache_tier.backing.type;
	// Copy backing path
	strcpy(tier->backing.pathname, page_cache_tier.backing.pathname);
	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMmapQueryPageCacheTier                                  */
/* Scope       : Public API                                                 */
/* Description : Query a page cache tier.                                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWMmapQueryPageCacheTier(XSW_ID pct, XSW_PAGE_CACHE_TIER *tier)
{
XSW_MMAP_PAGE_CACHE_TIER page_cache_tier;
int i;

	// Setup Request
	page_cache_tier.version = XSWMMAP_VERSION;
	page_cache_tier.id      = pct;
	// Query Page cache tier
	if (XSWMmapDeviceIoctl(XSWMMAP_QUERY_PCT, &page_cache_tier)) {
		// Debug output
		XSWMmapDebug("XSWMmapQueryPageCacheTier(): ioctl(XSWMMAP_QUERY_PCT) failed\n");
		// Return error
		return(-1);
	}
	// Copy in page cache tier info
	tier->type     = page_cache_tier.type;
	tier->pcid     = page_cache_tier.pcid;
	tier->id       = page_cache_tier.id;
	tier->next_id  = page_cache_tier.next_id;
	// Copy stats
	for (i=0; i<XSW_IOC_NUM_STATS; i++)
		tier->stats[i] = page_cache_tier.stats[i];
	// Copy backing info
	tier->backing.virt_addr = page_cache_tier.backing.virt_addr;
	tier->backing.size      = page_cache_tier.backing.size;
	tier->backing.offset    = page_cache_tier.backing.offset;
	tier->backing.type      = page_cache_tier.backing.type;
	// Copy backing path
	strcpy(tier->backing.pathname, page_cache_tier.backing.pathname);
	// Return success
	return(0);
}

