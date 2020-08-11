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

MYCUSTOMTAB='     '
MYCUSTOMSPACE='============================================================================================'
MYCUSTOMSPACE1='------------------------------------------------------------'

##############################################################
#  Tools configuration
##############################################################
CC  := icc
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

PROJECT_NAME := sample-app
PROJECT_TYPE := elf
PROJECT_DIR  := $(XRAN_DIR)/app
BUILDDIR := ./build
PROJECT_BINARY := $(BUILDDIR)/$(PROJECT_NAME)

ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

RTE_TARGET ?= x86_64-native-linuxapp-gcc
RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include

API_DIR := $(XRAN_DIR)/lib/api
SRC_DIR := $(PROJECT_DIR)/src

ifeq ($(MLOG),1)
ifeq ($(MLOG_DIR),)
    MLOG_DIR=$(XRAN_DIR)/../mlog
endif
endif

CC_SRC = $(SRC_DIR)/common.c \
	$(SRC_DIR)/sample-app.c \
	$(SRC_DIR)/config.c

CC_FLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations  \
	-fdata-sections \
	-ffunction-sections \
	-g \
	-Wall \
	-Wimplicit-function-declaration \
	-g -O3 -wd1786

CPP_FLAGS := -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D_GNU_SOURCE -D_REENTRANT -pipe -no-prec-div \
                -no-prec-div -fp-model fast=2\
                -no-prec-sqrt  -falign-functions=16 -fast-transcendentals \
        -Werror -Wno-unused-variable -std=c++11 -mcmodel=large

INC :=  -I$(API_DIR) -I$(RTE_INC)
DEF :=

ifeq ($(MLOG),1)
	INC  += -I$(MLOG_DIR)/source
	DEF += -DMLOG_ENABLED
else
	DEF += -UMLOG_ENABLED
endif

XRAN_LIB_DIR=$(XRAN_DIR)/lib/build
LD_FLAGS += -L$(XRAN_LIB_DIR) -lxran

RTE_LIBS = -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,-lrte_flow_classify -Wl,--whole-archive -Wl,-lrte_pipeline -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_table -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_port -Wl,--no-whole-archive -Wl,-lrte_pdump -Wl,-lrte_distributor -Wl,-lrte_ip_frag -Wl,-lrte_meter -Wl,-lrte_lpm -Wl,--whole-archive -Wl,-lrte_acl -Wl,--no-whole-archive -Wl,-lrte_jobstats -Wl,-lrte_metrics -Wl,-lrte_bitratestats -Wl,-lrte_latencystats -Wl,-lrte_power -Wl,-lrte_efd -Wl,-lrte_bpf -Wl,--whole-archive -Wl,-lrte_cfgfile -Wl,-lrte_gro -Wl,-lrte_gso -Wl,-lrte_hash -Wl,-lrte_member -Wl,-lrte_vhost -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_net -Wl,-lrte_ethdev -Wl,-lrte_bbdev -Wl,-lrte_cryptodev -Wl,-lrte_security -Wl,-lrte_compressdev -Wl,-lrte_eventdev -Wl,-lrte_rawdev -Wl,-lrte_timer -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_pci -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_reorder -Wl,-lrte_sched -Wl,-lrte_kni -Wl,-lrte_common_octeontx -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_bus_dpaa -Wl,-lrte_common_dpaax -Wl,-lrte_stack -Wl,-lrte_bus_fslmc -Wl,-lrte_mempool_bucket -Wl,-lrte_mempool_stack -Wl,-lrte_mempool_dpaa -Wl,-lrte_mempool_dpaa2 -Wl,-lrte_pmd_af_packet -Wl,-lrte_pmd_ark -Wl,-lrte_pmd_iavf -Wl,-lrte_pmd_avp -Wl,-lrte_pmd_axgbe -Wl,-lrte_pmd_bnxt -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_cxgbe -Wl,-lrte_pmd_dpaa -Wl,-lrte_pmd_dpaa2 -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ena -Wl,-lrte_pmd_enic -Wl,-lrte_pmd_fm10k -Wl,-lrte_pmd_failsafe -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_kni -Wl,-lrte_pmd_lio -Wl,-lrte_pmd_nfp -Wl,-lrte_pmd_null -Wl,-lrte_pmd_qede -Wl,-lrte_pmd_ring -Wl,-lrte_pmd_softnic -Wl,-lrte_pmd_tap -Wl,-lrte_pmd_thunderx_nicvf -Wl,-lrte_pmd_vdev_netvsc -Wl,-lrte_pmd_virtio -Wl,-lrte_pmd_vhost -Wl,-lrte_pmd_ifc -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_bus_vmbus -Wl,-lrte_pmd_netvsc -Wl,-lrte_pmd_bbdev_null -Wl,-lrte_pmd_null_crypto -Wl,-lrte_pmd_crypto_scheduler -Wl,-lrte_pmd_dpaa2_sec -Wl,-lrte_pmd_dpaa_sec -Wl,-lrte_pmd_virtio_crypto -Wl,-lrte_pmd_octeontx_zip -Wl,-lrte_pmd_qat -Wl,-lrte_pmd_skeleton_event -Wl,-lrte_pmd_sw_event -Wl,-lrte_pmd_octeontx_ssovf -Wl,-lrte_pmd_dpaa_event -Wl,-lrte_pmd_dpaa2_event -Wl,-lrte_mempool_octeontx -Wl,-lrte_pmd_octeontx -Wl,-lrte_pmd_opdl_event -Wl,-lrte_rawdev_skeleton -Wl,-lrte_rawdev_dpaa2_cmdif -Wl,-lrte_rawdev_dpaa2_qdma -Wl,-lrte_bus_ifpga -Wl,--no-whole-archive -Wl,-lrt -Wl,-lm -Wl,-lnuma -Wl,-ldl -Wl,
LD_FLAGS += $(RTE_LIBS)

ifeq ($(MLOG),1)
	LD_FLAGS +=  -L$(MLOG_DIR)/bin -lmlog
endif

AS_FLAGS :=
AR_FLAGS := rc

PROJECT_OBJ_DIR := build/obj

CC_OBJS := $(patsubst %.c,%.o,$(CC_SRC))
CPP_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRC))
AS_OBJS := $(patsubst %.s,%.o,$(AS_SRC))
OBJS    := $(CC_OBJS) $(CPP_OBJS) $(AS_OBJS) $(LIBS)
DIRLIST := $(addprefix $(PROJECT_OBJ_DIR)/,$(sort $(dir $(OBJS))))

CC_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(CC_OBJS))
CPP_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(CPP_OBJS))

AS_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(AS_OBJS))

CC_FLAGS_FULL  := $(CC_FLAGS)  $(INC) $(DEF)
CPP_FLAGS_FULL := $(CPP_FLAGS) $(INC) $(DEF)

AS_FLAGS := $(AS_FLAGS) $(INC)

PROJECT_DEP_FILE := $(PROJECT_OBJ_DIR)/$(PROJECT_NAME).dep

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))
GENERATE_DEPS :=
else

CC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CC_SRC)))
CPP_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CPP_SRC)))
GENERATE_DEPS := generate_deps
endif

all : welcome_line 	$(PROJECT_BINARY)
	@echo $(PROJECT_BINARY)

.PHONY : clear_dep
clear_dep:
	@$(RM) $(PROJECT_DEP_FILE)
	@echo [DEP]   $(subst $(PROJECT_OBJ_DIR)/,,$(PROJECT_DEP_FILE))

$(CC_DEPS) :
	@$(CC) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(PROJECT_OBJ_DIR)/$(patsubst %.c,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CC_FLAGS_FULL) >> $(PROJECT_DEP_FILE)

$(CPP_DEPS) :
	@$(CPP) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(PROJECT_OBJ_DIR)/$(patsubst %.cpp,%.o,$(subst __up__,../,$(subst __dep__,,$@))) $(CPP_FLAGS_FULL) >> $(PROJECT_DEP_FILE)

.PHONY : generate_deps
generate_deps : clear_dep $(CC_DEPS) $(CPP_DEPS)


.PHONY : echo_start_build
echo_start_build :
	@echo [BUILD] $(PROJECT_TYPE) : $(PROJECT_NAME)

$(DIRLIST) :
	-@$(MD) $@

$(CC_OBJTARGETS) :
	@echo [CC]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(CC) -c $(CC_FLAGS_FULL) -o"$@" $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))

$(CPP_OBJTARGETS) :
	@echo [CPP]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(CPP) -c $(CPP_FLAGS_FULL) -o"$@" $(patsubst %.o,%.cpp,$(subst $(PROJECT_OBJ_DIR)/,,$@))

$(AS_OBJTARGETS) :
	@echo [AS]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(AS) $(AS_FLAGS) -o"$@" $(patsubst %.o,%.s,$(subst $(PROJECT_OBJ_DIR)/,,$@))

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))

include $(PROJECT_DEP_FILE)

endif

.PHONY: clean xclean
clean:
	@echo [CLEAN]  : $(PROJECT_NAME)
	@$(RM) $(CC_OBJTARGETS) $(CPP_OBJTARGETS) $(AS_OBJTARGETS)

xclean: clean
ifneq ($(wildcard $(PROJECT_DIR)/$(PROJECT_MAKE)),)
	@echo [XCLEAN] : $(PROJECT_NAME)
	@$(RM) $(PROJECT_BINARY) $(PROJECT_BINARY_LIB) $(PROJECT_DEP_FILE)
endif

.PHONY : welcome_line
welcome_line :
	@echo $(MYCUSTOMSPACE)
	@echo Building  $(PROJECT_BINARY)
	@echo $(MYCUSTOMTAB)RTE_TARGET           = $(RTE_TARGET)
	@echo $(MYCUSTOMSPACE)


.PHONY : debug release

debug :  all
release :  all

$(PROJECT_BINARY): $(DIRLIST) echo_start_build $(GENERATE_DEPS) $(PRE_BUILD) $(CC_OBJTARGETS) $(CPP_OBJTARGETS) $(AS_OBJTARGETS)
	@echo "[LD] $@ "
	@$(LD) -o $@ $(CC_OBJTARGETS) $(CPP_OBJTARGETS) $(AS_OBJTARGETS) $(LD_FLAGS) -Wl,-L $(BUILDDIR) -lrt -lpthread

#@echo [APP]   $@
#@$(OBJDUMP) -d $(PROJECT_BINARY) > $(PROJECT_BINARY).asm
