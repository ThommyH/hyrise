/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW DRAM BIO API
 *
 * Filename: xsw_api_dram.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW DRAM BIO API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/22/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_DRAM_H
#define __XSW_API_DRAM_H 

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XSW_DRAM_INFO_T { 
	int device_id;
	u64 phys_addr;
	u64 virt_addr;
	u64 size;
	int refcnt;
	char node[XSW_MAX_PATH];
} XSW_DRAM_INFO;

int      XSWDramUnloadModule(void);
XSW_BIO *XSWDramOpen        (char *device);
int      XSWDramCreate      (u64 phys_addr, u64 size);
int      XSWDramDestroy     (int device_id);
int      XSWDramQuery       (int device_id, XSW_DRAM_INFO *info);
int      XSWDramQueryNode   (char *node, XSW_DRAM_INFO *info);
int      XSWDramNumDevices  (void);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_DRAM_H 

