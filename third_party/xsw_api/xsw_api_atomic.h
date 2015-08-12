/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW atomic API
 *
 * Filename: xsw_api_atomic.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: Provice 64-bit atomic add/subtract operations.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 11/03/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_ATOMIC_H
#define __XSW_API_ATOMIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAtomicAdd64                                             */
/* Scope       : Public API                                                 */
/* Description : Provide an atomic 64-bit addition.                         */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline u64 XSWAtomicAdd64(u64 *data, u64 value)
{
	// 64-bit atomic addq using lock prefix
	asm volatile ("lock; xaddq %0, %1;" : "+r" (value), "=m" (*data) : "m" (*data));
	// Return result
	return(value);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWAtomicSub64                                             */
/* Scope       : Public API                                                 */
/* Description : Provide an atomic 64-bit subtraction.                      */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static inline u64 XSWAtomicSub64(u64 *data, u64 value)
{
	// Convert to two's complement and subtract using xaddq
	value = (u64)(-(int64_t)value);
	// 64-bit atomic addq using lock prefix
	asm volatile ("lock; xaddq %0, %1;" : "+r" (value), "=m" (*data) : "m" (*data));
	// Return result
	return(value);
}

#ifdef __cplusplus
}
#endif

#endif // __XSW_CONF_H


