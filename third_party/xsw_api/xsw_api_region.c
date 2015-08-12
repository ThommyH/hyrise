/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Region
 *
 * Filename: xsw_api_region.c
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the Linux XtremSW Region interface module.
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
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <assert.h>
#include <stdarg.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

#define REGION_HEAD_SIG 0xAA55A5A555AA5A5A
#define REGION_TAIL_SIG 0x55AA5A5AAA55A5A5

#define XSW_REGION_BUFFER_SIZE 1024*1024*32   // 32M Buffer for block IO

#define XSW_MAX_REGIONS 14

#define XSW_DEFAULT_BLOCK_SIZE 4096

#define XSW_REGION_TABLE_SIZE XSW_DEFAULT_BLOCK_SIZE

#define XSW_BLOCK_BUFFER_ALIGN 4096

typedef struct XSW_REGION_TABLE_BLOCK_T { __attribute__((packed))
	u64 region_head_sig;
	u64 disk_size;
	XSW_BLOCK_REGION region[XSW_MAX_REGIONS];
	int number_regions;
	u32 blksize;
	u64 region_tail_sig;
	u64 current_lba;
	u64 next_lba;
	u32 crc32;
} XSW_REGION_TABLE_BLOCK;

typedef struct XSW_REGION_TABLE_BLOCKS_T {
	XSW_REGION_TABLE_BLOCK *table;
	struct XSW_REGION_TABLE_BLOCKS_T *next;
	struct XSW_REGION_TABLE_BLOCKS_T *prev;
} XSW_REGION_TABLE_BLOCKS;

typedef struct XSW_REGION_TABLE_T { __attribute__((packed))
	u64 disk_size;
	XSW_BLOCK_REGION *region;
	int number_regions;
	u32 blksize;
	XSW_REGION_TABLE_BLOCKS *head;
	XSW_REGION_TABLE_BLOCKS *tail;
} XSW_REGION_TABLE;

static void (*_progress)(void);

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerDebug                                          */
/* Scope       : Private API                                                */
/* Description : Debug print                                                */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerDebug(const char *fmt, ...)
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_REGION)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerAllocBuffer                                    */
/* Scope       : Private API                                                */
/* Description : Allocate an optimally aligned transfer buffer.             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void *XSWContainerAllocBuffer(size_t size, unsigned long align)
{
	// Allocate size + alignment overhead
	return(XSWInternalMalloc(size+(size_t)align));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerAllocBuffer                                    */
/* Scope       : Private API                                                */
/* Description : Allocate an optimally aligned transfer buffer.             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void *XSWContainerBufferAlign(void *ptr, unsigned long align)
{
	// Drop lower bits.
	return((void*)((u64)(((unsigned long)ptr)+((u64)align-1)) & ~((u64)(align-1))));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerFreeBuffer                                     */
/* Scope       : Private API                                                */
/* Description : Free a transfer buffer.                                    */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerFreeBuffer(void *ptr)
{
	XSWInternalFree(ptr);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerOpenRegion                                     */
/* Scope       : Public API                                                 */
/* Description : Creates a BIO object for a region                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWContainerOpenRegion(XSW_BIO *bio, char *name)
{
XSW_BIO *child;
XSW_BLOCK_REGION region;

	// Load region
	if (XSWContainerGetRegion(bio, name, &region)) {
		return(0);
	}

	XSWContainerDebug("XSWContainerOpenRegion(bio=%p, name='%s') called\n", bio, name);

	// Allocate BIO
	child = XSWAllocBIO(bio);

	if (!child) {
		XSWContainerDebug("XSWContainerOpenRegion() out of memory\n");
		return(0);
	}

	// Copy in the device name
	sprintf(child->pathname, "%s/%s", bio->pathname, name);

	child->type           = bio->type;                 // Copy type
	child->info           = bio->info;                 // Info for raw device
	child->fsync          = bio->fsync;                // sync device
	child->write          = bio->write;                // Write device
	child->read           = bio->read;                 // Read device
	child->trim           = bio->trim;                 // Trim device
	child->offset         = region.offset+bio->offset; // Physical offset
	child->size           = region.size;               // Physical size 
	child->blksize        = region.blksize;            // Physical block size
	child->fd             = bio->fd;                   // Same device handle
	child->mmap_vaddr     = bio->mmap_vaddr;           // Copy vaddr

	return(child);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerResolveRegion                                  */
/* Scope       : Public API                                                 */
/* Description : Modify BIO to point to region.                             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWContainerResolveRegion(XSW_BIO *bio, char *name)
{
XSW_BLOCK_REGION region;
char  pathname[XSW_MAX_PATH];

	// Debug output
	XSWContainerDebug("XSWContainerResolveRegion(bio=%p, name='%s') called\n", bio, name);

	// Load region
	if (XSWContainerGetRegion(bio, name, &region)) {
		return(0);
	}

	// Create a new pathname
	sprintf(pathname, "%s/%s", bio->pathname, name);

	// Copy in modified pathname with appended region name
	strcpy(bio->pathname, pathname);

	// Adjust offset, size, and blksize.
	bio->offset         = region.offset+bio->offset; // Physical offset
	bio->size           = region.size;               // Physical size 
	bio->blksize        = region.blksize;            // Physical block size

	// Return modified BIO that now points to the region.
	return(bio);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerRoundSize                                      */
/* Scope       : Public API                                                 */
/* Description : Rounds the size to a granular of block size.               */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
u64 XSWContainerRoundSize(u64 size, u32 blksize)
{
	return((((u64)(size)+(u64)(blksize-1))/(u64)(blksize))*((u64)(blksize)));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerSetProgressCB                                  */
/* Scope       : Public API                                                 */
/* Description : Setup a callback to call when progress is being made.      */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerSetProgressCB(void (*progress)(void))
{
	// Setup progress callback handler
	_progress = progress;

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCRC32                                          */
/* Scope       : Private API                                                */
/* Description : http://en.wikipedia.org/wiki/Cyclic_redundancy_check       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static u32 XSWContainerCRC32(XSW_REGION_TABLE_BLOCK *table)
{
static u32 crc_table[256];
static int crc_init = 0;
u32 i,c,j;
u64 crc_len;

	crc_len = (u64)&table->crc32 - (u64)table;

	if (!crc_init) {
		for (i = 0; i < 256; i++) {
			c = i;
			for (j = 0; j < 8; j++) 
				c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
			crc_table[i] = c;
		}
		crc_init = 1;
	}

	c = 0xFFFFFFFF;

	// Calculate CRC32 up to (but not including) the crc32 member 
	for (i = 0; i < (u32)crc_len; i++) 
		c = crc_table[(c ^ ((u8*)table)[i]) & 0xFF] ^ (c >> 8);

	return(c ^ 0xFFFFFFFF);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableAdd                                       */
/* Scope       : Internal API                                               */
/* Description : Add a region block to the table.                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableAdd(XSW_REGION_TABLE *region_table, XSW_REGION_TABLE_BLOCK *table)
{
XSW_REGION_TABLE_BLOCKS *new_block;

	// Allocate a new table block holder
	new_block = XSWInternalMalloc(sizeof(XSW_REGION_TABLE_BLOCKS));

	new_block->table = table;
	new_block->prev  = region_table->tail;
	new_block->next  = 0;

	if (region_table->tail) 
		region_table->tail->next = new_block;

	if (!region_table->head)
		region_table->head = new_block;

	region_table->tail = new_block;

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableFree                                      */
/* Scope       : Internal API                                               */
/* Description : Load the region table from the device (if any).            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableFree(XSW_REGION_TABLE *table)
{
XSW_REGION_TABLE_BLOCKS *block, *next;

	// Start walking blocks from the head pointer
	block = table->head;

	// Free raw blocks
	while(block) {
		// Get pointer to next (if any)
		next = block->next;
		// Free current block table
		XSWInternalFree(block->table);
		// Free current block table holder
		XSWInternalFree(block);
		// Move to the next block (if any)
		block = next;
	}

	// Free regions array
	XSWInternalFree(table->region);

	// Free table
	XSWInternalFree(table);

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableLoadBlock                                 */
/* Scope       : Internal API                                               */
/* Description : Load the region table from the device (if any).            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableLoadBlock(XSW_BIO *bio, u64 lba, XSW_REGION_TABLE_BLOCK *table)
{
ssize_t bytes;
u32 crc32;

	// Read the region table from block 0 on the device.
	bytes = XSWRead(bio, lba, sizeof(XSW_REGION_TABLE_BLOCK), table);

	// Check to see if read failed.
	if (bytes < 0) {
		XSWContainerDebug("XSWContainerTableLoadBlock() failed: pread failed %d\n", bytes);
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_DEVICE);
	}

	// Check to see if read expected number of bytes.
	if (bytes != sizeof(XSW_REGION_TABLE_BLOCK)) {
		XSWContainerDebug("XSWContainerTableLoadBlock() failed: unable to read %d bytes (read only %d)\n",
			sizeof(XSW_REGION_TABLE_BLOCK), bytes);
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_DEVICE);
	}

	// Calculate CRC32
	if ((crc32 = XSWContainerCRC32(table)) != table->crc32) {
		XSWContainerDebug("XSWContainerTableLoadBlock() failed: CRC mismatch 0x%08x vs 0x%08x\n",
			crc32, table->crc32);
		// Return failure.
		return(XSW_CONTAINER_ERR_NO_REGION_TABLE);
	}
	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerInitBlock                                      */
/* Scope       : Internal API                                               */
/* Description : Create an empty block table.                               */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerInitBlock(XSW_REGION_TABLE_BLOCK *block, u64 lba, u64 disk_size, u32 blksize)
{
	// Zero out the region block.
	memset(block, 0, sizeof(XSW_REGION_TABLE_BLOCK));

	// Init the region header.
	block->region_head_sig  = REGION_HEAD_SIG;
	block->region_tail_sig  = REGION_TAIL_SIG;
	block->blksize          = blksize;
	block->disk_size        = disk_size;
	block->current_lba      = lba;
	block->next_lba         = 0;
	block->number_regions   = 0;

	// return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableInsert                                    */
/* Scope       : Private API                                                */
/* Description : Add a new region to the table.                             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableInsert(XSW_REGION_TABLE *table, u64 offset, 
	u64 size, u32 blksize, char *name, char state)
{
int i;

	// Round block size to default block size
	blksize = XSWContainerRoundSize(blksize, table->blksize);

	// Round offset to blocksize granularity.
	offset = XSWContainerRoundSize(offset, blksize);

	// Round size to blocksize granularity.
	size = XSWContainerRoundSize(size, blksize);

	// Look for the 1st empty slot, if there is one. 
	for (i=0; i<table->number_regions; i++) 
		if (table->region[i].state == REGION_STATE_EMPTY)
			break;

	// Copy in region data
	table->region[i].offset  = offset;
	table->region[i].size    = size;
	table->region[i].blksize = blksize;
	table->region[i].state   = state;

	// Copy in the region name, or "" if none specified. 
	strncpy(table->region[i].name, name ? name : "", MAX_REGION_NAME);

	// If we're inserting a new one, increase the number of regions.
	if (i == table->number_regions)
		table->number_regions++;

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerAlloc                                          */
/* Scope       : Internal API                                               */
/* Description : Allocate from a free region (region)                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerAlloc(XSW_REGION_TABLE *table, int region, char *name, 
	u64 size, u32 blksize, u8 state, u32 flags)
{
u64 offset, align, truncate;

	// Calculate aligned offset.
	offset = XSWContainerRoundSize(table->region[region].offset, blksize);

	// Calculate actual alignment errors (if any)
	align = offset - table->region[region].offset;

	// Calculate truncation if any
	truncate = (table->region[region].offset+table->region[region].size) -
		(offset + size);

	// Setup flags
	table->region[region].flags = flags;
	// Change this from a free to an allocated region.
	table->region[region].state = state;
	// Truncate the new free region's size for an exact fit. 
	table->region[region].size = size;
	// Adjust block size.
	table->region[region].blksize = blksize;
	// Update the region's name.
	strncpy(table->region[region].name, name, MAX_REGION_NAME);

	// If we adjusted the start offset based on blksize alignment
	// then create a free region for the alignment error.
	if (align) {
		// Try to insert a new FREE region for the remaining unused space.
		if (!XSWContainerTableInsert(table, table->region[region].offset, 
			align, table->blksize, 0, REGION_STATE_FREE)) 
			table->region[region].offset = offset;
	}

	// If we broke a free region, create a new region
	// for the un-used space.
	if (truncate) {
		// Try to insert a new FREE region for the remaining unused space.
		XSWContainerTableInsert(table, table->region[region].offset + size, 
			truncate, table->blksize, 0, REGION_STATE_FREE);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableMerge                                     */
/* Scope       : Private API                                                */
/* Description : Merge neighboring regions.                                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerTableMerge(XSW_REGION_TABLE *table, char state)
{
int search_again,i,j;

	do {
		// Do an N^ search for back-to-back regions that can be combined into
		// one larger region.
		for (search_again=0, i=0; i<table->number_regions; i++) {
			if (table->region[i].state == state) {
				for (j=0; j<table->number_regions; j++) {
					if (j !=i && table->region[j].state == state) {
						// Does region[j] start at the end of region[i]? If so
						// merge the regions
						if ((table->region[i].offset + table->region[i].size) == table->region[j].offset) {
							// Increase the size of region[i] by region[j]'s size
							table->region[i].size += table->region[j].size;
							// Delete region[j] now.
							table->region[j].state  = REGION_STATE_EMPTY;
							// Set the search again flag.
							search_again = 1;
						}
					}
				}
			}
		}
	} while(search_again);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableCompress                                  */
/* Scope       : Private API                                                */
/* Description : Compresses region table by removing empty entries.         */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerTableCompress(XSW_REGION_TABLE *table)
{
int i,j;

	// Compress empty regions by copying non-empty over them and re-adjusting the
	// total number of regions.
	for (j=0, i=0; i<table->number_regions; i++) {
		if (table->region[i].state != REGION_STATE_EMPTY) {
			// Copy current region[i] to compressed region[j] location.
			memcpy(&table->region[j++], &table->region[i], sizeof(XSW_BLOCK_REGION));
		}
	}
	// Update new number of regions based on compressed region[j] count.
	table->number_regions = j;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableOptimize                                  */
/* Scope       : Private API                                                */
/* Description : Merge neighboring free regions.                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerTableOptimize(XSW_REGION_TABLE *table)
{
	// Merge any FREE regions
	XSWContainerTableMerge(table, REGION_STATE_FREE);
	// Compress region table.
	XSWContainerTableCompress(table);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCreate                                         */
/* Scope       : Internal API                                               */
/* Description : Allocate and return a region.                              */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerAllocGet(XSW_REGION_TABLE *table, char *name, 
	u64 size, u32 blksize, u8 state, XSW_BLOCK_REGION *region, u32 flags)
{
int i, min_region = 0;
u64 min_leftover;

	// Setup min_leftover to be the biggest number possible.
	min_leftover = table->disk_size;

	// Look for the smallest region that fits the requested size. 
	for (i=0; i<table->number_regions; i++) {
		// Look for a free region
		if (table->region[i].state == REGION_STATE_FREE) {
			// If this region has enough space available for the request consider it.
			if (XSWContainerAlignSize(&table->region[i], blksize) >= size) {
				// Is this a better fit that any previous region found?
				if ((XSWContainerAlignSize(&table->region[i], blksize) - size) < min_leftover) {
					// Adjust the new best-fit region size
					min_leftover = XSWContainerAlignSize(&table->region[i], blksize) - size;
					// Adjust the new best-fit region index.
					min_region = i;
				}
			}
		}
	}

	// If we didn't find any available regions, fail
	if (min_leftover == table->disk_size) {
		XSWContainerDebug("XSWContainerCreate() failed: unable to create region '%s'\n", name);
		// Failed to create new region.
		return(XSW_CONTAINER_ERR_OUT_OF_SPACE);
	}

	// Alloc the region from this free region with an offset of 0
	XSWContainerAlloc(table, min_region, name, size, blksize, state, flags);

	// Copy in region info if requested
	if (region)
		memcpy(region, &table->region[min_region], sizeof(XSW_BLOCK_REGION));

	// Optimize the table-> The free region we just added
	// might be mergeable with other free regions.
	XSWContainerTableOptimize(table);

	// return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableLoad                                      */
/* Scope       : Internal API                                               */
/* Description : Load the region table from the device (if any).            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableLoad(XSW_BIO *bio, XSW_REGION_TABLE **region_table)
{
int retCode;
XSW_REGION_TABLE *table;
XSW_REGION_TABLE_BLOCK *block, *new_block;
XSW_REGION_TABLE_BLOCKS *walk;
XSW_BLOCK_REGION region;
int i;
int alloc=0;
int free=0;
u64 lba=0;

	// Allocate a region table
	table = XSWInternalMalloc(sizeof(XSW_REGION_TABLE));

	// Zero out table
	memset(table, 0, sizeof(XSW_REGION_TABLE));

	while(1) {
		// Allocate a table block
		block = XSWInternalMalloc(sizeof(XSW_REGION_TABLE_BLOCK));
		// Load the region table block at the current LBA
		if ((retCode=XSWContainerTableLoadBlock(bio, lba, block))) {
			// Free region table
			XSWContainerTableFree(table);
			// return error
			return(retCode);
		}
		// Update table disk_size and blksize from any of these blocks. 
		table->disk_size = block->disk_size;
		table->blksize   = block->blksize;
		// Add this block to the region table.
		XSWContainerTableAdd(table, block);
		// increase alloc size
		alloc += XSW_MAX_REGIONS;
		// Count how many available regions there are
		for (i=0; i<XSW_MAX_REGIONS; i++)
			if (block->region[i].state == REGION_STATE_EMPTY)
				free++;
		// Are there any more blocks to read?
		if (!(lba = block->next_lba))
			break;
	}

	// Add an extra allocation
	alloc += XSW_MAX_REGIONS;

	// Allocate regions
	table->region = XSWInternalMalloc(alloc * sizeof(XSW_BLOCK_REGION));

	// Zero out regions
	memset(table->region, 0, alloc * sizeof(XSW_BLOCK_REGION));

	// Setup total regions
	table->number_regions = 0;

	// Setup head pointer 
	walk = table->head;

	// Walk each table and copy in regions. 
	while(walk) {
		for (i=0; i<walk->table->number_regions; i++) {
			memcpy(&table->region[table->number_regions++], &walk->table->region[i],
				sizeof(XSW_BLOCK_REGION));
		}
		walk = walk->next;
	}

	// If there are less than 4 free regions left, allocate a new table. 
	if (free < 4) {
		// Allocate a free region.
		if ((retCode=XSWContainerAllocGet(table, (char*)"", XSW_REGION_TABLE_SIZE, 
			XSW_REGION_TABLE_SIZE, REGION_STATE_EXT, &region, 0))) {
			// Return failure.
			XSWContainerTableFree(table);
			// return error
			return(retCode);
		}

		// Allocate new block
		new_block = XSWInternalMalloc(sizeof(XSW_REGION_TABLE_BLOCK));

		// Setup new block
		XSWContainerInitBlock(new_block, region.offset, table->disk_size, table->blksize);

		// Add this block to the region table.
		XSWContainerTableAdd(table, new_block);

		// Setup the next LBA pointer
		block->next_lba = region.offset;
	}

	// Return table pointer
	*region_table = table;

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableSaveBlock                                 */
/* Scope       : Internal API                                               */
/* Description : Save the region table to the device.                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableSaveBlock(XSW_BIO *bio, XSW_REGION_TABLE_BLOCK *table)
{
ssize_t bytes;

	// Update CRC
	table->crc32 = XSWContainerCRC32(table);

	// Write the region table to block 0 on the device.
	bytes = XSWWrite(bio, table->current_lba,  sizeof(XSW_REGION_TABLE_BLOCK), table);

	// Check to see if write failed.
	if (bytes < 0) {
		XSWContainerDebug("XSWContainerTableSaveBlock() failed: pwrite failed %d\n", bytes);
		// Return failure.
		return(XSW_CONTAINER_ERR_WRITE_ERROR);
	}

	// Check to see if wrote expected number of bytes.
	if (bytes != sizeof(XSW_REGION_TABLE_BLOCK)) {
		XSWContainerDebug("XSWContainerTableSaveBlock() failed: unable to write %d bytes (wrote only %d)\n",
			sizeof(XSW_REGION_TABLE_BLOCK), bytes);
		// Return failure.
		return(XSW_CONTAINER_ERR_WRITE_ERROR);
	}

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableSave                                      */
/* Scope       : Internal API                                               */
/* Description : Save the region table to the device.                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerTableSave(XSW_BIO *bio, XSW_REGION_TABLE *table)
{
int i;
int retCode;
XSW_REGION_TABLE_BLOCKS *block;

	block = table->head;

	block->table->number_regions = 0;

	for (i=0; i<table->number_regions; i++) {
		memcpy(&block->table->region[block->table->number_regions++],
			&table->region[i], sizeof(XSW_BLOCK_REGION));

		if (block->table->number_regions >= XSW_MAX_REGIONS) {
			block=block->next;
			block->table->number_regions = 0;
		}
	}

	// Zero out leftover (if any)
	for (i=block->table->number_regions; i<XSW_MAX_REGIONS; i++)
		memset(&block->table->region[i], 0, sizeof(XSW_BLOCK_REGION));

	// Zero out remaining blocks
	while(block->next) {
		block = block->next;
		block->table->number_regions = 0;
		for (i=0; i<XSW_MAX_REGIONS; i++)
			memset(&block->table->region[i], 0, sizeof(XSW_BLOCK_REGION));
	}

	block = table->tail;

	// Write out all blocks to disk starting from the tail. 
	while(block) {
		if ((retCode=XSWContainerTableSaveBlock(bio, block->table))) {
			XSWContainerDebug("XSWContainerTableSave() failed\n");
			// Return failure.
			return(retCode);

		}
		block=block->prev;
	}

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerGetName                                        */
/* Scope       : Private API                                                */
/* Description : Get an existing named region from a region table.          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerGetName(XSW_REGION_TABLE *table, char *name, XSW_BLOCK_REGION *region)
{
int i;

	// Walk through all allocated regions.
	for (i=0; i<table->number_regions; i++) {
		// Only interested in allocated regions
		if (table->region[i].state == REGION_STATE_ALLOCATED) {
			// Check for a region name match.
			if (!strcmp(name, table->region[i].name)) {
				// Copy in region data if requested
				if (region)
					memcpy(region, &table->region[i], sizeof(XSW_BLOCK_REGION));
				// Return success
				return(XSW_CONTAINER_ERR_SUCCESS);
			}
		}
	}
	// Failed to find region/name
	return(XSW_CONTAINER_ERR_NAME_NOT_FOUND);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerFree                                           */
/* Scope       : Private API                                                */
/* Description : Free the region entry in the region table.                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWContainerFree(XSW_REGION_TABLE *table, int region)
{
	// Zero out the flags
	table->region[region].flags = 0;
	// Change region state to FREE
	table->region[region].state = REGION_STATE_FREE;
	// Normalize the block size.
	table->region[region].blksize = table->blksize;
	// Null out region name
	strcpy(table->region[region].name, "");
	// Optimize table and consolidate any back-to-back regions.
	XSWContainerTableOptimize(table);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyData                                       */
/* Scope       : Private API                                                */
/* Description : Copy data from region source to region dest.               */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerCopyData(XSW_BIO *fd, XSW_BLOCK_REGION *src, XSW_BLOCK_REGION *dst)
{
char *buffer;
void *memory;
u64 leftover = 0;
u64 src_pos = 0;
u64 dst_pos = 0;
ssize_t bytes;
int retCode = XSW_CONTAINER_ERR_SUCCESS;
static int progress=0;

	// Allocate buffer
	memory = XSWContainerAllocBuffer(XSW_REGION_BUFFER_SIZE, XSW_BLOCK_BUFFER_ALIGN);

	// Are we out of memory?
	if (!memory) {
		XSWContainerDebug("XSWContainerCopyData() failed: out of memory\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_MEMORY);
	}

	// Align buffer
	buffer = (char*)XSWContainerBufferAlign(memory, XSW_BLOCK_BUFFER_ALIGN);

	// If no soruce, zero out buffer
	if (!src) {
		// Fill buffer with zeros.
		memset(buffer, 0, XSW_REGION_BUFFER_SIZE);
	}

	// If there is a dest, setup leftover and position
	if (dst) {
		// Calculate size to copy
		leftover = dst->size;
		// Calculate starting write position
		dst_pos = dst->offset;
	}

	// If there is a source, setup leftover and position
	if (src) {
		// Calculate size to copy. We overwrite leftover with source's value
		// If src isn't set, then we use the previous leftover taken from dst.
		leftover = src->size;
		// Calculate starting read position
		src_pos = src->offset;
	}

	// Write loop while leftover remaining
	while(leftover) {

		// Calculate the smaller:  leftover or XSW_REGION_BUFFER_SIZE
		bytes = leftover > XSW_REGION_BUFFER_SIZE ? XSW_REGION_BUFFER_SIZE : leftover;

		if (src) {
			// Read the src
			if (XSWRead(fd, src_pos, bytes, buffer) != bytes) {
				XSWContainerDebug("XSWContainerCopyData() failed: reading\n");
				retCode = XSW_CONTAINER_ERR_READ_ERROR;
				break;
			}

			// Adjust next read position
			src_pos += bytes;
		}

		if (dst) {
			// Write the dstbuffer to the region.
			if (XSWWrite(fd, dst_pos, bytes, buffer) != bytes) {
				XSWContainerDebug("XSWContainerCopyData() failed: writing\n");
				retCode = XSW_CONTAINER_ERR_WRITE_ERROR;
				break;
			}

			// Adjust next write position
			dst_pos += bytes;
		}

		// Subtract bytes written from leftover
		leftover -= bytes;

		// Display progress via the callback (if installed)
		if (++progress >= 10) {
			progress = 0;
			if (_progress)
				_progress();
		}
	}

	// Free buffer
	XSWContainerFreeBuffer(memory);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyToFileInternal                             */
/* Scope       : Private API                                                */
/* Description : Copy data from region source to a file.                    */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerCopyToFileInternal(XSW_BIO *src_fd, XSW_BLOCK_REGION *src, int dst, u64 dst_pos)
{
void *memory;
char *buffer;
u64 leftover = 0;
u64 src_pos = 0;
ssize_t bytes;
int retCode = XSW_CONTAINER_ERR_SUCCESS;
static int progress=0;

	// Allocate buffer
	memory = XSWContainerAllocBuffer(XSW_REGION_BUFFER_SIZE, XSW_BLOCK_BUFFER_ALIGN);

	// Are we out of memory?
	if (!memory) {
		XSWContainerDebug("XSWContainerCopyToFileInternal() failed: out of memory\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_MEMORY);
	}

	// Align buffer
	buffer = (char*)XSWContainerBufferAlign(memory, XSW_BLOCK_BUFFER_ALIGN);

	// Calculate size to copy. We overwrite leftover with source's value
	// If src isn't set, then we use the previous leftover taken from dst.
	leftover = src->size;
	// Calculate starting read position
	src_pos = src->offset;

	// Write loop while leftover remaining
	while(leftover) {

		// Calculate the smaller:  leftover or XSW_REGION_BUFFER_SIZE
		bytes = leftover > XSW_REGION_BUFFER_SIZE ? XSW_REGION_BUFFER_SIZE : leftover;

		// Read the src
		if (XSWRead(src_fd, src_pos, bytes, buffer) != bytes) {
			XSWContainerDebug("XSWContainerCopyToFileInternal() failed: reading\n");
			retCode = XSW_CONTAINER_ERR_READ_ERROR;
			break;
		}

		// Adjust next read position
		src_pos += bytes;


		// Write the dstbuffer to the region.
		if (pwrite(dst, buffer, bytes, dst_pos) != bytes) {
			XSWContainerDebug("XSWContainerCopyToFileInternal() failed: writing\n");
			retCode = XSW_CONTAINER_ERR_WRITE_ERROR;
			break;
		}

		// Adjust next write position
		dst_pos += bytes;

		// Subtract bytes written from leftover
		leftover -= bytes;

		// Display progress via the callback (if installed)
		if (++progress >= 10) {
			progress = 0;
			if (_progress)
				_progress();
		}
	}

	// Free buffer
	XSWContainerFreeBuffer(memory);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyFromFileInternal                           */
/* Scope       : Private API                                                */
/* Description : Copy data from file to a region.                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerCopyFromFileInternal(XSW_BIO *dst_fd, XSW_BLOCK_REGION *dst, int src, u64 src_pos)
{
void *memory;
char *buffer;
u64 leftover = 0;
u64 dst_pos = 0;
ssize_t bytes;
int retCode = XSW_CONTAINER_ERR_SUCCESS;
static int progress=0;

	// Allocate buffer
	memory = XSWContainerAllocBuffer(XSW_REGION_BUFFER_SIZE, XSW_BLOCK_BUFFER_ALIGN);

	// Are we out of memory?
	if (!memory) {
		XSWContainerDebug("XSWContainerCopyFromFileInternal() failed: out of memory\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_MEMORY);
	}

	// Align buffer
	buffer = (char*)XSWContainerBufferAlign(memory, XSW_BLOCK_BUFFER_ALIGN);

	// Calculate size to copy. We overwrite leftover with source's value
	// If src isn't set, then we use the previous leftover taken from dst.
	leftover = dst->size;
	// Calculate starting read position
	dst_pos = dst->offset;

	// Write loop while leftover remaining
	while(leftover) {

		// Calculate the smaller:  leftover or XSW_REGION_BUFFER_SIZE
		bytes = leftover > XSW_REGION_BUFFER_SIZE ? XSW_REGION_BUFFER_SIZE : leftover;

		// Read the src
		if (pread(src, buffer, bytes, src_pos) != bytes) {
			XSWContainerDebug("XSWContainerCopyFromFileInternal() failed: reading\n");
			retCode = XSW_CONTAINER_ERR_READ_ERROR;
			break;
		}

		// Adjust next read position
		src_pos += bytes;

		// Write the dstbuffer to the region.
		if (XSWWrite(dst_fd, dst_pos, bytes, buffer) != bytes) {
			XSWContainerDebug("XSWContainerCopyFromFileInternal() failed: writing\n");
			retCode = XSW_CONTAINER_ERR_WRITE_ERROR;
			break;
		}

		// Adjust next write position
		dst_pos += bytes;

		// Subtract bytes written from leftover
		leftover -= bytes;

		// Display progress via the callback (if installed)
		if (++progress >= 10) {
			progress = 0;
			if (_progress)
				_progress();
		}
	}

	// Free buffer
	XSWContainerFreeBuffer(memory);

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerGetFlagsRegion                                 */
/* Scope       : Public API                                                 */
/* Description : Get region flags.                                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerGetFlagsRegion(XSW_BIO *bio, char *name, u32 *flags)
{
XSW_BLOCK_REGION region;
int retCode;

	// Zero flags
	*flags = 0;

	// Load region
	if ((retCode=XSWContainerGetRegion(bio, name, &region)))
		return(retCode);

	// Store flags from region
	*flags = region.flags;

	// Return success.
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerSetFlagsRegion                                 */
/* Scope       : Public API                                                 */
/* Description : Set region flags.                                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerSetFlagsRegion(XSW_BIO *bio, char *name, u32 flags)
{
XSW_REGION_TABLE *table = 0;
int retCode;
int i;

	// Did we fail to load the region table?
	if ((retCode=XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerSetFlags() failed: XSWContainerTableLoad() failed\n");
		// Return failure.
		return(retCode);
	}

	// Walk through regions and find region name.
	for (i=0; i<table->number_regions; i++) {
		// Only look in allocated named regions.
		if (table->region[i].state == REGION_STATE_ALLOCATED) {
			// Is this the region we're looking for?
			if (!strcmp(name, table->region[i].name)) {
				// Set new flags.
				table->region[i].flags = flags;
				// Save table back to device and return.
				retCode = XSWContainerTableSave(bio, table);
				// Free the region table
				XSWContainerTableFree(table);
				// return error code
				return(retCode);
			}
		}
	}

	// Free the region table
	XSWContainerTableFree(table);

	// Region name not found, return error
	return(XSW_CONTAINER_ERR_NAME_NOT_FOUND);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerMove                                           */
/* Scope       : Private API                                                */
/* Description : Move allocated regionA into free regionB                   */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWContainerMove(XSW_BIO *bio, XSW_REGION_TABLE *table, int alloced, int free)
{
int retCode = 0;

	// Alloc the region from this free region with an offset of 0
	XSWContainerAlloc(table, free, table->region[alloced].name, 
		table->region[alloced].size, table->region[alloced].blksize, REGION_STATE_ALLOCATED, 0);

	// Copy data from source to dest
	if ((retCode=XSWContainerCopyData(bio, &table->region[alloced], &table->region[free]))) {
		XSWContainerDebug("XSWContainerSwap() XSWContainerCopyData() failed %d\n", retCode);
		return(retCode);
	}

	// Free the alloced region
	XSWContainerFree(table, alloced);

	// Optimize the table-> The free region we just added
	// might be mergeable with other free regions.
	XSWContainerTableOptimize(table);

	// Return error code
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWRegionQSortOffset                                       */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWRegionQSortOffset(const void *p1, const void *p2)
{
XSW_BLOCK_REGION *r1 = (XSW_BLOCK_REGION *)p1;
XSW_BLOCK_REGION *r2 = (XSW_BLOCK_REGION *)p2;

	if (r1->offset > r2->offset) return(1);
	if (r1->offset < r2->offset) return(-1);
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerTableDefragment                                */
/* Scope       : Private API                                                */
/* Description : Defragment a device.                                       */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerTableDefragment(XSW_BIO *bio)
{
XSW_REGION_TABLE *table = 0;
int defrag,i,j;
int retCode;
int modified=0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerTableLoad() failed\n");
		// Return failure.
		return(retCode);
	}

	defrag = 1;

	while(defrag) {
		// Sort the regions based on offset
		qsort(&table->region[0], table->number_regions, sizeof(XSW_BLOCK_REGION), XSWRegionQSortOffset);

		for (defrag=0,i=0; i<table->number_regions-1; i++) {
			// Find the first free slot from the bottom.
			if (table->region[i].state == REGION_STATE_FREE) {
				// Look for allocated slots of the same size that are higher in offset values.
				for (j=i+1; j<table->number_regions; j++) {
					// Calculate closest free region upwards and generate a score for it.
					if (table->region[j].state == REGION_STATE_ALLOCATED &&
						!(table->region[j].flags & REGION_FLAGS_UNMOVABLE)) {
						if ((table->region[i].size == table->region[j].size) &&
							(table->region[i].blksize == table->region[j].blksize)) {
							if ((retCode=XSWContainerMove(bio, table, j, i))) {
								XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerSwap()\n");
						   		// Free the region table
								XSWContainerTableFree(table);
								// return error code
								return(retCode);
							}
							modified=defrag=1;
							break;
						}
					}
				}
			}
		}
	}

	if (modified) {
		if ((retCode = XSWContainerTableSave(bio, table))) {
			XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerTableSave()\n");
	   		// Free the region table
			XSWContainerTableFree(table);
			// return error code
			return(retCode);
		}
		modified = 0;
	}

	// Free the region table
	XSWContainerTableFree(table);

	defrag = 1;

	while(defrag) {

		// Did we fail to load the region table?
		if ((retCode = XSWContainerTableLoad(bio, &table))) {
			XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerTableLoad() failed\n");
			// Return failure.
			return(retCode);
		}

		// Sort the regions based on offset
		qsort(&table->region[0], table->number_regions, sizeof(XSW_BLOCK_REGION), XSWRegionQSortOffset);

		for (defrag=0,i=0; i<table->number_regions-1; i++) {
			if (table->region[i].state == REGION_STATE_FREE) {
				for (j=i+1; j<table->number_regions; j++) {
					// Calculate closest free region upwards and generate a score for it.
					if (table->region[j].state == REGION_STATE_ALLOCATED) {
						// Make sure free region is large enough to Swap with 
						// the allocated region below it.
						if (XSWContainerAlignSize(&table->region[i], 
							table->region[j].blksize) >= table->region[j].size) {
							// Make sure this allocated region is movable.
							if (!(table->region[j].flags & REGION_FLAGS_UNMOVABLE)) {
	//							XSWContainerDebug("XSWContainerTableDefragment() Swapping Free %d with %d (%s)\n",
	//								i, i+1, table->region[i+1].name);

								if ((retCode=XSWContainerMove(bio, table, j, i))) {
									XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerSwap()\n");
							   		// Free the region table
									XSWContainerTableFree(table);
									// return error code
									return(retCode);
								}
								modified=defrag=1;
								break;
							}
						}
					}
				}
				if (defrag)
					break;
			}
		}

		if (modified) {
			if ((retCode = XSWContainerTableSave(bio, table))) {
				XSWContainerDebug("XSWContainerTableDefragment() failed: XSWContainerTableSave()\n");
				// Free the region table
				XSWContainerTableFree(table);
				// return error code
				return(retCode);
			}
			modified=0;
		}

		// Free the region table
		XSWContainerTableFree(table);
	}

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerInit                                           */
/* Scope       : Public API                                                 */
/* Description : Create an empty region table for a device.                 */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerInit(XSW_BIO *bio, u32 blksize)
{
XSW_REGION_TABLE_BLOCK table;

	if (sizeof(XSW_REGION_TABLE_BLOCK) > XSW_REGION_TABLE_SIZE) {
		XSWContainerDebug("XSWContainerInit() failed: internal error\n");
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	XSWContainerInitBlock(&table, 0, bio->size, blksize);

	table.region[0].offset  = XSW_REGION_TABLE_SIZE;
	table.region[0].size    = bio->size-XSW_REGION_TABLE_SIZE;
	table.region[0].blksize = blksize;
	table.region[0].flags   = 0;
	table.region[0].state   = REGION_STATE_FREE;
	strcpy(table.region[0].name, "");

	table.number_regions = 1;

	// Save region table to device.
	return(XSWContainerTableSaveBlock(bio, &table));
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerAlignSize                                      */
/* Scope       : Public API                                                 */
/* Description : Calculate max available bytes in region when block aligned */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
u64 XSWContainerAlignSize(XSW_BLOCK_REGION *region, u32 blksize)
{
u64 offset, align, size;

	// Calculate aligned offset.
	offset = XSWContainerRoundSize(region->offset, blksize);

	align = offset - region->offset;

	size = XSWContainerRoundSize(region->size - align, blksize); 

	while((align + size) > region->size)
		size -= blksize;

	return(size);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCreateRegion                                   */
/* Scope       : Public API                                                 */
/* Description : Add a new region.                                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCreateRegion(XSW_BIO *bio, char *name, u64 size, u32 blksize, u32 flags)
{
XSW_REGION_TABLE *table = 0;
int retCode;

	// Make sure a name is used.
	if (!name) {
		XSWContainerDebug("XSWContainerCreate() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Make sure a name is used.
	if (!strlen(name)) {
		XSWContainerDebug("XSWContainerCreate() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerCreate() failed: XSWContainerTableLoad() failed\n");
		// Return failure.
		return(retCode);
	}

	// Make sure named region does not already exist. 
	if (!XSWContainerGetName(table, name, 0)) {
		XSWContainerDebug("XSWContainerCreate() failed: region '%s' already exists!\n", name);
   		// Free the region table
		XSWContainerTableFree(table);
		// Return failure.
		return(XSW_CONTAINER_ERR_ALREADY_EXISTS);
	}

	// Round block sise to default blocksize granularity.
	blksize = XSWContainerRoundSize(blksize, table->blksize);

	// Round size to blksize granularity.
	size = XSWContainerRoundSize(size,blksize);

	// Allocate the region
	if ((retCode=XSWContainerAllocGet(table, name, size, blksize, REGION_STATE_ALLOCATED, 0, flags))) {
		XSWContainerDebug("XSWContainerCreate() failed to allocate region\n");
   		// Free the region table
		XSWContainerTableFree(table);
		// Return failure.
		return(retCode);
	}

	// Save table back to device and return.
	retCode = XSWContainerTableSave(bio, table);

   	// Free the region table
	XSWContainerTableFree(table);

	// Return to status code
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyRegionToFile                               */
/* Scope       : Public API                                                 */
/* Description : Copy a region to a standard file.                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCopyRegionToFile(XSW_BIO *bio, char *region, char *file)
{
int retCode;
XSW_REGION_TABLE *table = 0;
XSW_BLOCK_REGION src_region;
int dst_fd;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerCopyToFile() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Look up src region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, region, &src_region))) {
		XSWContainerDebug("XSWContainerCopyToFile() failed: region '%s' does not exist\n", region);
   		// Free the region table
		XSWContainerTableFree(table);
		// return error code
		return(retCode);
	}

	// Free the region table
	XSWContainerTableFree(table);

	// Open the file
	if ((dst_fd = open(file, O_RDWR | O_CREAT, 0777)) <0) {
		XSWContainerDebug("XSWContainerCopyToFile() failed: open failed %d\n", dst_fd);
		// Return failure.
		return(XSW_CONTAINER_ERR_READ_ERROR);
	}

	// Copy the region data to a file
	retCode = XSWContainerCopyToFileInternal(bio, &src_region, dst_fd, 0);

	// Close the device
	close(dst_fd);

	// Return the status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyAllToFile                                  */
/* Scope       : Public API                                                 */
/* Description : Copy all regions to a standard backup file.                */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCopyAllToFile(XSW_BIO *bio, char *file)
{
int retCode;
int i;
XSW_REGION_TABLE *table = 0;
int dst_fd;
u64 dst_pos = 0;

	// Load the region table for this device
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerCopyAllToFile() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Create output file
	if ((dst_fd = open(file, O_RDWR | O_CREAT, 0777)) <0) {
		XSWContainerDebug("XSWContainerCopyAllToFile() failed to create '%s'\n", file);
		// Free the region table
		XSWContainerTableFree(table);
		// Return write error
		return(XSW_CONTAINER_ERR_WRITE_ERROR);
	}

	// Write initial region table.
	dst_pos += pwrite(dst_fd, table, sizeof(XSW_REGION_TABLE), 0);

	// Write region header and region data.
	for (i=0; i<table->number_regions; i++) {
		// Write out region info
		dst_pos += pwrite(dst_fd, &table->region[i], sizeof(XSW_BLOCK_REGION), dst_pos);

		// Only write out allocated regions. 
		if (table->region[i].state == REGION_STATE_ALLOCATED) {
			// Write out region data to file
			XSWContainerCopyToFileInternal(bio, &table->region[i], dst_fd, dst_pos);

			// Adjust write pointer
			dst_pos += table->region[i].size;
		}
	}

	// Free the region table
	XSWContainerTableFree(table);

	// Close output file
	close(dst_fd);

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyAllFromFile                                */
/* Scope       : Public API                                                 */
/* Description : Copy all regions from a standard backup file.              */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCopyAllFromFile(XSW_BIO *bio, char *file)
{
int retCode = XSW_CONTAINER_ERR_SUCCESS;
int i;
XSW_REGION_TABLE table;
XSW_BLOCK_REGION region;
XSW_BLOCK_REGION dst_region;
int src_fd;
u64 src_pos = 0;

	if ((src_fd = open(file, O_RDWR)) <0) {
		XSWContainerDebug("XSWContainerCopyAllToFile() failed to open '%s'\n", file);
		// Return failure.
		return(XSW_CONTAINER_ERR_READ_ERROR);
	}

	// Read initial region table.
	src_pos += pread(src_fd, &table, sizeof(XSW_REGION_TABLE), 0); 

	// Read region header and region data.
	for (i=0; i<table.number_regions; i++) {
		// Read region info
		src_pos += pread(src_fd, &region, sizeof(XSW_BLOCK_REGION), src_pos);

		// Only read in allocated regions. 
		if (region.state != REGION_STATE_ALLOCATED)
			continue;

		// Get existing region (if any) otherwise create one. 
		if (XSWContainerGetRegion(bio, region.name, &dst_region) != XSW_CONTAINER_ERR_SUCCESS)
			XSWContainerCreateRegion(bio, region.name, region.size, region.blksize, region.flags);

		if (XSWContainerGetRegion(bio, region.name, &dst_region) != XSW_CONTAINER_ERR_SUCCESS) {
			XSWContainerDebug("XSWContainerCopyAllToFile() failed to find/create region '%s'\n", region.name);
			// Create/locate error
			retCode = XSW_CONTAINER_ERR_NAME_NOT_FOUND;
			break;
		}

		if (region.size > dst_region.size) {
			XSWContainerDebug("XSWContainerCopyAllToFile() region size mismatch for '%s'\n", region.name);
			// Size error
			retCode = XSW_CONTAINER_ERR_SIZE_MISMATCH;
			break;
		}

		// Use region size in file for the copy size.
		dst_region.size = region.size;

		printf("Copying data to region '%s'", region.name);
		fflush(stdout);

		// Read data from file into region.
		XSWContainerCopyFromFileInternal(bio, &dst_region, src_fd, src_pos);

		printf("\n");

		// Update region flags
		XSWContainerSetFlagsRegion(bio, region.name, region.flags);

		// Adjust read pointer
		src_pos += region.size;
	}

	// Close the file handle.
	close(src_fd);

	// Return success
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyRegionFromFile                             */
/* Scope       : Public API                                                 */
/* Description : Copy a standard file to a region.                          */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCopyRegionFromFile(XSW_BIO *bio, char *region, char *file)
{
int retCode;
XSW_REGION_TABLE *table = 0;
XSW_BLOCK_REGION dst_region;
int src_fd;
u32 flags = 0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerCopyFromFile() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Look up src region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, region, &dst_region))) {
		XSWContainerDebug("XSWContainerCopyFromFile() failed: region '%s' does not exist\n", region);
   		// Free the region table
		XSWContainerTableFree(table);
		// return error code
		return(retCode);
	}

	// Free the region table
	XSWContainerTableFree(table);

	// Open the file
	if ((src_fd = open(file, O_RDWR)) <0) {
		XSWContainerDebug("XSWContainerCopyFromFile() failed: open failed %d\n", src_fd);
		// Return failure.
		return(XSW_CONTAINER_ERR_READ_ERROR);
	}

	// Copy the file to the region
	if ((retCode = XSWContainerCopyFromFileInternal(bio, &dst_region, src_fd, 0)) == XSW_CONTAINER_ERR_SUCCESS) {
		// If copy was successful, set the dirty flag in the region. 
		XSWContainerGetFlagsRegion(bio, region, &flags);
		flags |= REGION_FLAGS_DIRTY;
		XSWContainerSetFlagsRegion(bio, region, flags);
	}

	// Close the device
	close(src_fd);

	// Return the status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerCopyRegion                                     */
/* Scope       : Public API                                                 */
/* Description : Copy a region.                                             */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerCopyRegion(XSW_BIO *bio, char *src, char *dst)
{
int retCode;
XSW_REGION_TABLE *table = 0;
XSW_BLOCK_REGION src_region;
XSW_BLOCK_REGION dst_region;
u32 flags = 0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerCopy() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Look up src region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, src, &src_region))) {
		XSWContainerDebug("XSWContainerCopy() failed: region '%s' does not exist\n", src);
   		// Free the region table
		XSWContainerTableFree(table);
		// return error code
		return(retCode);
	}

	// Look up dst region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, dst, &dst_region))) {
		XSWContainerDebug("XSWContainerCopy() failed: region '%s' does not exist\n", dst);
   		// Free the region table
		XSWContainerTableFree(table);
		// return error code
		return(retCode);
	}

	// Free the region table
	XSWContainerTableFree(table);

	if (src_region.size != dst_region.size) {
		XSWContainerDebug("XSWContainerCopy() failed: region size mismatch\n");
		return(XSW_CONTAINER_ERR_SIZE_MISMATCH);
	}

	// Copy the region data
	if ((retCode = XSWContainerCopyData(bio, &src_region, &dst_region)) == XSW_CONTAINER_ERR_SUCCESS) {
		// If copy was successful, set the dirty flag in the dest. 
		XSWContainerGetFlagsRegion(bio, dst, &flags);
		flags |= REGION_FLAGS_DIRTY;
		XSWContainerSetFlagsRegion(bio, dst, flags);
	}

	// Return the status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerZeroRegion                                     */
/* Scope       : Public API                                                 */
/* Description : Zero out a region.                                         */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerZeroRegion(XSW_BIO *bio, char *dst)
{
int retCode;
XSW_REGION_TABLE *table = 0;
XSW_BLOCK_REGION region;
u32 flags = 0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerZero() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Get region
	retCode = XSWContainerGetName(table, dst, &region);

   	// Free the region table
	XSWContainerTableFree(table);

	// Look up region name and return region data if it exists. 
	if (retCode) {
		XSWContainerDebug("XSWContainerZero() failed: region '%s' does not exist\n", dst);
		return(retCode);
	}

	// Fill destination with zeros
	if ((retCode = XSWContainerCopyData(bio, 0, &region)) == XSW_CONTAINER_ERR_SUCCESS) {
		// If Zero was successful, clear the dirty flag.
		XSWContainerGetFlagsRegion(bio, dst, &flags);
		flags &= ~REGION_FLAGS_DIRTY;
		XSWContainerSetFlagsRegion(bio, dst, flags);
	}

	// Flush out data to device
	XSWSync(bio, region.offset, region.size, XSW_FSYNC_SYNC);
	// Discard data
	XSWDiscard(bio, region.offset, region.size);

	// Return the status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerRenameRegion                                   */
/* Scope       : Public API                                                 */
/* Description : Rename an existing named region.                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerRenameRegion(XSW_BIO *bio, char *name, char *rename)
{
XSW_REGION_TABLE *table = 0;
int i;
int retCode;

	// Make sure a name is used.
	if (!name) {
		XSWContainerDebug("XSWContainerRename() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Make sure a name is used.
	if (!strlen(name)) {
		XSWContainerDebug("XSWContainerRename() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerRename() failed: XSWContainerTableLoad() failed\n");
		// Return failure.
		return(retCode);
	}

	for (i=0; i<table->number_regions; i++) {
		if (table->region[i].state == REGION_STATE_ALLOCATED) {
			if (!strcmp(name, table->region[i].name)) {
				// Update the name
				strcpy(table->region[i].name, rename);

				// Save table back to device and return.
				retCode = XSWContainerTableSave(bio, table);

				// Free the region table
				XSWContainerTableFree(table);

				// return status code
				return(retCode);
			}
		}
	}

	// Free the region table
	XSWContainerTableFree(table);

	XSWContainerDebug("XSWContainerRename() failed: region '%s' does not exist!\n", name);
	// Failed to delete region.
	return(XSW_CONTAINER_ERR_NAME_NOT_FOUND);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerDelete                                         */
/* Scope       : Public API                                                 */
/* Description : Delete an existing named region.                           */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerDeleteRegion(XSW_BIO *bio, char *name)
{
XSW_REGION_TABLE *table = 0;
int i;
int retCode;

	// Make sure a name is used.
	if (!name) {
		XSWContainerDebug("XSWContainerDelete() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Make sure a name is used.
	if (!strlen(name)) {
		XSWContainerDebug("XSWContainerDelete() failed: name must be defined\n");
		// Return failure.
		return(XSW_CONTAINER_ERR_BAD_ARGUMENT);
	}

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerDelete() failed: XSWContainerTableLoad() failed\n");
		// Return failure.
		return(retCode);
	}

	for (i=0; i<table->number_regions; i++) {
		if (table->region[i].state == REGION_STATE_ALLOCATED) {
			if (!strcmp(name, table->region[i].name)) {
				// Free the region
				XSWContainerFree(table, i);
				// Save table back to device and return.
				retCode = XSWContainerTableSave(bio, table);

				// Free the region table
				XSWContainerTableFree(table);

				// return status code
				return(retCode);
			}
		}
	}

	// Free the region table
	XSWContainerTableFree(table);

	XSWContainerDebug("XSWContainerDelete() failed: region '%s' does not exist!\n", name);
	// Failed to delete region.
	return(XSW_CONTAINER_ERR_NAME_NOT_FOUND);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerRegionExists                                   */
/* Scope       : Public API                                                 */
/* Description : Check if a region exists.                                  */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerRegionExists(XSW_BIO *bio, char *name)
{
int retCode;
XSW_REGION_TABLE *table = 0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerTableGet() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Look up region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, name, 0)))
		XSWContainerDebug("XSWContainerGet() failed: region '%s' does not exist\n", name);

	// Free the region table
	XSWContainerTableFree(table);

	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerGetRegion                                      */
/* Scope       : Public API                                                 */
/* Description : Get an existing named region from a device.                */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerGetRegion(XSW_BIO *bio, char *name, XSW_BLOCK_REGION *region)
{
int retCode;
XSW_REGION_TABLE *table = 0;

	// Did we fail to load the region table?
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerTableGet() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	// Look up region name and return region data if it exists. 
	if ((retCode = XSWContainerGetName(table, name, region)))
		XSWContainerDebug("XSWContainerGet() failed: region '%s' does not exist\n", name);

	// Free the region table
	XSWContainerTableFree(table);

	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerQueryAll                                       */
/* Scope       : Public API                                                 */
/* Description : Query all regions.                                         */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWContainerQueryAll(XSW_BIO *bio, int *count, XSW_BLOCK_REGION **regions)
{
int retCode;
int i;
XSW_REGION_TABLE *table = 0;
XSW_BLOCK_REGION *user;

	// Load the region table for this device
	if ((retCode = XSWContainerTableLoad(bio, &table))) {
		XSWContainerDebug("XSWContainerQueryAll() failed: XSWContainerTableLoad() failed\n");
		return(retCode);
	}

	if (count && regions) {
		// Alloc region array
		user = XSWInternalMalloc(table->number_regions * sizeof(XSW_BLOCK_REGION));
		// Copy the regions
		for (i=0; i<table->number_regions; i++) 
			memcpy(&user[i], &table->region[i], sizeof(XSW_BLOCK_REGION));
		// Update count with actual. 
		*count = table->number_regions;
		// Copy region pointer
		*regions = user;
		// Sort the regions based on offset
		qsort(&user[0], table->number_regions, sizeof(XSW_BLOCK_REGION), XSWRegionQSortOffset);
	}

	// Free the region table
	XSWContainerTableFree(table);

	// Return success
	return(XSW_CONTAINER_ERR_SUCCESS);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWContainerErrString                                      */
/* Scope       : Public API                                                 */
/* Description : Convert an error to a string.                              */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
char *XSWContainerErrString(int error)
{
	switch(error) {
		case XSW_CONTAINER_ERR_SUCCESS:
			return((char *)"XSW_CONTAINER_ERR_SUCCESS");

		case XSW_CONTAINER_ERR_BAD_DEVICE:
			return((char *)"XSW_CONTAINER_ERR_BAD_DEVICE");

		case XSW_CONTAINER_ERR_NO_REGION_TABLE:
			return((char *)"XSW_CONTAINER_ERR_NO_REGION_TABLE");

		case XSW_CONTAINER_ERR_ALREADY_EXISTS:
			return((char *)"XSW_CONTAINER_ERR_ALREADY_EXISTS");

		case XSW_CONTAINER_ERR_NAME_NOT_FOUND:
			return((char *)"XSW_CONTAINER_ERR_NAME_NOT_FOUND");

		case XSW_CONTAINER_ERR_OUT_OF_SPACE:
			return((char *)"XSW_CONTAINER_ERR_OUT_OF_SPACE");

		case XSW_CONTAINER_ERR_MAX_REGIONS:
			return((char *)"XSW_CONTAINER_ERR_MAX_REGIONS");

		case XSW_CONTAINER_ERR_WRITE_ERROR:
			return((char *)"XSW_CONTAINER_ERR_WRITE_ERROR");

		case XSW_CONTAINER_ERR_BAD_ARGUMENT:
			return((char *)"XSW_CONTAINER_ERR_BAD_ARGUMENT");

		default:
			return((char *)"Unknown");
	}
}


