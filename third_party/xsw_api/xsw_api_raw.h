/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW RAW Block I/O API
 *
 * Filename: xsw_api_raw.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW RAW block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_RAW_H
#define __XSW_API_RAW_H 

#ifdef __cplusplus
extern "C" {
#endif

XSW_BIO *XSWRawOpen  (char *device_path);
XSW_BIO *XSWRawOpenEx(int fd, u64 offset, u64 size);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_RAW_H




