/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2013  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW API master header file.
 *
 * Filename: xsw_apis.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW API master header file.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 04/04/13 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_APIS_H
#define __XSW_APIS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __XSW_TYPES
#define __XSW_TYPES
#include <linux/types.h>
#include <sys/types.h>
typedef __u32 u32;
typedef __u16 u16;
typedef __u8  u8;
typedef __u64 u64;
#endif // __XSW_TYPES

#define XSW_UNUSED_ARG(arg) (void)(arg)

#include "xsw_api_system.h"
#include "xsw_api_bio.h"
#include "xsw_api_mmap.h"
#include "xsw_api_pagecache.h"
#include "xsw_api_xke.h"
#include "xsw_api_raw.h"
#include "xsw_api_region.h"
#include "xsw_api_ckpt.h"
#include "xsw_api_dram.h"
#include "xsw_api_file.h"
#include "xsw_api_module.h"
#include "xsw_api_debug.h"
#include "xsw_api_conf.h"
#include "xsw_api_group.h"
#include "xsw_api_anon.h"
#include "xsw_api_malloc.h"
#include "xsw_api_atomic.h"
#include "xsw_api_block.h"

#ifdef __cplusplus
}
#endif

#endif // __XSW_APIS_H

