hyr-memory := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

-include ../../../rules.mk

hyr-memory.libname := hyr-memory
hyr-memory.libs := hwloc rt
hyr-memory.deps := xsw
$(eval $(call library,hyr-memory))

INCLUDE_DIRS += $(PROJECT_ROOT)/third_party
WITH_XSW := 1