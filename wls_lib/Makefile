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

MYCUSTOMTAB='     '
MYCUSTOMSPACE='============================================================================================'
MYCUSTOMSPACE1='------------------------------------------------------------'

##############################################################
#  Tools configuration
##############################################################
CC  := icc
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

PROJECT_NAME := libwls
PROJECT_TYPE := lib
PROJECT_DIR  := ./
BUILDDIR := make
PROJECT_BINARY := $(PROJECT_NAME).so

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

ifeq ($(MESON_BUILD),0)
RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include
else
RTE_INC := $(shell PKG_CONFIG_PATH=$(RTE_SDK)/build/meson-uninstalled pkg-config --cflags-only-I libdpdk)
endif

CC_SRC = wls_lib_dpdk.c \
	syslib.c

CC_FLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations  \
	-fdata-sections \
	-ffunction-sections \
	-g \
    -fPIC \
	-Wall \
	-Wimplicit-function-declaration \
	-g -O3 -wd1786 -mcmodel=large

INC := -I$(RTE_INC)
DEF :=

AS_FLAGS :=
AR_FLAGS := rc

PROJECT_OBJ_DIR := $(BUILDDIR)/obj

CC_OBJS := $(patsubst %.c,%.o,$(CC_SRC))
AS_OBJS := $(patsubst %.s,%.o,$(AS_SRC))
OBJS    := $(CC_OBJS) $(AS_OBJS) $(LIBS)
DIRLIST := $(addprefix $(PROJECT_OBJ_DIR)/,$(sort $(dir $(OBJS))))

CC_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(CC_OBJS))

AS_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(AS_OBJS))
CC_FLAGS_FULL  := $(CC_FLAGS) $(INC) $(DEF)

AS_FLAGS := $(AS_FLAGS) $(INC)

PROJECT_DEP_FILE := $(PROJECT_OBJ_DIR)/$(PROJECT_NAME).dep

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))
GENERATE_DEPS :=
else

CC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CC_SRC)))
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

.PHONY : generate_deps
generate_deps : clear_dep $(CC_DEPS)


.PHONY : echo_start_build
echo_start_build :
	@echo [BUILD] $(PROJECT_TYPE) : $(PROJECT_NAME)

$(DIRLIST) :
	-@$(MD) $@

$(CC_OBJTARGETS) :
	@echo [CC]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(CC) -c $(CC_FLAGS_FULL) -o"$@" $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))

$(AS_OBJTARGETS) :
	@echo [AS]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(AS) $(AS_FLAGS) -o"$@" $(patsubst %.o,%.s,$(subst $(PROJECT_OBJ_DIR)/,,$@))

ifeq ($(wildcard $(PROJECT_DEP_FILE)),$(PROJECT_DEP_FILE))

include $(PROJECT_DEP_FILE)

endif

.PHONY: clean xclean
clean:
	@echo [CLEAN]  : $(PROJECT_NAME)
	@$(RM) $(CC_OBJTARGETS) $(AS_OBJTARGETS)
ifneq ($(wildcard $(PROJECT_DIR)/$(PROJECT_MAKE)),)
	@echo [CLEAN] : $(PROJECT_NAME)
	@$(RM) $(PROJECT_BINARY) $(PROJECT_BINARY_LIB) $(PROJECT_DEP_FILE)
endif

xclean: clean

.PHONY : welcome_line
welcome_line :
	@echo $(MYCUSTOMSPACE)
	@echo Building  $(PROJECT_BINARY)
	@echo $(MYCUSTOMSPACE)

.PHONY : debug release

debug :  all
release :  all

$(PROJECT_BINARY) : $(DIRLIST) echo_start_build $(GENERATE_DEPS) $(PRE_BUILD) $(CC_OBJTARGETS) $(AS_OBJTARGETS)
	@echo [AR]    $(subst $(BUILDDIR)/,,$@)
	@$(CC) $(CC_OBJTARGETS) $(AS_OBJTARGETS) -shared -fPIC -o $@

