/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Filename: xsw_internal.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This provices misc internal utilities
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 10/27/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_INTERNAL_H
#define __XSW_INTERNAL_H 

void *XSWInternalMalloc(size_t size);
void  XSWInternalFree  (void *ptr);
char *XSWInternalStrdup(char *string);

#endif

