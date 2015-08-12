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
 * Filename: xsw_api_xke.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW XKE API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 11/10/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_XKE_H
#define __XSW_API_XKE_H

#ifdef __cplusplus
extern "C" {
#endif

#define XSWMMAP_MAX_XKE_SEARCH     128

typedef struct XSW_XKE_INFO_T {
	XSW_ID pcid;
	char   process[XSW_MAX_PATH];
	char   search[XSWMMAP_MAX_XKE_SEARCH];
	char   swap[XSW_MAX_PATH];
} XSW_XKE_INFO;

int      XSWXkeEnable (char *process, char *search, XSW_BIO *swap, XSW_ID pcid);
int      XSWXkeDisable(char *process, char *search);
int      XSWXkeQuery  (XSW_XKE_INFO *info, int *count);
int      XSWXkeInstall(unsigned long sys_call_table);
int      XSWXkeRemove (void);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_XKE_H

