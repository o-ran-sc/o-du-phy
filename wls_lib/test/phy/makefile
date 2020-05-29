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

# Makefile to build Testphy application

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
WLSDIR            := $(DIR_WIRELESS_WLS)
BUILDDIR          := ../build/phy
SRCDIR            := $(CURDIR)

wls_phy_app_dep_file = $(BUILDDIR)/dep_file

APP := ../bin/phy/phy_app

INC := \
 $(WLSDIR) \
 $(SRCDIR) \
 $(RTE_SDK)/$(RTE_TARGET)/include \

INC := $(addprefix -I,$(INC))
 
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

RTE_LIBS := -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_pci -Wl,-lrte_net -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive
LDFLAGS := -g -Wl,-lrt -Wl,-lpthread -Wl,-lhugetlbfs -Wl,-lm -Wl,-lnuma -L $(WLSDIR) -lwls

LINUX_WLS_PHY_APP_SRC := \
	$(SRCDIR)/phy_main.c \

OBJS := $(LINUX_WLS_PHY_APP_SRC:.c=.o)

PROJECT_OBJ_DIR = $(BUILDDIR)

OBJS := $(addprefix $(PROJECT_OBJ_DIR)/,$(OBJS))

DIRLIST := $(sort $(dir $(OBJS)))

CC_DEPS := $(addprefix __dep__,$(LINUX_WLS_PHY_APP_SRC))

GEN_DEP :=
ifeq ($(wildcard $(wls_phy_app_dep_file)),)
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


ifneq ($(wildcard $(wls_phy_app_dep_file)),)
include $(wls_phy_app_dep_file)
endif

$(DIRLIST) :
	-@$(MD) $@

.PHONY : regenerate_dep
regenerate_dep : clean_dep echo_regeenrate_dep  $(CC_DEPS)

.PHONY: clean_dep
clean_dep:
	$(RM) $(wls_phy_app_dep_file)

.PHONY : echo_regeenrate_dep
echo_regeenrate_dep:
	@echo regenerating dep files


.PHONY : CC_DEPS
$(CC_DEPS):
	@$(CC) -MM $(subst __dep__,,$@) -MT $(addprefix $(PROJECT_OBJ_DIR)/,$(patsubst %.c,%.o,$(subst __dep__,,$@))) $(CFLAGS) >> $(wls_phy_app_dep_file)

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

