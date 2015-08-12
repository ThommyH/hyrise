/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Anonymous Block I/O API
 *
 * Filename: xsw_api_anon.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Anonymous block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 06/26/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_ANON_H
#define __XSW_API_ANON_H 

#ifdef __cplusplus
extern "C" {
#endif

int      XSWAnonUnloadModule(void);
XSW_BIO *XSWAnonOpen        (XSW_BIO *bio, u64 size);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_ANON_H




