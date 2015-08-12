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
 * Filename: xsw_api_conf.c
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "xsw_apis.h"
#include "xsw_internal.h"

#define XSW_CHAR_LF 0x0a   // Linefeed
#define XSW_CHAR_CR 0x0d   // Carrage return

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigDebug                                             */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWConfigDebug(const char *fmt, ...) 
{
va_list args;
char string[256];

	if (XSWDebugEnabled(XSW_DEBUG_CONFIG)) {
		va_start(args,fmt);
		vsprintf(string, fmt, args);
		va_end(args);
		XSWDebugOut(string); 
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigAddKeyValue                                       */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWConfigAddKeyValue(XSW_CONFIG *config, char *text, int text_len, char *key, int key_len, char *value, int value_len)
{
XSW_KEY_VALUE *key_value;

	// Allocate new key/value store
	key_value = XSWInternalMalloc(sizeof(XSW_KEY_VALUE));
	// Zero out key_value
	memset(key_value, 0, sizeof(XSW_KEY_VALUE));
	// Is there file text for this line?	
	if (text) {
		// Allocate text string
		key_value->text = XSWInternalMalloc(text_len+1);
		// Copy text
		memcpy(key_value->text, text, text_len);
		// Insert NULL for text
		key_value->text[text_len] = 0;
	}

	// Is there a key/value?
	if (key && value) {
		// Allocate key
		key_value->key = XSWInternalMalloc(key_len+1);
		// Copy key
		memcpy(key_value->key, key, key_len);
		// Insert NULL for key
		key_value->key[key_len] = 0;
		// Allocate value
		key_value->value = XSWInternalMalloc(value_len+1);
		// Copy value
		memcpy(key_value->value, value, value_len);
		// Insert NULL for value
		key_value->value[value_len] = 0;
	}
	// Setup prev pointer
	key_value->prev = 0;
	// Setup next pointer
	key_value->next = config->key_values;
	// Adjust current base (if any)
	if (config->key_values)
		config->key_values->prev = key_value;
	// Assign as new base
	config->key_values = key_value;
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigParseLine                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWConfigParseLine(XSW_CONFIG *config, char *text, int text_len)
{
int i, key, len=0, quotes, dquotes;
char *line = 0;

	// Is there any length to this line?
	if (text_len) {
		// Allocate new line
		line = XSWInternalMalloc(text_len);

		// Strip all whitespace on line.
		for (i=0, dquotes=0, quotes=0; i<text_len; i++) {
			// Only strip whitespace if outside of quotes.
			if (text[i] <= 32 && !quotes && !dquotes) {
				// Skip over
				continue;
			}
			// Check for double quotes
			if (text[i] == '"')	{
				// Toggle double quote flag
				dquotes = ~dquotes;
				// Skip over
				continue;
			}
			// Check for single quotes
			if (text[i] == '\'') {
				// Toggle single quote flag
				quotes = ~quotes;
				// Skip over
				continue;
			}
			// Copy data into new line
			line[len++] = text[i];
		}
	}

	// Was there any non-whitespace?
	if (len) {
		// Ignore comments
		if (line[0] == '#' || line[0] == '/' || line[0] == '!' || line[0] == '@') {
			// Add comment only
			XSWConfigAddKeyValue(config, text, text_len, 0, 0, 0, 0);
		} else {
			// Find key
			for (key=0; key<len; key++) {
				// Is this the key assignment operator?
				if (line[key] == '=') {
					// Does the key have any length and is there any data?
					if (key && ((key+1) < len)) {
						// Add new key to system
						XSWConfigAddKeyValue(config, text, text_len, line, key, &line[key+1], len-(key+1));
						// Free line
						XSWInternalFree(line);
						// We're done
						return;
					}
				}
			}
			// Just pass line though
			XSWConfigAddKeyValue(config, text, text_len, 0, 0, 0, 0);
		}
	} else {
		// Add empty line
		XSWConfigAddKeyValue(config, 0, 0, 0, 0, 0, 0);
	}
	// Free line
   	XSWInternalFree(line);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigParseData                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static void XSWConfigParseData(XSW_CONFIG *config, char *data, int len)
{
int head, i;

	// Scan all lines (if any) in loaded data
	for (head=0, i=0; i<len; i++) {
		// Is this the end of the current line?
		if (data[i] == XSW_CHAR_LF) {
			// Parse line
			XSWConfigParseLine(config, &data[head], i-head);
			// Adjust new head
			head = i+1;
		}
	}
	// If last line isn't terminated with a linefeed, handle it here
	if (i > head) {
		// Parse last line
		XSWConfigParseLine(config, &data[head], i-head);
	}
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigWrite                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWConfigWrite(XSW_CONFIG *config)
{
XSW_KEY_VALUE *walk;
int fd;
int retcode = -1;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Is pathname set?
	if (config->pathname) {
		// Open file for writing
		if ((fd = open(config->pathname, O_WRONLY|O_CREAT, 0777)) >= 0) {
			// Setup base
			walk = config->key_values;
			// Walk though all keys
			while(walk) {
				// Is there text data we should write out?
				if (walk->text) {
					// Write the file text
					write(fd, walk->text, strlen(walk->text));
					write(fd, "\n", strlen("\n"));
				} else {
					// Is this a modified/new key/value?
					if (walk->key && walk->value) {
						// Write the new key/value
						write(fd, walk->key, strlen(walk->key));
						write(fd, " = \"", 4);
						write(fd, walk->value, strlen(walk->value));
						write(fd, "\"\n", strlen("\"\n"));
					} else {
    		          	// Write empty line
						write(fd, "\n", strlen("\n"));
					}
				}
				// move to the next
		   	    walk = walk->next;
			}
			// Close file
			close(fd);
			// Setup success
			retcode = 0;
		} else {
			// Error opening/creating
			XSWConfigDebug("XSWConfigWrite(): Unable to write '%s'\n", config->pathname);
		}
	}
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
	// Return success or fail
	return(retcode);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigClose                                             */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWConfigClose(XSW_CONFIG *config)
{
XSW_KEY_VALUE *walk;
XSW_KEY_VALUE *next;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Setup base
	walk = config->key_values;
	// Walk though all keys
	while(walk) {
		// Setup next
		next = walk->next;
		// Free key
		XSWInternalFree(walk->key);
		// Free value
		XSWInternalFree(walk->value);
       	// Free data
       	XSWInternalFree(walk->text);
		// Free current key_value
		XSWInternalFree(walk);
		// move to the next
		walk = next;
	}
	// Free pathname
	XSWInternalFree(config->pathname);
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
	// Free config
	XSWInternalFree(config);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigOpen                                              */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
XSW_CONFIG* XSWConfigOpen(char *pathname, int options)
{
XSW_CONFIG* config;
char *data = 0;
int fd;
struct stat info;

	// Allocate a new config 
	config = XSWInternalMalloc(sizeof(XSW_CONFIG));
	// Zero out config
	memset(config, 0, sizeof(XSW_CONFIG));
	// Open file
	if ((fd = open(pathname, O_RDWR)) >= 0) {
		// Get file info
		fstat(fd, &info);
		// Only allocate/read in data if there is data to read
		if (info.st_size) {
			// Allocate buffer to hold config data
			data = XSWInternalMalloc(info.st_size);
			// Read in data
			if (pread(fd, data, info.st_size, 0) != info.st_size) {
				// Close file
				close(fd);
				// Free data
				XSWInternalFree(data);
				// Free config
				XSWInternalFree(config);
				// Error reading
				XSWConfigDebug("XSWConfigOpen(): error reading '%s'\n", pathname);
				// Return error
				return(0);
			}
			// Parse data
			XSWConfigParseData(config, data, (int)info.st_size);
			// Free data
			XSWInternalFree(data);
			// Close file
			close(fd);
		}
	} else {
		// Are we aloud to create?
		if (!(options & XSW_CONFIG_ENABLE_CREATE)) {
			// Free config
			XSWInternalFree(config);
			// Return error
			return(0);
		}
	}

	// Copy pathname
	config->pathname = XSWInternalStrdup(pathname);
	// Return Success
	return(config);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigFindKey                                           */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static XSW_KEY_VALUE *XSWConfigFindKey(XSW_CONFIG *config, char *key)
{
XSW_KEY_VALUE *walk;

	// Setup base
	walk = config->key_values;
	// Walk though all keys
	while(walk) {
		// Is this a valid key?
		if (walk->key && walk->value) {
			// Check if this is the key we're looking for. Ignore case
			if (!strcasecmp(key, walk->key)) {
				// Return key/value
				return(walk);
			}
		}
		// move to the next
		walk = walk->next;
	}
	// Return NULL if not found
	return(0);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigQueryString                                       */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
char *XSWConfigQueryString(XSW_CONFIG *config, char *key)
{
XSW_KEY_VALUE *key_value;
char *value = 0;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Find key, if any
	if ((key_value = XSWConfigFindKey(config, key))) {
		// Setup result
		value = key_value->value;
	}
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
	// Return string or NULL if key doesn't exist
	return(value);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigConvertULong                                      */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
static unsigned long XSWConfigConvertULong(char *string)
{
unsigned long value;
char *non_digit=0;

	// Does this string end with a modifier? 
	if ((value = strtoul(string, &non_digit, 0))) {
		// Look for supported modifiers (P,T,G,M,K)
		switch(*non_digit) {
			// Peta
			case 'p': case 'P':
				value = (value * XSW_1P);
				break;
			// Tera
			case 't': case 'T':
				value = (value * XSW_1T);
				break;
			// Giga
			case 'g': case 'G':
				value = (value * XSW_1G);
				break;
			// Mega
			case 'm': case 'M':
				value = (value * XSW_1M);
				break;
			// Kilo
			case 'k': case 'K':
				value = (value * XSW_1K);
				break;
		}
	}
	// Return result
	return(value);
}


/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigQueryULong                                        */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
unsigned long XSWConfigQueryULong(XSW_CONFIG *config, char *key)
{
XSW_KEY_VALUE *key_value;
unsigned long value = 0;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Find key, if any
	if ((key_value = XSWConfigFindKey(config, key))) {
		// Setup result
		value = XSWConfigConvertULong(key_value->value);
	}
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
	// Return value or 0 if key doesn't exist
	return(value);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigSetString                                         */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWConfigSetString(XSW_CONFIG *config, char *key, char *value)
{
XSW_KEY_VALUE *key_value;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Find key, if any
	if ((key_value = XSWConfigFindKey(config, key))) {
		// Free text data
		XSWInternalFree(key_value->text);
		// Zero text pointer
		key_value->text = 0;
		// Free old value
		XSWInternalFree(key_value->value);
		// Copy new value
		key_value->value = XSWInternalStrdup(value);
	} else {
		// Add new key/value
		XSWConfigAddKeyValue(config, 0, 0, key, strlen(key), value, strlen(value));
	}
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigSetULong                                          */
/* Scope       : Public API                                                 */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
void XSWConfigSetULong(XSW_CONFIG *config, char *key, unsigned long value)
{
XSW_KEY_VALUE *key_value;
char new_value[32];

	// Convert value to string
	sprintf(new_value, "%lu", value);
	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Find key, if any
	if ((key_value = XSWConfigFindKey(config, key))) {
		// Discard text data now that we're changing this value
		XSWInternalFree(key_value->text);
		// Zero text pointer
		key_value->text = 0;
		// Free old value
		XSWInternalFree(key_value->value);
		// Copy new value
		key_value->value = XSWInternalStrdup(new_value);
	} else {
		// Add new key/value
		XSWConfigAddKeyValue(config, 0, 0, key, strlen(key), new_value, strlen(new_value));
	}
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
}

/****************************************************************************/
/*                                                                          */
/* Function    : XSWConfigDeleteKey                                         */
/* Scope       : Internal API                                               */
/* Description :                                                            */
/* Arguments   :                                                            */
/* Returns     :                                                            */
/*                                                                          */
/****************************************************************************/
int XSWConfigDeleteKey(XSW_CONFIG *config, char *key)
{
XSW_KEY_VALUE *key_value;
int retcode = -1;

	// Acquire the config mutex
	XSWSysLock(XSW_LOCK_CONFIG);
	// Find key, if any
	if ((key_value = XSWConfigFindKey(config, key))) {
		// Free old value
		XSWInternalFree(key_value->value);
		// Free old key
		XSWInternalFree(key_value->key);
		// Free text data
		XSWInternalFree(key_value->text);
		// Adjust next (if any)
		if (key_value->next)
			key_value->next->prev = key_value->prev;
		// Adjust prev (if any)
		if (key_value->prev)
			key_value->prev->next = key_value->next;
		// Adjust base if we are the base
		if (config->key_values == key_value)
			config->key_values = key_value->next;
		// Free the key value
		XSWInternalFree(key_value);
		// Setup success
		retcode = 0;
	} 
	// Release the config mutex
	XSWSysUnlock(XSW_LOCK_CONFIG);
	// Return retcode
	return(retcode);
}


