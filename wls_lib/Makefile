###############################################################################
#
#   Copyright (c) 2019 Intel.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
###############################################################################

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-icc
RTE_OUTPUT = ${PWD}

include $(RTE_SDK)/mk/rte.vars.mk
SRCS-y := wls_lib_dpdk.c syslib.c
SHARED = libwls.so

CFLAGS +=-Wall -fstack-protector
EXTRA_CFLAGS +=-g
include $(RTE_SDK)/mk/rte.extshared.mk
