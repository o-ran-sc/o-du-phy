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

RTE_INC := $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --cflags-only-I libdpdk)

API_DIR := $(XRAN_DIR)/lib/api
OWD_DIR := $(XRAN_DIR)/lib/src
SRC_DIR := $(PROJECT_DIR)/src

ifeq ($(MLOG),1)
ifeq ($(MLOG_DIR),)
    MLOG_DIR=$(XRAN_DIR)/../mlog
endif
endif

CC_SRC = $(SRC_DIR)/common.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/app_io_fh_xran.c \
	$(SRC_DIR)/app_profile_xran.c \
	$(SRC_DIR)/sample-app.c

CC_FLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations  \
	-fdata-sections \
	-ffunction-sections \
	-g \
	-Wall \
	-Wimplicit-function-declaration \
	-g -O3 -wd1786 -mcmodel=large

CPP_FLAGS := -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D_GNU_SOURCE -D_REENTRANT -pipe -no-prec-div \
                -no-prec-div -fp-model fast=2\
                -no-prec-sqrt  -falign-functions=16 -fast-transcendentals \
        -Werror -Wno-unused-variable -std=c++11 -mcmodel=large

INC :=  -I$(API_DIR) -I$(RTE_INC) -I$(OWD_DIR)
DEF :=

ifeq ($(MLOG),1)
	INC  += -I$(MLOG_DIR)/source
	DEF += -DMLOG_ENABLED
else
	DEF += -UMLOG_ENABLED
endif


XRAN_LIB_DIR=$(XRAN_DIR)/lib/build
LD_FLAGS += -L$(XRAN_LIB_DIR) -lxran

RTE_LIBS = $(shell PKG_CONFIG_PATH=/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --static --libs libdpdk)

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
