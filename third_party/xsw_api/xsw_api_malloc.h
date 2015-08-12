/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW Malloc API
 *
 * Filename: xsw_api_malloc.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW Malloc API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 10/09/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_MALLOC_H
#define __XSW_API_MALLOC_H 

// Thread safe APIs
void* XSWMemMalloc     (size_t size);
void* XSWMemCalloc     (size_t nmemb, size_t size);
void* XSWMemRealloc    (void *ptr, size_t size);
void  XSWMemFree       (void *ptr);

#endif



