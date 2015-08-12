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
 * Filename: xsw_api_pagecache.h
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

#ifndef __XSW_API_PAGECACHE_H
#define __XSW_API_PAGECACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#define XSWMMAP_MAX_CACHE_TAG_NAME 256

typedef struct XSW_PAGE_CACHE_INFO_T {
	char          name[XSWMMAP_MAX_CACHE_TAG_NAME];
	XSW_ID        pcid;
	XSW_ID        ioctid;
	XSW_ID        pctid;
	u64           cache_size;
	u64           low_water;
	u64           eviction_size;
	XSW_COLOR     lowest;
	XSW_COLOR     highest;
	unsigned char type;
	unsigned long num_free;
	unsigned long num_lru;
	unsigned char busy;
} XSW_PAGE_CACHE_INFO;

#define XSW_IOC_STAT_READ_HITS            0
#define XSW_IOC_STAT_READ_MISSES          1
#define XSW_IOC_STAT_WRITE_HITS           2
#define XSW_IOC_STAT_WRITE_MISSES         3
#define XSW_IOC_STAT_SYNC_HITS            4
#define XSW_IOC_STAT_SYNC_MISSES          5
#define XSW_IOC_STAT_CLEAN_WRITE_HITS     6
#define XSW_IOC_STAT_CLEAN_WRITE_MISSES   7
#define XSW_IOC_STAT_EVICTIONS            8
#define XSW_IOC_STAT_EVICTION_RETRYS      9
#define XSW_IOC_STAT_OVERRUNS             10
#define XSW_IOC_STAT_ERRORS               11
#define XSW_IOC_STAT_ALLOC_FAILS          12

#define XSW_IOC_NUM_STATS                 13

typedef struct XSW_PAGE_CACHE_TIER_T {
	int              type;
	XSW_MMAP_BACKING backing;
	XSW_ID           pcid;
	XSW_ID           id;
	XSW_ID           next_id;
	u32              stats[XSW_IOC_NUM_STATS];
} XSW_PAGE_CACHE_TIER;

#define XSW_PAGE_CACHE_LRU      0x01
#define XSW_PAGE_CACHE_FIFO     0x02
#define XSW_PAGE_CACHE_PRIVATE  0x04

#define XSWMMAP_IOCT_WRITEBACK     1
#define XSWMMAP_IOCT_WRITEAROUND   2
#define XSWMMAP_IOCT_WRITETHROUGH  3
#define XSWMMAP_IOCT_VPC           4
#define XSWMMAP_IOCT_PASSTHROUGH   5

#define XSWMMAP_PCT_PPC            1
#define XSWMMAP_PCT_NUMA           2

XSW_ID   XSWMmapGetPageCache        (char *name);
int      XSWMmapGetPageCaches       (XSW_PAGE_CACHE_INFO *info, int *count);
int      XSWMmapQueryPageCache      (XSW_ID pcid, XSW_PAGE_CACHE_INFO *info);
int      XSWMmapDeletePageCache     (XSW_ID pcid);
int      XSWMmapResizePageCache     (XSW_ID pcid, u64 cache_size, u64 low_water, u64 eviction_size);
XSW_ID   XSWMmapCreatePageCache     (char *name, u64 cache_size, u64 low_water, u64 eviction_size, int type);
XSW_ID   XSWMmapInsertPageCacheTier (XSW_ID pcid, int type, XSW_ID insert, XSW_BIO *bio);
XSW_ID   XSWMmapInsertIOCacheTier   (XSW_ID pcid, int type, XSW_ID insert, XSW_BIO *bio);
int      XSWMmapRemoveIOCacheTier   (XSW_ID ioct);
int      XSWMmapRemovePageCacheTier (XSW_ID pct);
int      XSWMmapQueryIOCacheTier    (XSW_ID ioct, XSW_PAGE_CACHE_TIER *tier);
int      XSWMmapQueryPageCacheTier  (XSW_ID pct, XSW_PAGE_CACHE_TIER *tier);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_PAGECACHE_H

