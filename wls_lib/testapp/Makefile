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
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = wls_test

LDFLAGS += -L../.. -l wls -lpthread -lhugetlbfs

# all source are stored in SRCS-y
SRCS-y := pool.c testapp.c

CFLAGS += $(WERROR_FLAGS) -I../.. -DDPDK_WLS
EXTRA_CFLAGS += -g

include $(RTE_SDK)/mk/rte.extapp.mk

else

#EXTRA_CFLAGS +=-g -Wall -wd9 -I../
EXTRA_CFLAGS +=-g -Wall -I../
LDFLAGS += -L../ -lwls

# all source are stored in SRCS-y
SRCS-y := pool.c testapp.c
wls_test: testapp.o pool.o
	$(CC) $(LDFLAGS) $(EXTRA_CFLAGS) -o $@ testapp.o pool.o -L . -l wls -l pthread -l hugetlbfs

CFLAGS += $(WERROR_FLAGS) -g -I../../
testapp.o: testapp.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

pool.o: pool.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

clean:
	rm -f *.o wls_test
endif
