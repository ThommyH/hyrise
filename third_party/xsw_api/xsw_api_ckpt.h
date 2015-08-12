/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Checkpoint API
 *
 * Filename: xsw_api_ckpt.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Checkpoint API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 05/05/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_CKPT_H
#define __XSW_API_CKPT_H 

#ifdef __cplusplus
extern "C" {
#endif

#define XSWCKPT_STATUS_CREATED         0x01
#define XSWCKPT_STATUS_OPENED          0x02
#define XSWCKPT_STATUS_CHECKPOINTED    0x04
#define XSWCKPT_STATUS_ROLLED_BACKWARD 0x08
#define XSWCKPT_STATUS_ROLLED_FORWARD  0x10
#define XSWCKPT_STATUS_DIRTY           0x20
#define XSWCKPT_STATUS_CLOSED          0x40
#define XSWCKPT_STATUS_DISCARDED       0x80

#define XSW_CKPT_MAX_USER_DATA 16

typedef struct XSW_CKPT_INFO_T {
	u64  size;
	u32  blksize;
	int  options;
	u8   status;
	int  refcnt;
	int  users;
	u64  user_data[XSW_CKPT_MAX_USER_DATA];
	u64  num_checkpoints;
	char node[XSW_MAX_PATH];
} XSW_CKPT_INFO;

#define XSWCKPT_OPTION_ENABLE_CREATE    0x01
#define XSWCKPT_OPTION_FORCE_CREATE     0x02
#define XSWCKPT_OPTION_AUTO_CHECKPOINT  0x04
#define XSWCKPT_OPTION_DEVICE_MODE      0x08
#define XSWCKPT_OPTION_EXCLUSIVE        0x10

XSW_BIO *XSWCkptCreate        (char *device_path, u64 size);
XSW_BIO *XSWCkptOpen          (XSW_BIO *bio, int options);
int      XSWCkptCalculateSize (u64 user_size, u32 bio_blksize, u64 *bio_size);
int      XSWCkptCheckpoint    (XSW_BIO *bio);
int      XSWCkptDiscard       (XSW_BIO *bio);
int      XSWCkptRollBackward  (XSW_BIO *bio);
int      XSWCkptRollForward   (XSW_BIO *bio);
int      XSWCkptQuery         (XSW_BIO *bio, XSW_CKPT_INFO *info);
int      XSWCkptSetUserData   (XSW_BIO *bio, int index, u64 data);
int      XSWCkptGetUserData   (XSW_BIO *bio, int index, u64 *data);
int      XSWCkptUnloadModule  (void);

int      XSWCkptDeviceOpen        (XSW_BIO *bio, int options);
int      XSWCkptDeviceCheckpoint  (int device_id);
int      XSWCkptDeviceRollBackward(int device_id);
int      XSWCkptDeviceDiscard     (int device_id);
int      XSWCkptDeviceRollForward (int device_id);
int      XSWCkptDeviceQuery       (int device_id, XSW_CKPT_INFO *info);
int      XSWCkptDeviceClose       (int device_id);
int      XSWCkptDeviceSetUserData (int device_id, int index, u64 data);
int      XSWCkptDeviceGetUserData (int device_id, int index, u64 *data);
int      XSWCkptDevicesQueryNum   (void);

#define XSW_CKPT_BITMAP_BLOCK_SIZE 4096UL
#define XSW_CKPT_DEFAULT_BLKSIZE   4096UL

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_CKPT_H 

