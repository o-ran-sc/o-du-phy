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

ifdef DPDK_WLS
ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-icc
RTE_OUTPUT = ${PWD}

include $(RTE_SDK)/mk/rte.vars.mk
SRCS-y := wls_lib_dpdk.c syslib.c
SHARED = libwls.so
#CFLAGS +=-Wall -wd9 -DDPDK_WLS
CFLAGS +=-Wall -DDPDK_WLS -fstack-protector
EXTRA_CFLAGS +=-g
include $(RTE_SDK)/mk/rte.extshared.mk

else

obj-m := wls.o
wls-objs :=syslib.o \
	   wls_drv.o

INSTALL_MOD_DIR ?=/lib/modules/`uname -r`/updates/drivers/intel/wls
cmd_depmod = /sbin/depmod -a
EXTRA_CFLAGS +=-g -Wall
SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) $(EXTRA_CFLAGS) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) $(EXTRA_CFLAGS) M=$(SRC) modules_install

wls_lib.o: wls_lib.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -D_GNU_SOURCE -g  -fPIC -o $@ -c $<

syslib_user.o: syslib.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -g  -fPIC -o $@ -c $<

libwls.so: wls_lib.o syslib_user.o
	$(CC) $(LDFLAGS) -g -shared -fPIC -Wl,-soname,libwls.so -o $@ wls_lib.o syslib_user.o

install:
	[ -d "${INSTALL_MOD_DIR}" ] || mkdir -p ${INSTALL_MOD_DIR}
	cp ./wls.ko ${INSTALL_MOD_DIR}
	$(call cmd_depmod)
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.so *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
endif
