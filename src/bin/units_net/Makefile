hyr-units-net := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

include $(hyr-units-net)/../../../rules.mk

include $(PROJECT_ROOT)/src/lib/net/Makefile
include $(PROJECT_ROOT)/third_party/gtest/Makefile
include $(PROJECT_ROOT)/src/lib/memory/Makefile


hyr-units-net.binname := units-net
hyr-units-net.deps := hyr-memory hyr-net gtest hyr-access
$(eval $(call test-binary,hyr-units-net))
