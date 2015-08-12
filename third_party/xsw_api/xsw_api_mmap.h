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
 * Filename: xsw_api_mmap.h
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

#ifndef __XSW_API_MMAP_H
#define __XSW_API_MMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef u16 XSW_COLOR;
typedef u16 XSW_ID;

typedef struct XSW_MMAP_BACKING_T {
	void *virt_addr;
	u64   size;
	u64   offset;
	int   type;
	char  pathname[XSW_MAX_PATH];
} XSW_MMAP_BACKING;

typedef struct XSW_MMAP_INFO_T {
	u32    pid;                 
	XSW_ID pcid;
	u64    size;                
	u64    mapped;              
	u64    num_zerofills;              
	u64    num_writes;          
	u64    num_reads;           
	u64    num_syncs;           
	int    flags;               
	int    pending_flushes;     
	int    pending_updates;     
	u64    major_faults;        
	u64    minor_faults;        
	int    min_ra;              
	int    max_ra;              
	int    ra_misses;           
	int    ra_hits;             
	XSW_MMAP_BACKING backing;
	XSW_MMAP_BACKING swap;
} XSW_MMAP_INFO;

#define XSW_MMAP_POLICY_MASK   0x0f
#define XSW_MMAP_FILE          0x01
#define XSW_MMAP_DATABASE      0x02
#define XSW_MMAP_ANONYMOUS     0x03
#define XSW_MMAP_CUSTOM1       0x04
#define XSW_MMAP_CUSTOM2       0x05
#define XSW_MMAP_CUSTOM3       0x06
#define XSW_MMAP_CUSTOM4       0x07
#define XSW_MMAP_CUSTOM5       0x08
#define XSW_MMAP_CUSTOM6       0x09
#define XSW_MMAP_CUSTOM7       0x0a
#define XSW_MMAP_CUSTOM8       0x0b
#define XSW_MMAP_CUSTOM9       0x0c
#define XSW_MMAP_CUSTOM10      0x0d
#define XSW_MMAP_CUSTOM11      0x0e
#define XSW_MMAP_CUSTOM12      0x0f
#define XSW_MMAP_ADDR_ALIGN    0x10
#define XSW_MMAP_PRIVATE       0x20
#define XSW_MMAP_SHARED        0x40

#define XSW_MSYNC_SYNC         0x01
#define XSW_MSYNC_ASYNC        0x02
#define XSW_MSYNC_COW_PROTECT  0x04

#define XSW_IO_READ            1
#define XSW_IO_WRITE           2
#define XSW_IO_SYNC            3

#define XSW_PAGE_FLAGS_DIRTY         0x0000000100000000UL // 0 -Page is dirty
#define XSW_PAGE_FLAGS_UPTODATE      0x0000000200000000UL // 1 -Page is uptodate
#define XSW_PAGE_FLAGS_WRITING       0x0000000400000000UL // 2 -Page is being written
#define XSW_PAGE_FLAGS_EVICTING      0x0000000800000000UL // 3 -Page is being evicted
#define XSW_PAGE_FLAGS_LRU           0x0000001000000000UL // 4 -Page is in the LRU queue
#define XSW_PAGE_FLAGS_FREE          0x0000002000000000UL // 5 -Page is in the FREE queue
#define XSW_PAGE_FLAGS_PROTECT       0x0000004000000000UL // 6 -Page is write protected
#define XSW_PAGE_FLAGS_SWAP          0x0000008000000000UL // 7 -Page is swap allocated
#define XSW_PAGE_FLAGS_COWED         0x0000010000000000UL // 8 -Page has been COWed
#define XSW_PAGE_FLAGS_SYNC          0x0000020000000000UL // 9 -Page is syncing
#define XSW_PAGE_FLAGS_IO            0x0000040000000000UL // 10-Page is in I/O
#define XSW_PAGE_FLAGS_DISCARD       0x0000080000000000UL // 11-Page is discarded
#define XSW_PAGE_FLAGS_SOFT_DIRTY    0x0000100000000000UL // 12-Page is dirty
#define XSW_PAGE_FLAGS_BUSY          0x0000200000000000UL // 13-Page is busy, don't free
#define XSW_PAGE_FLAGS_CLEAN         0x0000400000000000UL // 14-Page is clean but should be written anyway
#define XSW_PAGE_FLAGS_RESERVED2     0x0000800000000000UL // 15-Reserved

XSW_BIO *XSWMmapOpen           (void *addr, u64 size, XSW_BIO *device, XSW_BIO *swap, XSW_ID pcid, int flags);
XSW_BIO *XSWMmapOpenMincore    (XSW_BIO *mmap);
int      XSWMmapQuery          (XSW_MMAP_INFO *info, int *count);
int      XSWMmapAdvise         (XSW_BIO *mmap, u64 offset, u64 size, int advise); 
int      XSWMmapMincore        (void *addr);
int      XSWMmapMsync          (XSW_BIO *mmap, u64 offset, u64 size, int mode);
int      XSWMmapSetupReadAhead (XSW_BIO *mmap, int minimum, int maximum);
int      XSWMmapSetupMaxBurst  (XSW_BIO *mmap, int max_burst);
int      XSWMmapSetupPdflush   (XSW_BIO *mmap, u64 freq_ms, u64 max, int enable);
int      XSWMmapSetColor       (XSW_BIO *mmap, u64 offset, u64 size, XSW_COLOR color);
int      XSWMmapUnloadModule   (void);
int      XSWMmapResetQuery     (void);
int      XSWMmapIO             (XSW_BIO *mmap, void *page, u32 index, XSW_COLOR color, unsigned long flags, int operation);

#define XSW_ADVISE_NORMAL      0x01 // Default
#define XSW_ADVISE_EVICT       0x02 // Evict these pages 
#define XSW_ADVISE_DONTEVICT   0x04 // Pin these pages and never evict them
#define XSW_ADVISE_ZEROFILL    0x08 // Zero fill initial references (vs reading backing store)
#define XSW_ADVISE_RANDOM      0x10 // Random IO
#define XSW_ADVISE_SEQUENTIAL  0x20 // Sequential IO
#define XSW_ADVISE_DISCARD     0x40 // Dealloc/discard memory pages (ZEROFILL set)

#define XSW_PAGE_SIZE             4096UL
#define XSW_PAGE_SHIFT            12UL
#define XSW_PAGE_MASK             (XSW_PAGE_SIZE-1UL)
#define XSW_PAGE_OFFSET(addr)     ((addr) >> XSW_PAGE_SHIFT)
#define XSW_PAGES_TO_BYTES(pages) ((unsigned long)(pages) << XSW_PAGE_SHIFT)
#define XSW_BYTES_TO_PAGES(bytes) (((unsigned long)(bytes)+XSW_PAGE_MASK) >> XSW_PAGE_SHIFT)

#define XSW_1K  (1024UL)        // Kilo
#define XSW_1M  (XSW_1K*XSW_1K) // Mega
#define XSW_1G  (XSW_1M*XSW_1K) // Giga
#define XSW_1T  (XSW_1G*XSW_1K) // Tera
#define XSW_1P  (XSW_1T*XSW_1K) // Peta

#define XSW_KB(x) ((unsigned long)(x)*XSW_1K)
#define XSW_MB(x) ((unsigned long)(x)*XSW_1M)
#define XSW_GB(x) ((unsigned long)(x)*XSW_1G)
#define XSW_TB(x) ((unsigned long)(x)*XSW_1T)
#define XSW_PB(x) ((unsigned long)(x)*XSW_1P)

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_MMAP_H

