/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW group block I/O API
 *
 * Filename: xsw_api_group.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW group block I/O API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 03/06/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_GROUP_H
#define __XSW_API_GROUP_H 

#ifdef __cplusplus
extern "C" {
#endif

XSW_BIO *XSWGroupAlloc    (void);
int      XSWGroupDealloc  (XSW_BIO *grp);
int      XSWGroupAddBIO   (XSW_BIO *grp, XSW_BIO *bio);
int      XSWGroupRemoveBIO(XSW_BIO *grp, XSW_BIO *bio);

#ifdef __cplusplus
}
#endif

#endif


