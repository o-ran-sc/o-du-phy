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
CC  := gcc
CXX := g++
CPP := g++
AS := as
AR := ar
LD := gcc
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

GTEST_ROOT=/opt/gtest/gtest-1.7.0
RTE_TARGET ?= x86_64-native-linuxapp-icc

ifeq ($(MESON_BUILD),)

#RTE_LIBS = -L/usr/local/lib/aarch64-linux-gnu -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -L$ENV{RTE_SDK}/$ENV{RTE_TARGET}/lib -Wl,--allow-multiple-definition -Wl,-Bstatic -lrte_kvargs -lrte_net -lrte_ethdev -lrte_timer -lrte_pci -lrte_eal -lrte_cmdline -lrte_bus_vdev -lrte_rcu -lrte_telemetry -lrte_common_mlx5 -lrte_pmd_mlx5 -lpcap -Wl,-Bdynamic -Wl,--no-whole-archive -lmlx5 -libverbs -lrte_mbuf -lrte_ring -lrte_mempool -lrte_mempool_ring -lrte_bus_pci -ldl -lbsd -ldbus-1 -lrte_node -lrte_graph -lrte_bpf -lrte_flow_classify -lrte_pipeline -lrte_table -lrte_port -lrte_fib -lrte_ipsec -lrte_vhost -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib -lrte_rawdev -lrte_pdump -lrte_power -lrte_member -lrte_lpm -lrte_latencystats -lrte_kni -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev -lrte_acl -lrte_hash -lrte_metrics -lrte_meter

RTE_LIBS = -L/usr/local/lib/aarch64-linux-gnu -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -L$ENV{RTE_SDK}/$ENV{RTE_TARGET}/lib -Wl,--allow-multiple-definition -Wl,-Bstatic -lrte_kvargs -lrte_net -lrte_ethdev -lrte_timer -lrte_pci -lrte_eal -lrte_cmdline -lrte_bus_vdev -lrte_rcu -lrte_telemetry -lrte_common_mlx5 -lrte_pmd_mlx5 -lpcap -Wl,-Bdynamic -Wl,--no-whole-archive -lmlx5 -libverbs -lrte_mbuf -lrte_ring -lrte_mempool -lrte_mempool_ring -lrte_bus_pci -ldl -lbsd -ldbus-1

#RTE_LIBS = -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,-lrte_flow_classify -Wl,--whole-archive -Wl,-lrte_pipeline -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_table -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_port -Wl,--no-whole-archive -Wl,-lrte_pdump -Wl,-lrte_distributor -Wl,-lrte_ip_frag -Wl,-lrte_meter -Wl,-lrte_lpm -Wl,--whole-archive -Wl,-lrte_acl -Wl,--no-whole-archive -Wl,-lrte_jobstats -Wl,-lrte_metrics -Wl,-lrte_bitratestats -Wl,-lrte_latencystats -Wl,-lrte_power -Wl,-lrte_efd -Wl,-lrte_bpf -Wl,--whole-archive -Wl,-lrte_cfgfile -Wl,-lrte_gro -Wl,-lrte_gso -Wl,-lrte_hash -Wl,-lrte_member -Wl,-lrte_vhost -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_net -Wl,-lrte_ethdev -Wl,-lrte_bbdev -Wl,-lrte_cryptodev -Wl,-lrte_security -Wl,-lrte_compressdev -Wl,-lrte_eventdev -Wl,-lrte_rawdev -Wl,-lrte_timer -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_pci -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_reorder -Wl,-lrte_sched -Wl,-lrte_kni -Wl,-lrte_common_octeontx -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_bus_dpaa -Wl,-lrte_common_dpaax -Wl,-lrte_stack -Wl,-lrte_bus_fslmc -Wl,-lrte_mempool_bucket -Wl,-lrte_mempool_stack -Wl,-lrte_mempool_dpaa -Wl,-lrte_mempool_dpaa2 -Wl,-lrte_pmd_af_packet -Wl,-lrte_pmd_ark -Wl,-lrte_pmd_iavf -Wl,-lrte_pmd_avp -Wl,-lrte_pmd_axgbe -Wl,-lrte_pmd_bnxt -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_cxgbe -Wl,-lrte_pmd_dpaa -Wl,-lrte_pmd_dpaa2 -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ena -Wl,-lrte_pmd_enic -Wl,-lrte_pmd_fm10k -Wl,-lrte_pmd_failsafe -Wl,-lrte_pmd_kni -Wl,-lrte_pmd_nfp -Wl,-lrte_pmd_null -Wl,-lrte_pmd_qede -Wl,-lrte_pmd_ring  -Wl,-lrte_pmd_tap  -Wl,-lrte_pmd_vdev_netvsc -Wl,-lrte_pmd_virtio -Wl,-lrte_pmd_vhost -Wl,-lrte_pmd_ifc -Wl,-lrte_bus_vmbus -Wl,-lrte_pmd_netvsc -Wl,-lrte_pmd_bbdev_null -Wl,-lrte_pmd_null_crypto -Wl,-lrte_pmd_crypto_scheduler -Wl,-lrte_pmd_dpaa2_sec -Wl,-lrte_pmd_dpaa_sec -Wl,-lrte_pmd_virtio_crypto -Wl,-lrte_pmd_qat -Wl,-lrte_pmd_skeleton_event -Wl,-lrte_pmd_sw_event -Wl,-lrte_pmd_dpaa_event -Wl,-lrte_pmd_dpaa2_event -Wl,-lrte_mempool_octeontx -Wl,-lrte_pmd_octeontx -Wl,-lrte_pmd_opdl_event -Wl,-lrte_rawdev_skeleton -Wl,-lrte_rawdev_dpaa2_cmdif -Wl,-lrte_rawdev_dpaa2_qdma -Wl,-lrte_bus_ifpga -Wl,--no-whole-archive -Wl,-lrt -Wl,-lm -Wl,-lnuma -Wl,-ldl -Wl,
RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include
else
#RTE_LIBS = $(shell PKG_CONFIG_PATH=$(RTE_SDK)/build/meson-uninstalled pkg-config --static --libs libdpdk)
RTE_LIBS =-Wl,--whole-archive -Wl,--as-needed -Wl,--no-whole-archive -pthread -L$(RTE_SDK)/$(RTE_TARGET)/drivers -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -L/usr//usr/lib64 -l:librte_common_cpt.a -l:librte_common_dpaax.a -l:librte_common_iavf.a -l:librte_common_octeontx.a -l:librte_common_octeontx2.a -l:librte_common_sfc_efx.a -l:librte_bus_dpaa.a -l:librte_bus_fslmc.a -l:librte_bus_ifpga.a -l:librte_bus_pci.a -l:librte_bus_vdev.a -l:librte_bus_vmbus.a -l:librte_common_mlx5.a -l:librte_common_qat.a -l:librte_mempool_bucket.a -l:librte_mempool_dpaa.a -l:librte_mempool_dpaa2.a -l:librte_mempool_octeontx.a -l:librte_mempool_octeontx2.a -l:librte_mempool_ring.a -l:librte_mempool_stack.a -l:librte_net_af_packet.a -l:librte_net_ark.a -l:librte_net_atlantic.a -l:librte_net_avp.a -l:librte_net_axgbe.a -l:librte_net_bond.a -l:librte_net_bnx2x.a -l:librte_net_bnxt.a -l:librte_net_cxgbe.a -l:librte_net_dpaa.a -l:librte_net_dpaa2.a -l:librte_net_e1000.a -l:librte_net_ena.a -l:librte_net_enetc.a -l:librte_net_enic.a -l:librte_net_failsafe.a -l:librte_net_fm10k.a -l:librte_net_i40e.a -l:librte_net_hinic.a -l:librte_net_hns3.a -l:librte_net_iavf.a -l:librte_net_ice.a -l:librte_net_igc.a -l:librte_net_ixgbe.a -l:librte_net_kni.a -l:librte_net_liquidio.a -l:librte_net_memif.a -l:librte_net_mlx4.a -l:librte_net_mlx5.a -l:librte_net_netvsc.a -l:librte_net_nfp.a -l:librte_net_null.a -l:librte_net_octeontx.a -l:librte_net_octeontx2.a -l:librte_net_pfe.a -l:librte_net_qede.a -l:librte_net_ring.a -l:librte_net_sfc.a -l:librte_net_softnic.a -l:librte_net_tap.a -l:librte_net_thunderx.a -l:librte_net_vdev_netvsc.a -l:librte_net_vhost.a -l:librte_net_virtio.a -l:librte_net_vmxnet3.a -l:librte_raw_dpaa2_cmdif.a -l:librte_raw_dpaa2_qdma.a -l:librte_raw_ioat.a -l:librte_raw_ntb.a -l:librte_raw_octeontx2_dma.a -l:librte_raw_octeontx2_ep.a -l:librte_raw_skeleton.a -l:librte_crypto_bcmfs.a -l:librte_crypto_caam_jr.a -l:librte_crypto_ccp.a -l:librte_crypto_dpaa_sec.a -l:librte_crypto_dpaa2_sec.a -l:librte_crypto_nitrox.a -l:librte_crypto_null.a -l:librte_crypto_octeontx.a -l:librte_crypto_octeontx2.a -l:librte_crypto_openssl.a -l:librte_crypto_scheduler.a -l:librte_crypto_virtio.a -l:librte_compress_octeontx.a -l:librte_compress_zlib.a -l:librte_regex_mlx5.a -l:librte_regex_octeontx2_regex.a -l:librte_vdpa_ifc.a -l:librte_vdpa_mlx5.a -l:librte_event_dpaa.a -l:librte_event_dpaa2.a -l:librte_event_octeontx2.a -l:librte_event_opdl.a -l:librte_event_skeleton.a -l:librte_event_sw.a -l:librte_event_dsw.a -l:librte_event_octeontx.a -l:librte_baseband_null.a -l:librte_baseband_turbo_sw.a -l:librte_baseband_fpga_lte_fec.a -l:librte_baseband_fpga_5gnr_fec.a -l:librte_baseband_acc100.a -l:librte_node.a -l:librte_graph.a -l:librte_bpf.a -l:librte_flow_classify.a -l:librte_pipeline.a -l:librte_table.a -l:librte_port.a -l:librte_fib.a -l:librte_ipsec.a -l:librte_vhost.a -l:librte_stack.a -l:librte_security.a -l:librte_sched.a -l:librte_reorder.a -l:librte_rib.a -l:librte_regexdev.a -l:librte_rawdev.a -l:librte_pdump.a -l:librte_power.a -l:librte_member.a -l:librte_lpm.a -l:librte_latencystats.a -l:librte_kni.a -l:librte_jobstats.a -l:librte_ip_frag.a -l:librte_gso.a -l:librte_gro.a -l:librte_eventdev.a -l:librte_efd.a -l:librte_distributor.a -l:librte_cryptodev.a -l:librte_compressdev.a -l:librte_cfgfile.a -l:librte_bitratestats.a -l:librte_bbdev.a -l:librte_acl.a -l:librte_timer.a -l:librte_hash.a -l:librte_metrics.a -l:librte_cmdline.a -l:librte_pci.a -l:librte_ethdev.a -l:librte_meter.a -l:librte_net.a -l:librte_mbuf.a -l:librte_mempool.a -l:librte_rcu.a -l:librte_ring.a -l:librte_eal.a -l:librte_telemetry.a -l:librte_kvargs.a -lmlx5 -lcrypto -lmlx4 -lelf -lrte_node -lrte_graph -lrte_bpf -lrte_flow_classify -lrte_pipeline -lrte_table -lrte_port -lrte_fib -lrte_ipsec -lrte_vhost -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib -lrte_regexdev -lrte_rawdev -lrte_pdump -lrte_power -lrte_member -lrte_lpm -lrte_latencystats -lrte_kni -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev -lrte_acl -lrte_timer -lrte_hash -lrte_metrics -lrte_cmdline -lrte_pci -lrte_ethdev -lrte_meter -lrte_net -lrte_mbuf -lrte_mempool -lrte_rcu -lrte_ring -lrte_eal -lrte_telemetry -lrte_kvargs -Wl,--no-whole-archive -lm -ldl -lnuma -libverbs -lpthread -lz
RTE_INC := $(shell PKG_CONFIG_PATH=$(RTE_SDK)/build/meson-uninstalled pkg-config --cflags-only-I libdpdk)
endif


# Where to find user code.
COMMON_TEST_DIR = $(XRAN_DIR)/test/common
XRAN_TEST_DIR = $(XRAN_DIR)/test/test_xran
USER_DIR = $(XRAN_DIR)/lib/src
USER_ETH = $(XRAN_DIR)/lib/ethernet
USER_API = $(XRAN_DIR)/lib/api

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
CPPFLAGS += -isystem $(GTEST_ROOT)/include

# Flags passed to the C++ compiler.
CXXFLAGS += -g -std=c++14 -Wall -Wextra -pthread -I$(USER_API) -I$(USER_DIR) -I$(USER_ETH) -I$(MLOG_DIR)/source -I$(COMMON_TEST_DIR) -I$(RTE_INC)
ARM_FLAGS = -march=armv8.2-a+crc+crypto -mcpu=neoverse-n1 -mtune=native -DRTE_ARCH_64 -DRTE_ARCH_ARM64 -DRTE_MACHINE_CPUFLAG_NEON  -DRTE_MACHINE_CPUFLAG_CRC32 -DTEST_XRAN
CXXFLAGS += $(ARM_FLAGS) -mtls-dialect=trad

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
	$(XRAN_TEST_DIR)/c_plane_tests.cc 
#	chain_tests.cc \
#	prach_functional.cc \
#	prach_performance.cc \
#	u_plane_functional.cc \
#	init_sys_functional.cc \
#	compander_functional.cc \
#	unittests.cc

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

CPP_COMP = -O1 -g 

ifeq ($(GEN_ASM), 1)
CPP_ASMS := $(patsubst %.cpp,%.asm,$(CPP_SRC))
CPP_SNC_ASMS := $(patsubst %.cpp,%.asm,$(CPP_SRC_SNC))
CPP_COMP += -qopt-report=5 -qopt-matmul -qopt-report-phase=all
endif

PROJECT_DEP_FILE := $(TESTS).dep

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))
GENERATE_DEPS :=
else
C_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(C_SRC)))
CC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CC_SRC)))
CPP_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CPP_SRC)))
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


.PHONY : generate_deps
generate_deps : clear_dep $(C_DEPS) $(CC_DEPS) $(CPP_DEPS) 

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



$(C_OBJS) :
	@echo "[C] $@"
	@echo $(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$@)
	@$(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$@)

$(TESTS) : $(CC_OBJS) $(CPP_OBJS) $(C_OBJS) $(GTEST_ROOT)/libgtest.a
	@echo "[LD] $@"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -L$(MLOG_DIR)/bin  $(RTE_LIBS) -lpthread -lnuma $^ -o $@
