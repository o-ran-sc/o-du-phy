###############################################################################
#
#   Copyright (c) 2021 Intel.
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
ifeq ($(WIRELESS_SDK_TOOLCHAIN),icc)
CC := icc
CPP := icpc
AS := as
AR := ar
LD := icc
else ifeq ($(WIRELESS_SDK_TOOLCHAIN),icx)
    CC  := icx
    CPP := icx
    AS := as
    AR := ar
    LD := icx
else
    $(error "Please define WIRELESS_SDK_TOOLCHAIN environment variable")
endif

ifeq ($(WIRELESS_SDK_TARGET_ISA),sse)
    TARGET_PROCESSOR := -xSSE4.2
else ifeq ($(WIRELESS_SDK_TARGET_ISA),avx2)
    TARGET_PROCESSOR := -xCORE-AVX2
else ifeq ($(WIRELESS_SDK_TARGET_ISA),avx512)
    TARGET_PROCESSOR := -xCORE-AVX512
else ifeq ($(WIRELESS_SDK_TARGET_ISA),snc)
    TARGET_PROCESSOR := -xicelake-server
else ifeq ($(WIRELESS_SDK_TARGET_ISA),spr)
    TARGET_PROCESSOR := -march=sapphirerapids
endif

ifeq ($(TARGET_PROCESSOR),)
    $(error "Please define valid WIRELESS_SDK_TARGET_ISA environment variable $(WIRELESS_SDK_TARGET_ISA)")
endif
OBJDUMP := objdump
MD := mkdir -p
CP := cp -f
RM := rm -rf

##############################################################
# TARGET
##############################################################
ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

RTE_INC := $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --cflags-only-I libdpdk)

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
 -I$(WLSDIR) \
 -I$(SRCDIR)/../include \
 -I$(SRCDIR)/common \
 -I$(SRCDIR)/include \
 -I$(SRCDIR)/framework/workers \
 -I$(SRCDIR)/framework/wls/fapi2mac \
 -I$(SRCDIR)/framework/wls/fapi2phy \
 -I$(SRCDIR)/framework/wls/lib \
 -I$(SRCDIR)/api/fapi2mac \
 -I$(SRCDIR)/api/fapi2phy \
 -I$(FLEXRANDIR)/l1/source/nr5g/api \
 -I$(FLEXRANDIR)/l1/source/common \
 -I$(SRCDIR)/api/fapi2phy/p5 \
 -I$(SRCDIR)/api/fapi2phy/p7 \
 -I$(SRCDIR)/api/fapi2mac/p5 \
 -I$(SRCDIR)/api/fapi2mac/p7 \
 -I$(SRCDIR)/utils \
 $(RTE_INC) \
 
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

CFLAGS := -g -Wall -Wextra -Wunused -diag-disable9 -Wno-deprecated-declarations -Wimplicit-function-declaration -fasm-blocks -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -fwrapv -mssse3 $(DEFS) $(INC)

ifeq ($(PRINTDBG),)
CFLAGS := $(CFLAGS) -Werror
endif

RTE_LIBS := $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --static --libs libdpdk)
LDFLAGS := -z now -z relro -z noexecstack -g -Wl,-lrt -Wl,-lpthread -Wl,-lhugetlbfs -Wl,-lm -Wl,-lnuma -L $(WLSDIR) -lwls

LINUX_ORAN_5G_FAPI_SRC := \
	$(SRCDIR)/nr5g_fapi.c \
	$(SRCDIR)/utils/nr5g_fapi_args.c \
	$(SRCDIR)/utils/nr5g_fapi_config_loader.c \
	$(SRCDIR)/utils/nr5g_fapi_log.c \
	$(SRCDIR)/utils/nr5g_fapi_stats.c \
	$(SRCDIR)/utils/nr5g_fapi_memory.c \
	$(SRCDIR)/utils/nr5g_fapi_cmd.c \
	$(SRCDIR)/utils/nr5g_fapi_snr_conversion.c \
	$(SRCDIR)/framework/workers/nr5g_fapi_mac2phy_thread.c \
	$(SRCDIR)/framework/workers/nr5g_fapi_phy2mac_thread.c \
	$(SRCDIR)//framework/workers/nr5g_fapi_urllc_phy2mac_thread.c \
	$(SRCDIR)//framework/workers/nr5g_fapi_urllc_mac2phy_thread.c \
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
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_rx_data_uci_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_srs_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_uci_ind.c \
	$(SRCDIR)/api/fapi2mac/p7/nr5g_fapi_proc_vendor_p7_msgs.c \
	$(SRCDIR)/api/fapi2phy/nr5g_fapi_fapi2phy_api.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_add_remove_core_msg.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_config_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_param_resp.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_start_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_stop_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_shutdown_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_dl_iq_samples_req.c \
	$(SRCDIR)/api/fapi2phy/p5/nr5g_fapi_proc_ul_iq_samples_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_dl_tti_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_ul_tti_req.c \
	$(SRCDIR)/api/fapi2phy/p7/nr5g_fapi_proc_tti_req_common.c \
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
	@$(CC) -o $(APP) $(OBJS) $(LDFLAGS) $(RTE_LIBS) -lstdc++ # stdc++ flag needed for RTE LIBS
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

