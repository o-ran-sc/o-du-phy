#/******************************************************************************
#*
#*   Copyright (c) 2020 Intel.
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


.SUFFIXES: .o .c .s .i .cpp

##############################################################
#  Tools configuration
##############################################################
CC  := icc
CXX := icpc
CPP := icpc
AS := as
AR := ar
LD := icc
OBJDUMP := objdump

ifeq ($(SHELL),cmd.exe)
MD := mkdir.exe -p
CP := cp.exe -f
RM := rm.exe -rf
else
MD := mkdir -p
CP := cp -f
RM := rm -rf
endif

ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

ifeq ($(MLOG_DIR),)
    MLOG_DIR=$(XRAN_DIR)/../mlog
endif

RTE_TARGET ?= x86_64-native-linuxapp-icc

RTE_LIBS = $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --static --libs libdpdk)
RTE_INC := $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --cflags-only-I libdpdk)


# Where to find user code.
COMMON_TEST_DIR = $(XRAN_DIR)/test/common
USER_DIR = $(XRAN_DIR)/lib/src
USER_ETH = $(XRAN_DIR)/lib/ethernet
USER_API = $(XRAN_DIR)/lib/api

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
CPPFLAGS += -isystem $(GTEST_ROOT)/include

# Flags passed to the C++ compiler.
CXXFLAGS += -g -std=c++14 -Wall -Wextra -pthread -mcmodel=large -I$(USER_API) -I$(USER_DIR) -I$(USER_ETH) -I$(MLOG_DIR)/source -I $(COMMON_TEST_DIR) -I$(RTE_INC)

# All tests produced by this Makefile.  Remember to add new tests you
# created to the list.
TESTS = unittests

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_ROOT)/include/gtest/*.h \
                $(GTEST_ROOT)/include/gtest/internal/*.h

CFLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations \
	-fdata-sections \
	-ffunction-sections \
	-g \
	-Wall \
	-Wimplicit-function-declaration  \
	-wd1786 \
	-mcmodel=large \
	-I$(USER_API) -I$(USER_DIR) -I$(USER_ETH) -I$(MLOG_DIR)/source -I$(RTE_INC)

C_SRC = \
	$(USER_ETH)/ethdi.c \
	$(USER_ETH)/ethernet.c \
	$(USER_DIR)/xran_up_api.c \
	$(USER_DIR)/xran_sync_api.c \
	$(USER_DIR)/xran_timer.c \
	$(USER_DIR)/xran_cp_api.c \
	$(USER_DIR)/xran_transport.c \
	$(USER_DIR)/xran_common.c \
	$(USER_DIR)/xran_ul_tables.c \
	$(USER_DIR)/xran_frame_struct.c \
	$(USER_DIR)/xran_app_frag.c \
	$(USER_DIR)/xran_dev.c \
	$(USER_DIR)/xran_rx_proc.c	\
	$(USER_DIR)/xran_tx_proc.c	\
	$(USER_DIR)/xran_cp_proc.c	\
	$(USER_DIR)/xran_cb_proc.c	\
	$(USER_DIR)/xran_mem_mgr.c	\
	$(USER_DIR)/xran_main.c \
    $(USER_DIR)/xran_delay_measurement.c

CC_SRC = \
	$(COMMON_TEST_DIR)/xranlib_unit_test_main.cc \
	c_plane_tests.cc \
	chain_tests.cc \
	prach_functional.cc \
	prach_performance.cc \
	u_plane_functional.cc \
	u_plane_performance.cc \
	init_sys_functional.cc \
	compander_functional.cc \
	mod_compression_unit_test.cc \
	unittests.cc

#	u_plane_performance.cc \

CPP_SRC = $(COMMON_TEST_DIR)/common.cpp \
	$(USER_DIR)/xran_compression.cpp \
	$(USER_DIR)/xran_bfp_ref.cpp \
	$(USER_DIR)/xran_bfp_cplane8.cpp \
	$(USER_DIR)/xran_bfp_cplane16.cpp \
	$(USER_DIR)/xran_bfp_cplane32.cpp \
	$(USER_DIR)/xran_bfp_cplane64.cpp \
	$(USER_DIR)/xran_bfp_uplane_9b16rb.cpp \
	$(USER_DIR)/xran_bfp_uplane.cpp \
	$(USER_DIR)/xran_mod_compression.cpp

CPP_SRC_SNC = $(USER_DIR)/xran_compression_snc.cpp \
	$(USER_DIR)/xran_bfp_cplane8_snc.cpp \
	$(USER_DIR)/xran_bfp_cplane16_snc.cpp \
	$(USER_DIR)/xran_bfp_cplane32_snc.cpp \
	$(USER_DIR)/xran_bfp_cplane64_snc.cpp \
	$(USER_DIR)/xran_bfp_uplane_snc.cpp


C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CC_OBJS := $(patsubst %.cc,%.o,$(CC_SRC))
CPP_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRC))
CPP_SNC_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRC_SNC))

CPPFLAGS += -I$(USER_DIR) -I$(USER_API)

#-qopt-report=5 -qopt-matmul -qopt-report-phase=all
CPP_COMP := -O3 -DNDEBUG  -xcore-avx512 -fPIE -restrict -fasm-blocks
CPP_COMP_SNC := -O3 -DNDEBUG -march=icelake-server -fPIE -restrict -fasm-blocks

CPP_COMP := $(CPP_COMP)
CPP_COMP_SNC := $(CPP_COMP_SNC)

ifeq ($(GEN_ASM), 1)
CPP_ASMS := $(patsubst %.cpp,%.asm,$(CPP_SRC))
CPP_SNC_ASMS := $(patsubst %.cpp,%.asm,$(CPP_SRC_SNC))
CPP_COMP += -qopt-report=5 -qopt-matmul -qopt-report-phase=all
CPP_COMP_SNC += -qopt-report=5 -qopt-matmul -qopt-report-phase=all
endif

PROJECT_DEP_FILE := $(TESTS).dep

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))
GENERATE_DEPS :=
else
C_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(C_SRC)))
CC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CC_SRC)))
CPP_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CPP_SRC)))
CPP_SNC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CPP_SRC_SNC)))
GENERATE_DEPS := generate_deps
endif


# House-keeping build targets.
all : echo_start_build $(GENERATE_DEPS) $(TESTS)

clean :
	@echo [CLEAN]
	@$(RM) -f $(TESTS) *.o $(COMMON_TEST_DIR)/*.o $(USER_DIR)/*.o $(USER_ETH)/*.o \
	*.asm $(COMMON_TEST_DIR)/*.asm $(USER_DIR)/*.asm $(USER_ETH)/*.asm \
	*.asm $(COMMON_TEST_DIR)/*.asm2 $(USER_DIR)/*.asm2 $(USER_ETH)/*.asm2 \
	*.optrpt $(COMMON_TEST_DIR)/*.optrpt $(USER_DIR)/*.optrpt $(USER_ETH)/*.optrpt;\
	$(RM) $(PROJECT_DEP_FILE)

.PHONY: xclean
xclean: clean

.PHONY : clear_dep
clear_dep:
	@$(RM) $(PROJECT_DEP_FILE)
	@echo [DEP] $(PROJECT_DEP_FILE)

$(C_DEPS) :
	@$(CC) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(patsubst %.c,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CFLAGS)  >> $(PROJECT_DEP_FILE)

$(CC_DEPS) :
	@$(CC) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(patsubst %.cc,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CPPFLAGS) $(CXXFLAGS) >> $(PROJECT_DEP_FILE)

$(CPP_DEPS) :
	@$(CPP) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(patsubst %.cpp,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP) >> $(PROJECT_DEP_FILE)

$(CPP_SNC_DEPS) :
	@$(CPP) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(patsubst %.cpp,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP_SNC) >> $(PROJECT_DEP_FILE)

.PHONY : generate_deps
generate_deps : clear_dep $(C_DEPS) $(CC_DEPS) $(CPP_DEPS) $(CPP_SNC_DEPS)

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))

include $(PROJECT_DEP_FILE)

endif

.PHONY : echo_start_build
echo_start_build :
	@echo Build Tests with
	@echo $(USER_DIR)
	@echo $(USER_API)

$(CC_OBJS) : $(CC_SRC)
	@echo "[CC] $@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o"$@" $(patsubst %.o,%.cc,$@)

$(CPP_ASMS) :
	@echo "[CPP->ASM] $@"
	@$(CXX) -S $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP) -fsource-asm -save-temps -o"$@" $(patsubst %.asm,%.cpp,$@)
	@cat $@ | grep -v '^..LN' |grep -v '	.loc' > $@2

$(CPP_OBJS) : $(CPP_ASMS)
	@echo "[CPP] $@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP) -o"$@" $(patsubst %.o,%.cpp,$@)

$(CPP_SNC_ASMS) :
	@echo "[CPP-SNC->ASM] $@"
	@$(CXX) -S $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP_SNC) -fsource-asm -save-temps -o"$@" $(patsubst %.asm,%.cpp,$@)
	@cat $@ | grep -v '^..LN' |grep -v '	.loc' > $@2

$(CPP_SNC_OBJS) : $(CPP_SNC_ASMS)
	@echo "[CPP-SNC] $@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP_SNC) -o"$@" $(patsubst %.o,%.cpp,$@)

$(C_OBJS) :
	@echo "[C] $@"
	@$(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$@)

$(TESTS) : $(CC_OBJS) $(CPP_OBJS) $(CPP_SNC_OBJS) $(C_OBJS) $(GTEST_ROOT)/libgtest.a
	@echo "[LD] $@"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -L$(MLOG_DIR)/bin -Wl, $(RTE_LIBS) -lpthread -lnuma $^ -o $@
