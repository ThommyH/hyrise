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
 * Filename: xsw_api_region.h
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

#ifndef __XSW_API_REGION_H
#define __XSW_API_REGION_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REGION_NAME 255

#define REGION_STATE_EMPTY     0
#define REGION_STATE_FREE      1
#define REGION_STATE_ALLOCATED 2
#define REGION_STATE_RAW       3
#define REGION_STATE_EXT       4

#define REGION_FLAGS_UNMOVABLE 0x01
#define REGION_FLAGS_DIRTY     0x02
#define REGION_FLAGS_CKPT      0x04

typedef struct XSW_BLOCK_REGION_T { __attribute__((packed))
	u64  offset;                    //8
	u64  size;                      //8
	u32  blksize;                   //4
	u32  flags;                     //4
	u8   state;                     //1
	char name[MAX_REGION_NAME+1];   //256 = 281 bytes
} XSW_BLOCK_REGION;

XSW_BIO *XSWContainerOpenRegion         (XSW_BIO *bio, char *name);
XSW_BIO *XSWContainerResolveRegion      (XSW_BIO *bio, char *name);

int      XSWContainerInit               (XSW_BIO *bio, u32 blksize);
int      XSWContainerGetRegion          (XSW_BIO *bio, char *name, XSW_BLOCK_REGION *region);
int      XSWContainerRegionExists       (XSW_BIO *bio, char *name);
int      XSWContainerCreateRegion       (XSW_BIO *bio, char *name, u64 size, u32 blksize, u32 flags);
int      XSWContainerDeleteRegion       (XSW_BIO *bio, char *name);
int      XSWContainerRenameRegion       (XSW_BIO *bio, char *name, char *rename);
int      XSWContainerZeroRegion         (XSW_BIO *bio, char *name);
int      XSWContainerCopyRegion         (XSW_BIO *bio, char *src, char *dst);
int      XSWContainerQueryAll           (XSW_BIO *bio, int *count, XSW_BLOCK_REGION **regions);
int      XSWContainerTableDefragment    (XSW_BIO *bio); 
int      XSWContainerSetFlagsRegion     (XSW_BIO *bio, char *name, u32 flags);
int      XSWContainerGetFlagsRegion     (XSW_BIO *bio, char *name, u32 *flags);
int      XSWContainerCopyRegionToFile   (XSW_BIO *bio, char *region, char *file);
int      XSWContainerCopyRegionFromFile (XSW_BIO *bio, char *region, char *file);
int      XSWContainerCopyAllToFile      (XSW_BIO *bio, char *file);
int      XSWContainerCopyAllFromFile    (XSW_BIO *bio, char *file);
u64      XSWContainerAlignSize          (XSW_BLOCK_REGION *region, u32 blksize);
int      XSWContainerSetProgressCB      (void (*progress)(void));
char    *XSWContainerErrString          (int error);
u64      XSWContainerRoundSize          (u64 size, u32 blksize);

#define XSW_CONTAINER_ERR_SUCCESS         0
#define XSW_CONTAINER_ERR_BAD_DEVICE      1
#define XSW_CONTAINER_ERR_NO_REGION_TABLE 2
#define XSW_CONTAINER_ERR_ALREADY_EXISTS  3
#define XSW_CONTAINER_ERR_NAME_NOT_FOUND  4
#define XSW_CONTAINER_ERR_OUT_OF_SPACE    5
#define XSW_CONTAINER_ERR_MAX_REGIONS     6
#define XSW_CONTAINER_ERR_WRITE_ERROR     7
#define XSW_CONTAINER_ERR_BAD_ARGUMENT    8
#define XSW_CONTAINER_ERR_READ_ERROR      9
#define XSW_CONTAINER_ERR_MEMORY          10
#define XSW_CONTAINER_ERR_SIZE_MISMATCH   11

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_REGION_H


