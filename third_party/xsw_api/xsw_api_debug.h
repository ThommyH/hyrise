/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW debug API
 *
 * Filename: xsw_api_debug.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW debug API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_DEBUG_H
#define __XSW_API_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#define XSW_DEBUG_MMAP      0x0001  // Select MMAP debug
#define XSW_DEBUG_MODULE    0x0002  // Select module debug
#define XSW_DEBUG_REGION    0x0004  // Select region debug
#define XSW_DEBUG_BIO       0x0008  // Select base block I/O debug
#define XSW_DEBUG_CKPT      0x0010  // Select checkpoint debug
#define XSW_DEBUG_RAW       0x0020  // Select raw block I/O debug
#define XSW_DEBUG_CONFIG    0x0040  // Select configuration debug
#define XSW_DEBUG_FILE      0x0080  // Select file block I/O debug
#define XSW_DEBUG_GROUP     0x0100  // Select group BIO debug
#define XSW_DEBUG_DRAM      0x0200  // Select DRAM block I/O debug
#define XSW_DEBUG_ANON      0x0400  // Select Anonymous block I/O debug
#define XSW_DEBUG_XKE       0x0800  // Select XKE debug
#define XSW_DEBUG_BLOCK     0x1000  // Select Block debug
#define XSW_DEBUG_ALL       0xFFFF  // Select all 

#define XSW_DEBUG_ON        1
#define XSW_DEBUG_OFF       0

int  XSWDebugEnable      (int select, int enable);
void XSWDebugOut         (char *string);
void XSWDebugSetOutput   (char *filename);
int  XSWDebugEnabled     (int select);
void XSWDebugKernel      (const char *fmt, ...);
void XSWDebugKernelStack (void);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_DEBUG_H

