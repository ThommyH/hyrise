/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW File BIO API
 *
 * Filename: xsw_api_file.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW File BIO API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_FILE_H
#define __XSW_API_FILE_H 

#ifdef __cplusplus
extern "C" {
#endif

XSW_BIO *XSWFileOpen   (char *pathname);
XSW_BIO *XSWFileOpenEx (int fd, u64 offset, u64 size);
XSW_BIO *XSWFileCreate (char *pathname, u64 size);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_FILE_H 

