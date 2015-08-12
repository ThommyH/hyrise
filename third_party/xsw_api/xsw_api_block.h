/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2015  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Block BIO API
 *
 * Filename: xsw_api_block.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Block BIO API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 03/12/15 Adrian      Created.
 *------------------------------------------------------------------------
 */


#ifndef __XSW_API_BLOCK_H
#define __XSW_API_BLOCK_H 

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XSW_BLOCK_INFO_T { 
	int device_id;
	u64 size;
	u32 blksize;
	void *virt_addr;  // DRAM BIO
	int refcnt;
	int type;
	char node[XSW_MAX_PATH];
} XSW_BLOCK_INFO;

int      XSWBlockUnloadModule(void);
int      XSWBlockCreate      (XSW_BIO *bio);
int      XSWBlockDestroy     (int device_id);
int      XSWBlockQuery       (int device_id, XSW_BLOCK_INFO *info);
int      XSWBlockQueryNode   (char *node, XSW_BLOCK_INFO *info);
int      XSWBlockNumDevices  (void);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_BLOCK_H 

