/*
 *------------------------------------------------------------------------
 * EMC Project XtremSW
 *
 * Confidential and Proprietary
 * Copyright (C) 2014  EMC  All Rights Reserved
 *------------------------------------------------------------------------
 *
 * Product: XtremSW configuration API
 *
 * Filename: xsw_api_conf.h
 *
 * Author: Adrian Michaud <Adrian.Michaud@emc.com>
 *
 * File Description: This is the XtremSW configuration API interface module.
 *
 *------------------------------------------------------------------------
 * File History
 *
 * Date     Init        Notes
 *------------------------------------------------------------------------
 * 10/09/14 Adrian      Created.
 *------------------------------------------------------------------------
 */

#ifndef __XSW_API_CONF_H
#define __XSW_API_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XSW_KEY_VALUE_T {
	char *text;
	char *key;
	char *value;
	struct XSW_KEY_VALUE_T *next;
	struct XSW_KEY_VALUE_T *prev;
} XSW_KEY_VALUE;

typedef struct XSW_CONFIG_T {
	XSW_KEY_VALUE  *key_values;
	char           *pathname;
} XSW_CONFIG;

#define XSW_CONFIG_ENABLE_CREATE 0x01

XSW_CONFIG*   XSWConfigOpen        (char *pathname, int options);
int           XSWConfigWrite       (XSW_CONFIG *config);
void          XSWConfigClose       (XSW_CONFIG *config);
char         *XSWConfigQueryString (XSW_CONFIG *config, char *key);
unsigned long XSWConfigQueryULong  (XSW_CONFIG *config, char *key);
void          XSWConfigSetString   (XSW_CONFIG *config, char *key, char *value);
void          XSWConfigSetULong    (XSW_CONFIG *config, char *key, unsigned long value);
int           XSWConfigDeleteKey   (XSW_CONFIG *config, char *key);

#ifdef __cplusplus
}
#endif

#endif // __XSW_CONF_H

