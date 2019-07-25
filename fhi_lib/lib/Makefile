#/******************************************************************************
#*
#*   Copyright (c) 2019 Intel.
#*
#*   Licensed under the Apache License, Version 2.0 (the "License");
#*   you may not use this file except in compliance with the License.
#*   You may obtain a copy of the License at
#*
#*       http://www.apache.org/licenses/LICENSE-2.0
#*
#*   Unless required by applicable law or agreed to in writing, software
#*   distributed under the License is distributed on an "AS IS" BASIS,
#*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*   See the License for the specific language governing permissions and
#*   limitations under the License.
#*
#*******************************************************************************/
CC := icc
AR := xiar

ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

RTE_TARGET := x86_64-native-linuxapp-icc
RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include
#include $(RTE_SDK)/mk/rte.vars.mk
API_DIR := $(PWD)/api
SRC_DIR := $(PWD)/src
ETH_DIR := $(PWD)/ethernet


ifeq ($(MLOG_DIR),)
    MLOG_DIR=$(XRAN_DIR)/../mlog
endif

CFLAGS += -std=gnu11 -Wall -wd9 -Wno-deprecated-declarations -Wextra -Werror -qopt-report-phase:all -qopt-zmm-usage=high \
	-fdata-sections \
	-ffunction-sections \
	-restrict \
	-g \
	-Wall \
	-Wimplicit-function-declaration \
	-Werror \
	-Wextra \
	-no-inline-max-total-size \
	-no-inline-max-size \
        -I$(API_DIR) -I$(ETH_DIR) -I$(MLOG_DIR)/source -I$(RTE_INC) -g -O3

ifeq ($(ME),1)
    CFLAGS += -DMLOG_ENABLED
endif
SRC = $(ETH_DIR)/ethdi.c \
	$(ETH_DIR)/ethernet.c \
	$(SRC_DIR)/xran_up_api.c \
	$(SRC_DIR)/xran_sync_api.c \
	$(SRC_DIR)/xran_timer.c \
	$(SRC_DIR)/xran_cp_api.c 	\
	$(SRC_DIR)/xran_transport.c  \
	$(SRC_DIR)/xran_common.c        \
	$(SRC_DIR)/xran_ul_tables.c     \
	$(SRC_DIR)/xran_main.c 

FLEX_C_CRAN_LIB = libxran.a

OBJ = $(foreach file,$(SRC),$(file:.c=.o))


all: flex_lib install

$(OBJ): %.o: %.c
	$(CC) $(CFLAGS) -I$(API_DIR) -c $< -o $@ 


flex_lib:$(FLEX_C_CRAN_LIB)

$(FLEX_C_CRAN_LIB): $(OBJ)
	$(AR) rsu $@ $^

install: $(FLEX_C_CRAN_LIB)
	mkdir -p bin
	@mv $(FLEX_C_CRAN_LIB) ./bin

clean:
	@rm -rf $(FLEX_C_CRAN_LIB) $(OBJ) ./bin/$(FLEX_C_CRAN_LIB)

#include $(RTE_SDK)/mk/rte.extlib.mk
