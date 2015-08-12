/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Module API
 *
 * Filename: xsw_api_module.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW module API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_MODULE_H
#define __XSW_API_MODULE_H 

#ifdef __cplusplus
extern "C" {
#endif

int  XSWLoadModule       (char *module);
int  XSWUnloadModule     (char *module);

#ifdef __cplusplus
}
#endif

#endif // __XSW_API_MODULE_H 

