hyr-units := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

include $(hyr-units)/../../../rules.mk

include $(PROJECT_ROOT)/src/lib/helper/Makefile
include $(PROJECT_ROOT)/third_party/gtest/Makefile
include $(PROJECT_ROOT)/src/lib/memory/Makefile

hyr-units.binname := units
hyr-units.deps := hyr-memory hyr-helper gtest
$(eval $(call test-binary,hyr-units))
