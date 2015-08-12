/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW block I/O API
 *
 * Filename: xsw_api_bio.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_BIO_H
#define __XSW_API_BIO_H 

#ifdef __cplusplus
extern "C" {
#endif

#define XSW_MAX_PATH 256

#define XSW_FSYNC_ASYNC 0
#define XSW_FSYNC_SYNC  1

#define XSW_BIO_TYPE_MMAP    1  // MMAP
#define XSW_BIO_TYPE_CKPT    2  // Checkpoint device
#define XSW_BIO_TYPE_FILE    3  // Standard file
#define XSW_BIO_TYPE_RAW     4  // RAW device
#define XSW_BIO_TYPE_GROUP   5  // A Group BIO
#define XSW_BIO_TYPE_DRAM    6  // A DRAM device
#define XSW_BIO_TYPE_RDMA    7  // A RDMA device
#define XSW_BIO_TYPE_NETWORK 8  // A Network device

#define XSW_BIO_GROUP(bio) ((bio)->type == XSW_BIO_TYPE_GROUP)

typedef struct XSW_BIO_T {
	char     pathname[XSW_MAX_PATH];
	u32      type;
	struct   XSW_BIO_T *parent;
	struct   XSW_BIO_T *child;
	int      (*info)     (struct XSW_BIO_T *);
	int      (*close)    (struct XSW_BIO_T *);
	ssize_t  (*read)     (struct XSW_BIO_T *, u64, u64, void *);
	ssize_t  (*write)    (struct XSW_BIO_T *, u64, u64, void *);
	int      (*fsync)    (struct XSW_BIO_T *, u64, u64, int sync);
	int      (*trim)     (struct XSW_BIO_T *, u64, u64);
	int      (*discard)  (struct XSW_BIO_T *, u64, u64);
	int      fd;
	u64      offset;
	u64      size;
	u32      blksize;
	void    *user;
	void    *mmap_vaddr;
	u64      client_data;
	struct   XSW_BIO_T *sibling_next;
	struct   XSW_BIO_T *sibling_prev;
	struct   XSW_BIO_T *group_link;
} XSW_BIO;

#define XSW_STAT_FLAGS_EXISTS 0x01
#define XSW_STAT_FLAGS_FILE   0x02
#define XSW_STAT_FLAGS_REGION 0x04
#define XSW_STAT_FLAGS_DEVICE 0x08

typedef struct XSW_STAT_T {
	int          flags;
	char         device  [XSW_MAX_PATH];
	char         pathname[XSW_MAX_PATH];
} XSW_STAT;

XSW_BIO      *XSWOpen        (char *pathname);
XSW_BIO      *XSWOpenEx      (int fd, u64 offset, u64 size);
XSW_BIO      *XSWOpenStat    (XSW_STAT *xswstat);
XSW_BIO      *XSWCreate      (char *pathname, u64 size, u32 flags);
int           XSWClose       (XSW_BIO *bio);
ssize_t       XSWRead        (XSW_BIO *bio, u64 offset, u64 size, void *buf);
ssize_t       XSWWrite       (XSW_BIO *bio, u64 offset, u64 size, void *buf);
int           XSWSync        (XSW_BIO *bio, u64 offset, u64 size, int sync);
int           XSWTrim        (XSW_BIO *bio, u64 offset, u64 size);
int           XSWDiscard     (XSW_BIO *bio, u64 offset, u64 size);
int           XSWZero        (XSW_BIO *bio, u64 offset, u64 size);
void          XSWInfo        (XSW_BIO *bio);
void          XSWDump        (XSW_BIO *bio);
void          XSWSetPrivate  (XSW_BIO *bio, void *data);
void *        XSWGetPrivate  (XSW_BIO *bio);
XSW_BIO      *XSWAllocBIO    (XSW_BIO *parent);
int           XSWFreeBIO     (XSW_BIO *bio);
int           XSWStat        (char *pathname, XSW_STAT *stat);
int           XSWOrphan      (XSW_BIO *bio);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_BIO

