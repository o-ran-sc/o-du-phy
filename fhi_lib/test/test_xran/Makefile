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

RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include

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
CXXFLAGS += -g -std=gnu++11 -Wall -Wextra -pthread -I$(USER_API) -I$(USER_DIR) -I$(USER_ETH) -I$(MLOG_DIR)/source -I $(COMMON_TEST_DIR) -I$(RTE_INC)

# All tests produced by this Makefile.  Remember to add new tests you
# created to the list.
TESTS = unittests

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_ROOT)/include/gtest/*.h \
                $(GTEST_ROOT)/include/gtest/internal/*.h

#RTE_LIBS = -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_mempool_ring -Wl,-lrte_pci -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_net -Wl,-lrte_distributor -Wl,-lrte_reorder -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_jobstats -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,-lrte_vhost -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_cryptodev -Wl,-lrte_mempool -Wl,-lrte_ring -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_virtio -Wl,-lrte_pmd_cxgbe -Wl,-lrte_pmd_enic -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_fm10k -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl,-lrte_pmd_af_packet -Wl,-lrte_pmd_null -Wl,-lrte_pdump  -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
RTE_LIBS = -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,-lrte_flow_classify -Wl,--whole-archive -Wl,-lrte_pipeline -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_table -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_port -Wl,--no-whole-archive -Wl,-lrte_pdump -Wl,-lrte_distributor -Wl,-lrte_ip_frag -Wl,-lrte_meter -Wl,-lrte_lpm -Wl,--whole-archive -Wl,-lrte_acl -Wl,--no-whole-archive -Wl,-lrte_jobstats -Wl,-lrte_metrics -Wl,-lrte_bitratestats -Wl,-lrte_latencystats -Wl,-lrte_power -Wl,-lrte_efd -Wl,-lrte_bpf -Wl,--whole-archive -Wl,-lrte_cfgfile -Wl,-lrte_gro -Wl,-lrte_gso -Wl,-lrte_hash -Wl,-lrte_member -Wl,-lrte_vhost -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_net -Wl,-lrte_ethdev -Wl,-lrte_bbdev -Wl,-lrte_cryptodev -Wl,-lrte_security -Wl,-lrte_compressdev -Wl,-lrte_eventdev -Wl,-lrte_rawdev -Wl,-lrte_timer -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_pci -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_reorder -Wl,-lrte_sched -Wl,-lrte_kni -Wl,-lrte_common_octeontx -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_bus_dpaa -Wl,-lrte_common_dpaax -Wl,-lrte_stack -Wl,-lrte_bus_fslmc -Wl,-lrte_mempool_bucket -Wl,-lrte_mempool_stack -Wl,-lrte_mempool_dpaa -Wl,-lrte_mempool_dpaa2 -Wl,-lrte_pmd_af_packet -Wl,-lrte_pmd_ark -Wl,-lrte_pmd_iavf -Wl,-lrte_pmd_avp -Wl,-lrte_pmd_axgbe -Wl,-lrte_pmd_bnxt -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_cxgbe -Wl,-lrte_pmd_dpaa -Wl,-lrte_pmd_dpaa2 -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ena -Wl,-lrte_pmd_enic -Wl,-lrte_pmd_fm10k -Wl,-lrte_pmd_failsafe -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_kni -Wl,-lrte_pmd_lio -Wl,-lrte_pmd_nfp -Wl,-lrte_pmd_null -Wl,-lrte_pmd_qede -Wl,-lrte_pmd_ring -Wl,-lrte_pmd_softnic -Wl,-lrte_pmd_tap -Wl,-lrte_pmd_thunderx_nicvf -Wl,-lrte_pmd_vdev_netvsc -Wl,-lrte_pmd_virtio -Wl,-lrte_pmd_vhost -Wl,-lrte_pmd_ifc -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_bus_vmbus -Wl,-lrte_pmd_netvsc -Wl,-lrte_pmd_bbdev_null -Wl,-lrte_pmd_null_crypto -Wl,-lrte_pmd_crypto_scheduler -Wl,-lrte_pmd_dpaa2_sec -Wl,-lrte_pmd_dpaa_sec -Wl,-lrte_pmd_virtio_crypto -Wl,-lrte_pmd_octeontx_zip -Wl,-lrte_pmd_qat -Wl,-lrte_pmd_skeleton_event -Wl,-lrte_pmd_sw_event -Wl,-lrte_pmd_octeontx_ssovf -Wl,-lrte_pmd_dpaa_event -Wl,-lrte_pmd_dpaa2_event -Wl,-lrte_mempool_octeontx -Wl,-lrte_pmd_octeontx -Wl,-lrte_pmd_opdl_event -Wl,-lrte_rawdev_skeleton -Wl,-lrte_rawdev_dpaa2_cmdif -Wl,-lrte_rawdev_dpaa2_qdma -Wl,-lrte_bus_ifpga -Wl,--no-whole-archive -Wl,-lrt -Wl,-lm -Wl,-lnuma -Wl,-ldl -Wl,

CFLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations \
	-fdata-sections \
	-ffunction-sections \
	-g \
	-Wall \
	-Wimplicit-function-declaration  \
	-wd1786 \
	-I$(USER_DIR) -I$(USER_API) -I$(USER_API) -I$(USER_DIR) -I$(USER_ETH) -I$(MLOG_DIR)/source -I$(RTE_INC)

C_SRC = \
	$(USER_ETH)/ethdi.c    \
	$(USER_ETH)/ethernet.c	    \
	$(USER_DIR)/xran_up_api.c	\
	$(USER_DIR)/xran_sync_api.c	\
	$(USER_DIR)/xran_timer.c		\
	$(USER_DIR)/xran_cp_api.c 	\
	$(USER_DIR)/xran_transport.c 	\
	$(USER_DIR)/xran_common.c        \
	$(USER_DIR)/xran_ul_tables.c     \
	$(USER_DIR)/xran_frame_struct.c  \
	$(USER_DIR)/xran_app_frag.c \
	$(USER_DIR)/xran_main.c

#	$(USER_DIR)/xran_compression.c

CC_SRC = \
	$(COMMON_TEST_DIR)/xranlib_unit_test_main.cc \
	c_plane_tests.cc \
	chain_tests.cc \
	prach_functional.cc \
	prach_performance.cc \
	u_plane_functional.cc \
	init_sys_functional.cc \
	compander_functional.cc \
	unittests.cc

#	u_plane_performance.cc \

CPP_SRC = $(COMMON_TEST_DIR)/common.cpp \
	$(USER_DIR)/xran_compression.cpp \
	$(USER_DIR)/xran_bfp_ref.cpp \
	$(USER_DIR)/xran_bfp_cplane8.cpp \
	$(USER_DIR)/xran_bfp_cplane16.cpp \
	$(USER_DIR)/xran_bfp_cplane32.cpp \
	$(USER_DIR)/xran_bfp_cplane64.cpp

C_OBJS := $(patsubst %.c,%.o,$(C_SRC))
CC_OBJS := $(patsubst %.cc,%.o,$(CC_SRC))
CPP_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRC))

CPPFLAGS += -I$(USER_DIR) -I$(USER_API) 

CPP_COMP = -O3 -xcore-avx512 -restrict -g -fasm-blocks

# House-keeping build targets.
all : echo_start_build $(TESTS)

clean :
	@echo [CLEAN]
	rm -f $(TESTS) *.o $(COMMON_TEST_DIR)/*.o $(USER_DIR)/*.o $(USER_ETH)/*.o

.PHONY: xclean 
xclean: clean

.PHONY : echo_start_build
echo_start_build :
	@echo Build Tests with
	@echo $(USER_DIR)
	@echo $(USER_API)

$(CC_OBJS) :
	@echo "[CC] $@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o"$@" $(patsubst %.o,%.cc,$@)

$(CPP_OBJS) :
	@echo "[CPP] $@"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(CPP_COMP) -o"$@" $(patsubst %.o,%.cpp,$@)

$(C_OBJS) :
	@echo "[C] $@"
	@$(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$@)

unittests : $(CC_OBJS) $(CPP_OBJS) $(C_OBJS) $(GTEST_ROOT)/libgtest.a
	@echo "[LD] $@"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -L$(MLOG_DIR)/bin -Wl, $(RTE_LIBS) -lpthread -lnuma $^ -o $@
