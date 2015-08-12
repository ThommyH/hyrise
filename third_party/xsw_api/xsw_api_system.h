/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW System API
 *
 * Filename: xsw_api_system.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW System API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 03/06/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_SYSTEM_H
#define __XSW_API_SYSTEM_H 

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XSW_SYSTEM_SETUP_T {
	int   (*xsw_printf)(const char *fmt, ...);
} XSW_SYS_SETUP;

#define XSW_LOCK_MMAP_LIST        0 // If adding more, increase _locks in xsw_api_system.c 
#define XSW_LOCK_CONFIG           1 // If adding more, increase _locks in xsw_api_system.c
#define XSW_LOCK_USER_MALLOC      2 // If adding more, increase _locks in xsw_api_system.c
#define XSW_LOCK_INTERNAL_MALLOC  3 // If adding more, increase _locks in xsw_api_system.c

int  XSWSysSetup     (XSW_SYS_SETUP *setup);
void XSWSysEnableFork(void);
int  XSWSysPrintf    (const char *fmt, ...);
void XSWSysLock      (int index);
void XSWSysUnlock    (int index);

#ifdef __cplusplus
}
#endif

#endif


