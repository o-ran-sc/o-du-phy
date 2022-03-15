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

.SUFFIXES: .o .c .s .i .cpp

# Makefile to build TestMac application

##############################################################
#  Tools configuration
##############################################################
CC := icc
CPP := icpc
AS := as
AR := ar
LD := icc
OBJDUMP := objdump

ifeq ($(SHELL),cmd.exe)
MD := mkdir.exe -p
RM := rm.exe -rf
else
MD := mkdir -p
RM := rm -rf
endif

##############################################################
# TARGET
##############################################################
ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

#ifeq ($(MESON_BUILD),0)
#RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include
#else
RTE_INC := $(shell PKG_CONFIG_PATH=$(RTE_SDK)/build/meson-uninstalled pkg-config --cflags-only-I libdpdk)
#endif

##############################################################
# Projects folders
##############################################################
ORANDIR        	  := $(DIR_WIRELESS_ORAN)
WLSDIR            := $(DIR_WIRELESS_WLS)
BUILDDIR          := ../build/mac
SRCDIR            := $(CURDIR)

wls_mac_app_dep_file = $(BUILDDIR)/dep_file

APP := ../bin/mac/mac_app

INC := \
 -I$(WLSDIR) \
 -I$(SRCDIR) \
 -I$(ORANDIR)/include \
 -I$(ORANDIR) \
 -I$(RTE_INC) \
 #$(FLEXRANDIR)/source/nr5g/api \
 #$(FLEXRANDIR)/source/common \

DEFS := USE_WO_LOCK _GNU_SOURCE NR5G 

ifneq ($(PRINTDBG),)
DEFS := $(DEFS) PRINTF_DBG_OK
endif

ifeq ($(DEBUG_MODE),true)
DEFS := $(DEFS) DEBUG_MODE
endif

DEFS := $(addprefix -D,$(DEFS))

CFLAGS := -g -Wall -wd9 -Wno-deprecated-declarations -Wimplicit-function-declaration -fasm-blocks $(DEFS) $(INC)

ifeq ($(PRINTDBG),)
CFLAGS := $(CFLAGS) -Werror
endif

#RTE_LIBS := -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_pci -Wl,-lrte_net -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
#ifeq ($(MESON_BUILD),0)
#RTE_LIBS := -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_pci -Wl,-lrte_net -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl, -lrte_cryptodev -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
#else
RTE_LIBS := -Wl,--whole-archive -Wl,/usr/lib64/libnuma.so -Wl,--as-needed -pthread -L$(RTE_SDK)/build/drivers -L$(RTE_SDK)/build/lib -l:librte_common_cpt.a -l:librte_common_dpaax.a -l:librte_common_iavf.a -l:librte_common_octeontx.a -l:librte_common_octeontx2.a -l:librte_common_sfc_efx.a -l:librte_bus_dpaa.a -l:librte_bus_fslmc.a -l:librte_bus_ifpga.a -l:librte_bus_pci.a -l:librte_bus_vdev.a -l:librte_bus_vmbus.a -l:librte_mempool_bucket.a -l:librte_mempool_dpaa.a -l:librte_mempool_dpaa2.a -l:librte_mempool_octeontx.a -l:librte_mempool_octeontx2.a -l:librte_mempool_ring.a -l:librte_mempool_stack.a -l:librte_net_af_packet.a -l:librte_net_ark.a -l:librte_net_atlantic.a -l:librte_net_avp.a -l:librte_net_axgbe.a -l:librte_net_bond.a -l:librte_net_bnx2x.a -l:librte_net_bnxt.a -l:librte_net_cxgbe.a -l:librte_net_dpaa.a -l:librte_net_dpaa2.a -l:librte_net_e1000.a -l:librte_net_ena.a -l:librte_net_enetc.a -l:librte_net_enic.a -l:librte_net_failsafe.a -l:librte_net_fm10k.a -l:librte_net_i40e.a -l:librte_net_hinic.a -l:librte_net_hns3.a -l:librte_net_iavf.a -l:librte_net_ice.a -l:librte_net_igc.a -l:librte_net_ixgbe.a -l:librte_net_kni.a -l:librte_net_liquidio.a -l:librte_net_memif.a -l:librte_net_netvsc.a -l:librte_net_nfp.a -l:librte_net_null.a -l:librte_net_octeontx.a -l:librte_net_octeontx2.a -l:librte_net_pfe.a -l:librte_net_qede.a -l:librte_net_ring.a -l:librte_net_sfc.a -l:librte_net_tap.a -l:librte_net_thunderx.a -l:librte_net_txgbe.a -l:librte_net_vdev_netvsc.a -l:librte_net_vhost.a -l:librte_net_virtio.a -l:librte_net_vmxnet3.a -l:librte_raw_dpaa2_cmdif.a -l:librte_raw_dpaa2_qdma.a -l:librte_raw_ioat.a -l:librte_raw_ntb.a -l:librte_raw_octeontx2_dma.a -l:librte_raw_octeontx2_ep.a -l:librte_raw_skeleton.a -l:librte_crypto_bcmfs.a -l:librte_crypto_caam_jr.a -l:librte_crypto_dpaa_sec.a -l:librte_crypto_dpaa2_sec.a -l:librte_crypto_nitrox.a -l:librte_crypto_null.a -l:librte_crypto_octeontx.a -l:librte_crypto_octeontx2.a -l:librte_crypto_scheduler.a -l:librte_crypto_virtio.a -l:librte_compress_octeontx.a -l:librte_compress_zlib.a -l:librte_regex_octeontx2.a -l:librte_vdpa_ifc.a -l:librte_event_dlb.a -l:librte_event_dlb2.a -l:librte_event_dpaa.a -l:librte_event_dpaa2.a -l:librte_event_octeontx2.a -l:librte_event_opdl.a -l:librte_event_skeleton.a -l:librte_event_sw.a -l:librte_event_dsw.a -l:librte_event_octeontx.a -l:librte_node.a -l:librte_graph.a -l:librte_bpf.a -l:librte_flow_classify.a -l:librte_pipeline.a -l:librte_table.a -l:librte_fib.a -l:librte_ipsec.a -l:librte_vhost.a -l:librte_stack.a -l:librte_security.a -l:librte_sched.a -l:librte_reorder.a -l:librte_rib.a -l:librte_regexdev.a -l:librte_rawdev.a -l:librte_pdump.a -l:librte_power.a -l:librte_member.a -l:librte_lpm.a -l:librte_latencystats.a -l:librte_kni.a -l:librte_jobstats.a -l:librte_ip_frag.a -l:librte_gso.a -l:librte_gro.a -l:librte_eventdev.a -l:librte_efd.a -l:librte_distributor.a -l:librte_cryptodev.a -l:librte_compressdev.a -l:librte_cfgfile.a -l:librte_bitratestats.a -l:librte_bbdev.a -l:librte_acl.a -l:librte_timer.a -l:librte_hash.a -l:librte_metrics.a -l:librte_cmdline.a -l:librte_pci.a -l:librte_ethdev.a -l:librte_meter.a -l:librte_net.a -l:librte_mbuf.a -l:librte_mempool.a -l:librte_rcu.a -l:librte_ring.a -l:librte_eal.a -l:librte_telemetry.a -l:librte_kvargs.a -lelf -lrte_node -lrte_graph -lrte_bpf -lrte_flow_classify -lrte_pipeline -lrte_table -lrte_fib -lrte_ipsec -lrte_vhost -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib -lrte_regexdev -lrte_rawdev -lrte_pdump -lrte_power -lrte_member -lrte_lpm -lrte_latencystats -lrte_kni -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev -lrte_acl -lrte_timer -lrte_hash -lrte_metrics -lrte_cmdline -lrte_pci -lrte_ethdev -lrte_meter -lrte_net -lrte_mbuf -lrte_mempool -lrte_rcu -lrte_ring -lrte_eal -lrte_telemetry -lrte_kvargs -lm -ldl -lnuma -lz -Wl,--no-whole-archive
#endif
LDFLAGS := -g  -Wl,-lrt -Wl,-lpthread -Wl,-lhugetlbfs -Wl,-lm -Wl,-lnuma -L $(WLSDIR) -lwls

LINUX_WLS_MAC_APP_SRC := \
	$(SRCDIR)/mac_main.c \
	$(SRCDIR)/mac_wls.c \

OBJS := $(LINUX_WLS_MAC_APP_SRC:.c=.o)

PROJECT_OBJ_DIR = $(BUILDDIR)

OBJS := $(addprefix $(PROJECT_OBJ_DIR)/,$(OBJS))

DIRLIST := $(sort $(dir $(OBJS)))

CC_DEPS := $(addprefix __dep__,$(LINUX_WLS_MAC_APP_SRC))

GEN_DEP :=
ifeq ($(wildcard $(wls_mac_app_dep_file)),)
GEN_DEP := regenerate_dep
endif

.PHONY: $(APP)
$(APP): $(DIRLIST) echo_options $(GEN_DEP) $(OBJS)
	@echo [LD] $(APP)
	@$(CC) -o $(APP) $(OBJS) $(LDFLAGS) $(RTE_LIBS)
#	$(OBJDUMP) -d $(APP) > $(APP).asm

.PHONY : echo_options
echo_options:
	@echo [CFLAGS]	$(CFLAGS)
	@echo [LDFAGS]	$(LDFLAGS)
	@echo [RTE_LIBS] $(RTE_LIBS)
	@echo [ORANDIR] $(ORANDIR)
ifneq ($(wildcard $(wls_mac_app_dep_file)),)
include $(wls_mac_app_dep_file)
endif

$(DIRLIST) :
	-@$(MD) $@

.PHONY : regenerate_dep
regenerate_dep : clean_dep echo_regeenrate_dep  $(CC_DEPS)

.PHONY: clean_dep
clean_dep:
	$(RM) $(wls_mac_app_dep_file)

.PHONY : echo_regeenrate_dep
echo_regeenrate_dep:
	@echo regenerating dep files


.PHONY : CC_DEPS
$(CC_DEPS):
	@$(CC) -MM $(subst __dep__,,$@) -MT $(addprefix $(PROJECT_OBJ_DIR)/,$(patsubst %.c,%.o,$(subst __dep__,,$@))) $(CFLAGS) >> $(wls_mac_app_dep_file)

$(OBJS) : $(PROJECT_OBJ_DIR)/%.o: %.c
	@echo [CC]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@echo "";echo $@
	@echo "";echo $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@echo "";echo $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))
	@echo ""
	$(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))


.PHONY: xclean
xclean : clean_dep
	@$(RM) $(OBJS)
	@$(RM) $(APP)
	@$(RM) $(BUILDDIR)

.PHONY: clean
clean :
	@$(RM) $(OBJS)
	@$(RM) $(APP)

