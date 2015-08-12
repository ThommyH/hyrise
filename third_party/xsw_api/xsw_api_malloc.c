/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Malloc API
 *
 * Filename: xsw_api_malloc.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Malloc API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 10/20/14 Adrian      Created.
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
#include <stdarg.h>
#include <pthread.h>

#include "xsw_apis.h"

//#ifdef XSW_MALLOC_DEALLOC_HEAPS

#define XSW_ALLOC_ALIGN          32  // Force all allocations on cache line boundary
#define XSW_MIN_HEAP_WASTE       XSW_ALLOC_ALIGN
#define XSW_PAGE_AVAIL           (1<<31)
#define XSW_PAGE_COUNT(page)     (u32)((page) & ~((u32)XSW_PAGE_AVAIL))
#define XSW_ALLOC_OVERHEAD       (sizeof(XSW_HEAP)+sizeof(XSW_ALLOC)*2)
#define XSW_REGION_SIZE          (XSW_1M*128) // 128MB
#define XSW_MALLOC_MIN_HEAP_SIZE (XSW_1K*128) // 128k

#define XSW_REGION_PAGES         (XSW_REGION_SIZE/XSW_PAGE_SIZE)
#define XSW_BIG_ALLOC_THRESHOLD  XSW_REGION_SIZE

struct XSW_HEAP_T;
struct XSW_REGION_T;

#define XSW_TYPE_HEAP   1
#define XSW_TYPE_REGION 2
#define XSW_TYPE_MMAP   3

typedef struct XSW_ALLOC_T {
	u64 type : 16; // Type of alloc XSW_TYPE_xxx
	u64 size : 48; // Size
	union {
		struct XSW_HEAP_T   *heap;
		struct XSW_REGION_T *region;
	};
	struct XSW_ALLOC_T *next;
	struct XSW_ALLOC_T *prev;
	char data[0] __attribute__((aligned(XSW_ALLOC_ALIGN)));
} XSW_ALLOC;

typedef struct XSW_HEAP_T {
	u32                  pages;
	u32                  max_linear;
	struct XSW_REGION_T *region;
	struct XSW_HEAP_T   *next;
	struct XSW_HEAP_T   *prev;
	XSW_ALLOC            allocs[0] __attribute__((aligned(XSW_ALLOC_ALIGN)));
} XSW_HEAP;

typedef struct XSW_REGION_T {
	void *                user_pages;
	XSW_HEAP             *heap;
	u32                   max_linear;
	u32                   mmap_pages;
	struct XSW_REGION_T  *next;
	struct XSW_REGION_T  *prev;
	struct XSW_REGION_T **base;
	u32                   page_map[0] __attribute__((aligned(XSW_PAGE_SIZE)));
} XSW_REGION;

/****************************************************************************/
/*                                                                          */
/* Description : Region descriptor chain.                                   */
/*                                                                          */
/****************************************************************************/
static XSW_REGION *memory_regions;
/****************************************************************************/
/*                                                                          */
/* Description : Region lock.                                               */
/*                                                                          */
/****************************************************************************/
static pthread_mutex_t region_lock = PTHREAD_MUTEX_INITIALIZER; 

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemRescanHeapMaxLinear                                  */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemRescanHeapMaxLinear(XSW_HEAP *heap, XSW_ALLOC *alloc, u32 max_linear)
{
	// Walk heap allocs looking for the biggest linear one
	while(alloc) {
		// Is this alloc bigger than the current max_linear?
		if ((!alloc->type) && (alloc->size > max_linear))
			max_linear = (u32)alloc->size;
		// Move to the next alloc
		alloc=alloc->next;
	}
	// Set the new max_linear in the heap header
	heap->max_linear = max_linear;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemRescanRegionMaxLinear                                */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemRescanRegionMaxLinear(XSW_REGION *region, u32 walk, u32 max_linear)
{
u32 count;

	// Search for an available chunk
	while(walk < XSW_REGION_PAGES) {
		// Get count for this group
		count = XSW_PAGE_COUNT(region->page_map[walk]);
		// Is this free chunk bigger than the current max_linear?
		if ((count > max_linear) && (region->page_map[walk] & XSW_PAGE_AVAIL)) {
			// Adjust new max
			max_linear = count;
		}
		// Move to the next chunk
		walk+= count;
	}
	// Set the new max_linear in the region header
	region->max_linear = max_linear;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemMallocRegionPages                                    */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void *XSWMemMallocRegionPages(XSW_REGION *region, u32 pages)
{
u32 walk;
u32 count;
u32 extra;
u32 max_linear = 0;

	// Does this region have enough pages to satisify this request?
	if (region->max_linear >= pages) {
		// Start search from beginning
		walk = 0;
		// Search for an available chunk
		while(walk < XSW_REGION_PAGES) {
			// Get count for this group
			count = XSW_PAGE_COUNT(region->page_map[walk]);
			// Is this chunk big enough and available?
			if (region->page_map[walk] & XSW_PAGE_AVAIL) {
				// Is there enough free pages here?
				if (count >= pages) {
					// Calculate extra (if any)
					if ((extra = (count-pages))) {
						// Adjust free region size
						region->page_map[walk] = XSW_PAGE_AVAIL|extra; 
						// Create a new tail for this chunk
						region->page_map[walk+extra-1] = XSW_PAGE_AVAIL|extra;
						// If this is the biggest region we've seen so far, record it
						if (extra > max_linear) {
							// Setup new max linear
							max_linear = extra;
						}
						// Adjust walk
						walk += extra;
					}
					// Create a new head for this allocation
					region->page_map[walk] = pages; 
					// Create a new tail for this allocation
					region->page_map[walk+pages-1] = pages; 
					// Check to see if we should re-scan for max_linear.
					if (count == region->max_linear) {
						// Rescan for max linear
						XSWMemRescanRegionMaxLinear(region, walk+pages, max_linear);
					}
					// Return new address 
					return(region->user_pages+XSW_PAGES_TO_BYTES(walk));
				}
				// If this is the biggest region we've seen so far, record it
				if (count > max_linear) {
					// Setup new max linear
					max_linear = count;
				}
			}
			// Move to the next chunk
			walk+= count;
		}
		// This should never happen
		XSWDebugKernel("XSWMemMallocRegionPages() failed\n");
	}
	// Out of memory in this region
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemFreeRegionPages                                      */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemFreeRegionPages(XSW_REGION *region, void *addr, u32 pages)
{
u32 start = XSW_PAGE_OFFSET(addr - region->user_pages);
u32 new_start = start;
u32 new_pages = pages;

	// Check if the previous chunk is also free
	if (start) {
		// Is previous chunk available?
		if (region->page_map[start-1] & XSW_PAGE_AVAIL) {
			// Adjust new start
			new_start -= XSW_PAGE_COUNT(region->page_map[start-1]);
			// Increase our size
			new_pages += XSW_PAGE_COUNT(region->page_map[start-1]);
		}
	}
	// Adjust start to sit on next chunk
	start += pages;
	// Check if there is really another chunk following this one
	if (start < XSW_REGION_PAGES) {
		// Is the following chunk also available?
		if (region->page_map[start] & XSW_PAGE_AVAIL) {
			// Increase our size
			new_pages += XSW_PAGE_COUNT(region->page_map[start]);
		}
	}
	// Create a new head for the free chain
	region->page_map[new_start] = XSW_PAGE_AVAIL|new_pages;
	// Create a new tail for the free chain
	region->page_map[new_start+new_pages-1] = XSW_PAGE_AVAIL|new_pages;
	// Are we now the largest linear free block?
	if (new_pages > region->max_linear) {
		// If region is now empty, deallocate it.
		if (new_pages == XSW_REGION_PAGES) {
			// Disconnect next (if any)
			if (region->next) 
				region->next->prev = region->prev;
			// Disconnect prev (if any)
			if (region->prev) 
   			   	region->prev->next = region->next;
			else {
				// Adjust base
	   			*region->base = region->next;
			}
			// Add region to cache
			munmap(region, XSW_PAGES_TO_BYTES(region->mmap_pages));
			// We're done
			return;
		} 
		// Adjust new max linear
		region->max_linear = new_pages;
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemDestroyHeap                                          */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemDestroyHeap(XSW_HEAP *heap)
{
	// Disconnect next (if any) 
	if (heap->next) 
		heap->next->prev = heap->prev;
	// Disconenct prev (if any)
	if (heap->prev) 
		heap->prev->next = heap->next;
	else
   		// Assign new base
   		heap->region->heap = heap->next;
	// Dealloc the physical pages for this heap from the region
	XSWMemFreeRegionPages(heap->region, heap, heap->pages);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemAllocFromHeap                                        */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void *XSWMemAllocFromHeap(XSW_HEAP *heap, size_t bytes)
{
u8 rescan;
XSW_ALLOC *alloc;
XSW_ALLOC *new;
u32 max_linear=0;

	// Setup a pointer to the allocs within the heap
	alloc = heap->allocs;
	// Walk allocations within this heap
	while(alloc) {
		// Is this an available block of memory and is it big enough?
		if (!alloc->type) {
			// Is there enough free space in this alloc?
			if (alloc->size >= bytes) {
				// Should we rescan for max_linear? If so, set a flag to do it later.
				rescan = (heap->max_linear == (u32)alloc->size);
				// Should we split this block and create a new Free header?
				if ((alloc->size - bytes) >= (sizeof(XSW_ALLOC) + XSW_MIN_HEAP_WASTE)) {
					// Truncate size of free chunk 
					alloc->size -= (bytes+sizeof(XSW_ALLOC));
					// Is this the truncated size still the biggest?
					if (alloc->size > max_linear) {
						// Adjust for new biggest
						max_linear = (u32)alloc->size;
					}
					// Create a new chunk to hold the allocated data
					new = (void*)alloc->data + alloc->size;
					// Setup heap
					new->heap = heap;
					// Setup size
					new->size = bytes;
					// Setup prev
					new->prev = alloc;
					// Setup next
					if ((new->next = alloc->next)) {
						// Adjust next's prev pointer to point to us
						new->next->prev = new;
					}
					// Setup alloc's next
					alloc->next = new;
					// Switch new to alloc
					alloc = new;
				}
				// Mark this alloc as active
				alloc->type = XSW_TYPE_HEAP;
				// Check flag to see if we should re-scan for max_linear.
				if (rescan) {
					// Rescan for max linear
					XSWMemRescanHeapMaxLinear(heap, alloc->next, max_linear);
				}
				// Return a pointer to the data
				return(alloc->data);
			}
			// Is this the biggest free block we've seen so far?
			if (alloc->size > max_linear) {
				// Adjust for new biggest
				max_linear = (u32)alloc->size;
			}
		}
		// Continue to the next alloc.
		alloc = alloc->next;
	}
	// This should never happen
	XSWDebugKernel("XSWMemAllocFromHeap() failed\n");
	// Out of memory
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemCreateRegion                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static XSW_REGION *XSWMemCreateRegion(void)
{
XSW_REGION *region;
u32 page_map_pages;
u32 mmap_pages;

	// Calculate how many pages are needed for the page_map.
	page_map_pages = XSW_BYTES_TO_PAGES(XSW_REGION_PAGES * sizeof(u32));
	// Calculate number of mmap pages needed (User pages+map pages+header page)
	mmap_pages = XSW_REGION_PAGES + page_map_pages + 1;
	// Allocate a region
	region = mmap(0, XSW_PAGES_TO_BYTES(mmap_pages), PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON, -1, 0);
	// Check for mmap failure
	if (!region || region == MAP_FAILED) {
		XSWDebugKernel("mmap() failed to allocate heap\n");
		return(0);
	}
	// Calculate address of first user page
	region->user_pages = (void*)region->page_map + XSW_PAGES_TO_BYTES(page_map_pages);
	// Zero heap
	region->heap = 0;
	// Setup max linear pages
	region->max_linear = XSW_REGION_PAGES;
	// Setup mmap pages
	region->mmap_pages = mmap_pages;
	// Setup initial free page chunk
	region->page_map[0] = (u32)(XSW_PAGE_AVAIL|XSW_REGION_PAGES);
	// Create a new tail for the free chain
	region->page_map[XSW_REGION_PAGES-1] = (u32)(XSW_PAGE_AVAIL|XSW_REGION_PAGES);
	// Setup prev
	region->prev = 0;
	// Setup next
	region->next = memory_regions;
	// Setup thread specific base for regions
	region->base = &memory_regions;
	// If there is currently a base, adjust it's prev pointer
	if (memory_regions) 
		memory_regions->prev = region;
	// Setup this region as the new base
	memory_regions = region;
	// Return the newly created region
	return(region);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemBigAlloc                                             */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void* XSWMemBigAlloc(size_t bytes)
{
XSW_ALLOC *alloc;

	// Allocate using anonymous memory
	alloc = mmap(0, bytes + sizeof(XSW_ALLOC), PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON, -1, 0);
	// Check for mmap failure
	if (!alloc || alloc == MAP_FAILED) {
		// Failed to create mmap
		return(0);
	}
	// Setup MMAP type
	alloc->type = XSW_TYPE_MMAP;
	// Setup size
	alloc->size = bytes + sizeof(XSW_ALLOC);
	// Return virtual address of user data area
	return(alloc->data);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemRegionAlloc                                          */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void *XSWMemRegionAlloc(XSW_REGION *region, size_t bytes)
{
XSW_ALLOC *alloc;
u32 pages;

	// Calculate how many physical pages needed for heap + overhead.
	pages = XSW_BYTES_TO_PAGES(bytes + sizeof(XSW_ALLOC));
	// Allocate the physical pages for the heap.
	if ((alloc = XSWMemMallocRegionPages(region, pages))) {
		// Setup the region alloc
		alloc->type   = XSW_TYPE_REGION;
		alloc->size   = bytes;
		alloc->region = region;
		// Return virtual address of user data area
		return(alloc->data);
	}
	// Return failure
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemCreateAllocHeap                                      */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void *XSWMemCreateAllocHeap(XSW_REGION *region, size_t bytes)
{
XSW_HEAP *heap;
u32 pages;
XSW_ALLOC *free;
XSW_ALLOC *alloc;

	// Calculate how many physical pages needed for heap + overhead.
	pages = XSW_BYTES_TO_PAGES(XSW_MALLOC_MIN_HEAP_SIZE + XSW_ALLOC_OVERHEAD);
	// Allocate the physical pages for the heap.
	if ((heap = XSWMemMallocRegionPages(region, pages))) {
		// Setup the heap structure
		heap->region     = region;
		heap->pages      = pages;
		heap->max_linear = XSW_PAGES_TO_BYTES(pages)-(XSW_ALLOC_OVERHEAD+bytes);
		// Setup a single free alloc structure.
		free = heap->allocs;
		// Setup free
		free->type  = 0;
		free->heap  = heap;
		free->size  = heap->max_linear;
		free->next  = (XSW_ALLOC*)(free->data+free->size);
		free->prev  = 0;
		// Setup a single alloc structure.
		alloc = free->next;
		// Setup alloc
		alloc->type = XSW_TYPE_HEAP;
		alloc->heap = heap;
		alloc->size = bytes;
		alloc->next = 0;
		alloc->prev = free;
		// Setup next pointer
		heap->next = region->heap;
		// Setup previous pointer
		heap->prev = 0;
		// If base currently exist, adjust base's prev to point to this new heap
		if (region->heap)
			region->heap->prev = heap;
		// Assign this heap as the new base
		region->heap = heap;
		// Return data
		return(alloc->data);
	}
	// Unable to allocate a heap from this region
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemAllocHeapData                                        */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void *XSWMemAllocHeapData(XSW_REGION *region, size_t bytes)
{
XSW_HEAP *heap;

	// Setup base pointer
	heap = region->heap;
	// Walk region heap
	while(heap) {
		// Is there room in this heap?
		if (heap->max_linear >= bytes) {
			// Allocate data from this heap
			return(XSWMemAllocFromHeap(heap, bytes));
   		} 
		// Move to the next heap
		heap = heap->next;
	}
	// Return out of memory
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemMallocRegion                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void* XSWMemMallocRegion(size_t bytes)
{
XSW_REGION *region;
void *data;

	// Setup base region
	if ((region = memory_regions)) {
		// Walk through all regions
		while(region) {
			// Is the size big enough to justify direct region allocation?
			if (bytes >= (size_t)XSW_MALLOC_MIN_HEAP_SIZE) {
				// Try and allocate directly from the region
				data = XSWMemRegionAlloc(region, bytes);
			} else {
				// Try to allocate from this region
				if (!(data = XSWMemAllocHeapData(region, bytes))) {
					// Create and try to allocate from a new heap in this region
					data = XSWMemCreateAllocHeap(region, bytes);
				}
			}
			// Did we allocate data?
			if (data) {
				// Return data
				return(data);
			}
			// Get next region
			region = region->next;
		}
	}
	// We need to add a new region
	if ((region = XSWMemCreateRegion())) {
		// Is the size big enough to justify direct region allocation?
		if (bytes >= (size_t)XSW_MALLOC_MIN_HEAP_SIZE) {
			// Try and allocate directly from the region
			data = XSWMemRegionAlloc(region, bytes);
		} else {
			// Create and try to allocate from a new heap in this region
			data = XSWMemCreateAllocHeap(region, bytes);
		}
	}
	// Return memory or NULL if out of memory
	return(data);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemMalloc                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void* XSWMemMalloc(size_t bytes)
{
void *data;

	// Is this a big allocation?
	if (bytes >= (size_t)XSW_BIG_ALLOC_THRESHOLD) {
		// Use MMAP
		return(XSWMemBigAlloc(bytes));
	}
	// Round request up to optimal align size
	if (!(bytes = ((bytes+(XSW_ALLOC_ALIGN-1)) & ~(XSW_ALLOC_ALIGN-1)))) {
		// Return if user passes 0 for bytes
		return(0);
	}

	// Acquire the TLS region lock
	pthread_mutex_lock(&region_lock);
	// Allocate data from a region
	data = XSWMemMallocRegion(bytes);
	// Release the region lock
	pthread_mutex_unlock(&region_lock);

	// Return data or NULL if out of memory
	return(data);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemTruncateHeapAlloc                                    */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemTruncateHeapAlloc(XSW_ALLOC *alloc, size_t size) 
{
XSW_ALLOC *free;

	// Should we bother to split this block and create a new Free header?
	if ((alloc->size - size) >= (sizeof(XSW_ALLOC) + XSW_MIN_HEAP_WASTE)) {
		// Setup new free alloc
		free = (void*)alloc->data+size;
		// Setup type
		free->type = 0;
		// Setup heap
		free->heap = alloc->heap;
		// Setup prev
		free->prev = alloc;
		// Setup size
		free->size = alloc->size - (size + sizeof(XSW_ALLOC));
		// Is next region free? If so, merge with that
		if (alloc->next && !alloc->next->type) {
			// Adjust our size to include the next free as well.
			free->size += (sizeof(XSW_ALLOC)+alloc->next->size);
			// Disconnect the next from the linked list.
			if ((free->next = alloc->next->next)) {
				// Adjust next's prev to point to us
				free->next->prev = free;
			}
		} else {
			// Setup free's next
			if ((free->next = alloc->next)) {
				// Adjust next's prev to poing to us.
				free->next->prev = free;
			}
		}
		// Are we now the largest linear free block?
		if (free->size > alloc->heap->max_linear) {
			// Adjust new max linear
			alloc->heap->max_linear = (u32)free->size;
		}
		// Change next
		alloc->next = free;
		// Adjust new size
		alloc->size = size;
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemTruncateRegionAlloc                                  */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemTruncateRegionAlloc(XSW_ALLOC *alloc, size_t size) 
{
u32 new_size = XSW_BYTES_TO_PAGES(size);
u32 old_size = XSW_BYTES_TO_PAGES(alloc->size);

	// Adjust new size
	alloc->size = XSW_PAGES_TO_BYTES(new_size);
	// Truncate region alloc
	XSWMemFreeRegionPages(alloc->region, (void*)alloc+alloc->size, old_size-new_size);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemTruncateMmap                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline void XSWMemTruncateMmap(XSW_ALLOC *alloc, size_t size)
{
u32 new_size = XSW_BYTES_TO_PAGES(size);
u32 old_size = XSW_BYTES_TO_PAGES(alloc->size);

	// Adjust new size
	alloc->size = XSW_PAGES_TO_BYTES(new_size);
	// Unmap the allocation
	munmap((void*)alloc+alloc->size, XSW_PAGES_TO_BYTES(old_size-new_size));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemRealloc                                              */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWMemTruncate(XSW_ALLOC *alloc, size_t size)
{
	// Acquire the TLS region lock
	pthread_mutex_lock(&region_lock);
	// Find allocation type
	switch(alloc->type) {
		// Check type of allocation this is
		switch(alloc->type) {
			// Small heap alloc
			case XSW_TYPE_HEAP:
				// Truncate the heap alloc
				XSWMemTruncateHeapAlloc(alloc, size);
				// We're done
				break;
			// Medium size region alloc
			case XSW_TYPE_REGION:
				// Truncate the region alloc
				XSWMemTruncateRegionAlloc(alloc, size);
				// We're done
				break;
			// Large MMAP alloc
			case XSW_TYPE_MMAP:
				// Truncate an MMAP alloc
				XSWMemTruncateMmap(alloc, size);
				// We're done
				break;
		}
	}
	// Release the region lock
	pthread_mutex_unlock(&region_lock);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemFreeAlloc                                            */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWMemFreeAlloc(XSW_ALLOC *alloc)
{
	// Should we combine the next free region? 
	if (alloc->next && !alloc->next->type) {
		// Adjust our size to include the next free as well.
		alloc->size += (sizeof(XSW_ALLOC)+alloc->next->size);
		// Disconnect the next from the linked list.
		if ((alloc->next = alloc->next->next)) {
			// Adjust next's prev to point to us
			alloc->next->prev = alloc;
		}
	}
	// Should we combine the prev free region?
	if (alloc->prev && !alloc->prev->type) {
		// Add our free space to the previous free block.
		alloc->prev->size += (sizeof(XSW_ALLOC)+alloc->size);
		// Disconnect our self from the linked list.
		if ((alloc->prev->next = alloc->next)) {
			// Setup next's prev to point to our prev
			alloc->next->prev = alloc->prev;
		}
		// Switch to the previous alloc.
		alloc = alloc->prev;
	}
	// Does this heap still have more than one item?
	if (alloc->next || alloc->prev) {
		// Make it available
		alloc->type = 0;
		// Are we now the largest linear free block?
		if (alloc->size > alloc->heap->max_linear) {
			// Adjust new max linear
			alloc->heap->max_linear = alloc->size;
		}
	} else {
		// Free this heap.
		XSWMemDestroyHeap(alloc->heap);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemCalloc                                               */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void* XSWMemCalloc(size_t nmemb, size_t size)
{
void *ptr;

	// Allocate pool
	if ((ptr = XSWMemMalloc(nmemb*size))) {
		// Zero out memory
		memset(ptr, 0, nmemb*size);
	}
	// Return pointer or NULL of out of memory
	return(ptr);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemRealloc                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void* XSWMemRealloc(void *ptr, size_t size)
{
void *new;
XSW_ALLOC *alloc;

	// Make sure a valid malloc was passed in
	if (ptr) {
		// Round size up to optimal align size
		if (!(size = ((size+(XSW_ALLOC_ALIGN-1)) & ~(XSW_ALLOC_ALIGN-1)))) {
			// Free memory
			XSWMemFree(ptr);
			// Return 0
			return(0);
		}
		// Walk back from the pointer to the alloc header
		alloc = (XSW_ALLOC*)(ptr - sizeof(XSW_ALLOC));
		// Check if the size is the same.
		if (size == alloc->size) {
			// Return original pointer
			return(ptr);
		}
		// Check if we're truncating the size
		if (size < alloc->size) {
			// Truncate size
			XSWMemTruncate(alloc, size);
			// Return original pointer
			return(ptr);
		}
		// If we did allocate memory, copy old in
		if ((new = XSWMemMalloc(size))) {
			// Copy old data
			memcpy(new, ptr, alloc->size);
			// Free old
			XSWMemFree(ptr);
		}
		// Return new or NULL
		return(new);
	}
	// Call standard malloc
	return(XSWMemMalloc(size));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWMemFree                                                 */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWMemFree(void *address)
{
XSW_ALLOC *alloc;

	// Acquire the TLS region lock
	pthread_mutex_lock(&region_lock);
	// Walk back from the pointer to the alloc header
	alloc = (XSW_ALLOC*)(address - sizeof(XSW_ALLOC));
	// Check type of allocation this is
	switch(alloc->type) {
		// Small heap alloc
		case XSW_TYPE_HEAP:
			// Free the allocation
			XSWMemFreeAlloc(alloc);
			// We're done
			break;
		// Medium size region alloc
		case XSW_TYPE_REGION:
			// Free region alloc
			XSWMemFreeRegionPages(alloc->region, alloc, 
				XSW_BYTES_TO_PAGES(sizeof(XSW_ALLOC)+alloc->size));
			// We're done
			break;
		// Large MMAP alloc
		case XSW_TYPE_MMAP:
			// Unmap the allocation
			munmap(alloc, alloc->size);
			// We're done
			break;
	}
	// Release the region lock
	pthread_mutex_unlock(&region_lock);
}
