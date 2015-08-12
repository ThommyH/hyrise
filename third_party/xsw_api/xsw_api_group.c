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
 * Filename: xsw_api_group.c
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupDebug                                              */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWGroupDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_GROUP)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupSync                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWGroupSync(XSW_BIO *bio, u64 offset, u64 size, int sync)
{
int retCode = 0;

	// Move to first group item
	bio = bio->group_link; 

	// Walk through all items in group
	while(bio) {
		// If fsync fails, break;
		if ((retCode = XSWSync(bio, offset, size, sync)))
			break;
		// Move to the next BIO in the group
		bio = bio->group_link;
	}

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupTrim                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWGroupTrim(XSW_BIO *bio, u64 offset, u64 size)
{
int retCode = 0;

	// Move to first group item
	bio = bio->group_link; 

	// Walk through all items in group
	while(bio) {
		// If trim fails, break;
		if ((retCode = XSWTrim(bio, offset, size)))
			break;
		// Move to the next BIO in the group
		bio = bio->group_link;
	}

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWClose                                                   */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWGroupClose(XSW_BIO *group)
{
int retCode = 0;
XSW_BIO *bio;
XSW_BIO *next;

	// Move to first group item
	bio = group->group_link; 

	// Walk through all items in group
	while(bio) {
		// Sample next before we close the BIO
		next = bio->group_link;

		// If close fails, break
		if ((retCode=XSWClose(bio))) 
			break;

		// Remove BIO from group because we've closed the BIO and
		// don't want this deallocated BIO in a group anymore. 
		group->group_link = next;
		// Move to the next BIO 
		bio = next;
	}

	// Return status
	return(retCode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupWrite                                              */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static ssize_t XSWGroupWrite(XSW_BIO *bio, u64 offset, u64 size, void *buf)
{
ssize_t bytes = 0;
ssize_t retCode;

	// Move to first group item
	bio = bio->group_link; 

	// Walk through all items in group
	while(bio) {
		// Call write for BIO in group
		if ((retCode = XSWWrite(bio, offset, size, buf)) < 0) {
			// Break if write failed
			break;
		}
		// Add total bytes written
		bytes += retCode;
		// Move to the next BIO in the group
		bio = bio->group_link;
	}

	// Return total bytes written to the group
	return(bytes);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupInfo                                               */
/* Scope       : Private API                                                */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static int XSWGroupInfo(XSW_BIO *bio)
{
	XSWSysPrintf("XtremSW Group BIO:\n");

	// Setup list to base of group.
	bio = bio->group_link;

	while(bio) {
		// Call BIO info
		XSWInfo(bio);
		// Walk to next BIO in group
		bio = bio->group_link;
	}

	// Return Success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupAlloc                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_BIO *XSWGroupAlloc(void)
{
XSW_BIO *group;

	// Allocate a standard BIO
	if ((group = XSWAllocBIO(0))) {
		// Setup BIO type as a group
		group->type = XSW_BIO_TYPE_GROUP;

		// Setup group operations
		group->info  = XSWGroupInfo;
		group->close = XSWGroupClose;
		group->read  = 0;
		group->write = XSWGroupWrite;
		group->fsync = XSWGroupSync;
		group->trim  = XSWGroupTrim;
	}

	// Return group
	return(group);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupDealloc                                            */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWGroupDealloc(XSW_BIO *grp)
{
	// Verify BIO is a group
	if (grp->type != XSW_BIO_TYPE_GROUP) {
		XSWGroupDebug("XSWGroupDealloc() non group BIO: type %d\n", grp->type);
		// Return failure
		return(-1);
	}
	// Deallocate BIO
	XSWFreeBIO(grp);

	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupAddBIO                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWGroupAddBIO(XSW_BIO *grp, XSW_BIO *bio)
{
	// Verify BIO is a group
	if (grp->type != XSW_BIO_TYPE_GROUP) {
		XSWGroupDebug("XSWGroupAdd() non group BIO: type %d\n", grp->type);
		// Return failure
		return(-1);
	}

	// Setup BIO to point to current group link pointer
	bio->group_link = grp->group_link;

	// Set current group link pointer to new BIO.
	grp->group_link = bio;

	// Increase group size
	grp->size++;

	// Return success
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWGroupRemoveBIO                                          */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWGroupRemoveBIO(XSW_BIO *grp, XSW_BIO *bio)
{
XSW_BIO *walk = grp->group_link;
XSW_BIO *prev = grp;

	// Verify BIO is a group
	if (grp->type != XSW_BIO_TYPE_GROUP) {
		XSWGroupDebug("XSWGroupRemove() non group BIO: type %d\n", grp->type);
		// Return failure
		return(-1);
	}

	// Walk all members looking for this BIO in the group_link list
	while(walk) {
		// Is this the BIO we want to remove?
		if (walk == bio) {
			// Disconnect BIO.
			prev->group_link = walk->group_link;
			// Zero BIO's group_link
			walk->group_link = 0;
			// Decrease group size
			grp->size--;
			// Return success
			return(0);
		}
		// Setup previous to current
		prev = walk;
		// Walk to next BIO in the group_link
		walk = walk->group_link;
	}
	XSWGroupDebug("XSWGroupRemove() BIO %p not found in group %p\n", bio, grp);

	// Return failure
	return(-1);
}


