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
ifeq ($(RTE_TARGET),)
    RTE_TARGET :=x86_64-native-linuxapp-icc
endif
##############################################################
# DPDK
##############################################################
ifeq ($(RTE_SDK),)
$(info Please make sure RTE_SDK points to DPDK folder (current version of DPDK is 18.08))
    RTE_SDK := /opt/dpdk-18.08
endif

##############################################################
# Projects folders
##############################################################
FLEXRANDIR        := $(DIR_WIRELESS)
WLSDIR            := $(DIR_WIRELESS_WLS)
BUILDDIR          := ../build/make
SRCDIR            := $(CURDIR)

oran_5g_fapi_dep_file = $(BUILDDIR)/oran_5g_fapi_dep

APP := ../bin/oran_5g_fapi

INC := \
 $(WLSDIR) \
 $(SRCDIR)/../include \
 $(SRCDIR)/common \
 $(SRCDIR)/include \
 $(SRCDIR)/framework/workers \
 $(SRCDIR)/framework/wls/fapi2mac \
 $(SRCDIR)/framework/wls/fapi2phy \
 $(SRCDIR)/framework/wls/lib \
 $(SRCDIR)/api/fapi2mac \
 $(SRCDIR)/api/fapi2phy \
 $(FLEXRANDIR)/source/nr5g/api \
 $(FLEXRANDIR)/source/common \
 $(RTE_SDK)/$(RTE_TARGET)/include \
 $(SRCDIR)/api/fapi2phy/p5 \
 $(SRCDIR)/api/fapi2phy/p7 \
 $(SRCDIR)/api/fapi2mac/p5 \
 $(SRCDIR)/api/fapi2mac/p7 \
 $(SRCDIR)/utils \

INC := $(addprefix -I,$(INC))
 
DEFS := USE_WO_LOCK _GNU_SOURCE

ifneq ($(PRINTDBG),)
DEFS := $(DEFS) PRINTF_DBG_OK
endif

ifeq ($(DEBUG_MODE),true)
DEFS := $(DEFS) DEBUG_MODE
endif

ifeq ($(STATISTIC_MODE),true)
DEFS := $(DEFS) STATISTIC_MODE
endif

DEFS := $(addprefix -D,$(DEFS))

CFLAGS := -g -Wall -Wextra -Wunused -wd9 -Wno-deprecated-declarations -Wimplicit-function-declaration -fasm-blocks -fstack-protector-strong -z,now, -z,relro -z noexecstack -Wformat -Wformat-security -Werror=format-security -fno-strict-overflow -fwrapv $(DEFS) $(INC)

ifeq ($(PRINTDBG),)
CFLAGS := $(CFLAGS) -Werror
endif

#RTE_LIBS := -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_pci -Wl,-lrte_net -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
RTE_LIBS := -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_pci -Wl,-lrte_net -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl, -lrte_cryptodev -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
LDFLAGS := -g -Wl,-lrt -Wl,-lpthread -Wl,-lhugetlbfs -Wl,-lm -Wl,-lnuma -L $(WLSDIR) -lwls

LINUX_ORAN_5G_FAPI_SRC := \
	$(SRCDIR)/nr5g_fapi.c \
	$(SRCDIR)/utils/nr5g_fapi_args.c \
	$(SRCDIR)/utils/nr5g_fapi_config_loader.c \
	$(SRCDIR)/utils/nr5g_fapi_log.c \
	$(SRCDIR)/utils/nr5g_fapi_stats.c \
	$(SRCDIR)/utils/nr5g_fapi_memory.c \
	$(SRCDIR)/utils/nr5g_fapi_cmd.c \
	$(SRCDIR)/framework/workers/nr5g_fapi_mac2phy_thread.c \
	$(SRCDIR)/framework/workers/nr5g_fapi_phy2mac_thread.c \
	$(SRCDIR)/framework/nr5g_fapi_framework.c \
	$(SRCDIR)/framework/wls/fapi2mac/nr5g_fapi_fapi2mac_wls.c \
	$(SRCDIR)/framework/wls/fapi2phy/nr5g_fapi_fapi2phy_wls.c \
	$(SRCDIR)/framework/wls/lib/nr5g_fapi_wls.c \
	$(SRCDIR)/api/fapi2mac/nr5g_fapi_fapi2mac_api.c \
	$(SRCDIR)/api/fapi2mac/nr5g_fapi_proc_error_ind.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_config_resp.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_start_resp.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_stop_ind.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_ul_iq_samples_resp.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_dl_iq_samples_resp.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_shutdown_resp.c \
	$(SRCDIR)/api/fapi2mac/p5/nr5g_fapi_proc_fapi_msg_header.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_slot_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_crc_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_rach_ind.c \
    $(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_rx_data_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_srs_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_uci_ind.c \
	$(SRCDIR)/api/fapi2phy/nr5g_fapi_fapi2phy_api.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_config_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_start_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_stop_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_shutdown_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_dl_iq_samples_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_ul_iq_samples_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_dl_tti_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_ul_tti_req.c \
    $(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_tx_data_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_ul_dci_req.c

OBJS := $(LINUX_ORAN_5G_FAPI_SRC:.c=.o)

PROJECT_OBJ_DIR = $(BUILDDIR)

OBJS := $(addprefix $(PROJECT_OBJ_DIR)/,$(OBJS))

DIRLIST := $(sort $(dir $(OBJS)))

CC_DEPS := $(addprefix __dep__,$(LINUX_ORAN_5G_FAPI_SRC))

GEN_DEP :=
ifeq ($(wildcard $(oran_5g_fapi_dep_file)),)
GEN_DEP := regenerate_dep
endif

.PHONY: $(APP)
$(APP): $(DIRLIST) echo_options $(GEN_DEP) $(OBJS)
	@echo [LD] $(APP)
	@$(CC) -o $(APP) $(OBJS) $(RTE_LIBS) $(LDFLAGS)
#	$(OBJDUMP) -d $(APP) > $(APP).asm

.PHONY : echo_options
echo_options:
	@echo [CFLAGS]	$(CFLAGS)
	@echo [LDFAGS]	$(LDFLAGS)


ifneq ($(wildcard $(oran_5g_fapi_dep_file)),)
include $(oran_5g_fapi_dep_file)
endif

$(DIRLIST) :
	-@$(MD) $@

.PHONY : regenerate_dep
regenerate_dep : clean_dep echo_regeenrate_dep  $(CC_DEPS)

.PHONY: clean_dep
clean_dep:
	$(RM) $(oran_5g_fapi_dep_file)

.PHONY : echo_regeenrate_dep
echo_regeenrate_dep:
	@echo regenerating dep files


.PHONY : CC_DEPS
$(CC_DEPS):
	@$(CC) -MM $(subst __dep__,,$@) -MT $(addprefix $(PROJECT_OBJ_DIR)/,$(patsubst %.c,%.o,$(subst __dep__,,$@))) $(CFLAGS) >> $(oran_5g_fapi_dep_file)

$(OBJS) : $(PROJECT_OBJ_DIR)/%.o: %.c
	@echo [CC]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(CC) -c $(CFLAGS) -o"$@" $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))


.PHONY: xclean
xclean : clean_dep
	@$(RM) $(OBJS)
	@$(RM) $(APP)
	@$(RM) $(BUILDDIR)

.PHONY: clean
clean :
	@$(RM) $(OBJS)
	@$(RM) $(APP)

