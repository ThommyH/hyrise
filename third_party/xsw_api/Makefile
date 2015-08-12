#
# ------------------------------------------------------------------------
#  EMC Project XtremSW
# 
#  Confidential and Proprietary
#  Copyright (C) 2012-2014 EMC All Rights Reserved
# ------------------------------------------------------------------------
# 
#  Filename: Makefile
# 
#  Description: API Library
# 
#  Author: Adrian Michaud <Adrian.Michaud@emc.com>
# 
# ------------------------------------------------------------------------
#  File History
# 
#  Date     Init        Notes
# ------------------------------------------------------------------------
#  05/13/13 Adrian      Created.
# ------------------------------------------------------------------------
#

XSW_INCLUDE_DIR=/usr/local/include/xsw
XSW_LIB1_DIR=/usr/lib
XSW_LIB2_DIR=/usr/lib64
XSW_LIB3_DIR=/usr/local/lib

#XSW_OPT_FLAGS= -Wall -fPIC -DPIC -g
XSW_OPT_FLAGS= -O3 -Wall -fPIC -DPIC
XSW_SHARED_LIB=so
XSW_ARCHIVE_LIB=a

XSW_API_LIB=libxsw_api

XSW_API_OBJECTS=xsw_internal.o xsw_api_mmap.o xsw_api_ckpt.o xsw_api_conf.o \
 xsw_api_bio.o xsw_api_region.o xsw_api_module.o xsw_api_dram.o xsw_api_file.o xsw_api_raw.o \
 xsw_api_debug.o xsw_api_group.o xsw_api_system.o xsw_api_anon.o xsw_api_malloc.o \
 xsw_api_pagecache.o xsw_api_xke.o xsw_api_block.o

all: building $(XSW_API_LIB).$(XSW_SHARED_LIB) $(XSW_API_LIB).$(XSW_ARCHIVE_LIB)
	@install -d $(XSW_INCLUDE_DIR)
	@install -m 644 xsw_api*.h $(XSW_INCLUDE_DIR)
	@install -d $(XSW_LIB1_DIR)
	@install -d $(XSW_LIB2_DIR)
	@install -d $(XSW_LIB3_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_SHARED_LIB) $(XSW_LIB1_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_SHARED_LIB) $(XSW_LIB2_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_SHARED_LIB) $(XSW_LIB3_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_ARCHIVE_LIB) $(XSW_LIB1_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_ARCHIVE_LIB) $(XSW_LIB2_DIR)
	@install -m 755 $(XSW_API_LIB).$(XSW_ARCHIVE_LIB) $(XSW_LIB3_DIR)

building:
	@echo
	@echo "################################################"
	@echo "#                                              #"
	@echo "#                Building APIs                 #"
	@echo "#                                              #"
	@echo "################################################"
	@echo

$(XSW_API_LIB).$(XSW_SHARED_LIB) : $(XSW_API_OBJECTS)
	gcc -shared -Wl,-soname,$(@F) -o $@ $+ -lpthread

$(XSW_API_LIB).$(XSW_ARCHIVE_LIB) : $(XSW_API_OBJECTS)
	ar crus $@ $+

%.o : %.c
	gcc -c $(XSW_OPT_FLAGS) -W -Wredundant-decls  $<

clean:
	@echo "Cleaning APIs"
	@rm -f *.o
	@rm -f $(XSW_API_LIB).*
