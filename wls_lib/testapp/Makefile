################################################################
# <COPYRIGHT_TAG>
################################################################

MYCUSTOMTAB='     '
MYCUSTOMSPACE='============================================================================================'
MYCUSTOMSPACE1='------------------------------------------------------------'

##############################################################
#  Tools configuration
##############################################################
ifeq ($(WIRELESS_SDK_TOOLCHAIN),icc)
    CC  := icc
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

MD := mkdir -p
CP := cp -f
RM := rm -rf

PROJECT_NAME := wls_test
PROJECT_TYPE := elf
PROJECT_DIR  := ./
BUILDDIR := make
PROJECT_BINARY := $(PROJECT_NAME)

ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

RTE_LIBS := -Wl,--whole-archive -Wl,/usr/lib64/libnuma.so $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --static --libs libdpdk) -Wl,--no-whole-archive
RTE_INC := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):/usr/lib64/pkgconfig:$(RTE_SDK)/build/meson-uninstalled pkgconf --cflags-only-I libdpdk)

CC_SRC = pool.c \
	testapp.c

CC_FLAGS += -std=gnu11 -Wall -Wno-deprecated-declarations  \
	-fdata-sections \
	-ffunction-sections \
	-g \
	-Wall \
	-Wimplicit-function-declaration \
	-g -O3 -mcmodel=large $(TARGET_PROCESSOR)

INC :=  -I../ -I$(RTE_INC)
DEF :=

WLS_LIB_DIR=../
LD_FLAGS += -L$(WLS_LIB_DIR) -lwls -lpthread -lrt

LD_FLAGS += $(RTE_LIBS)

ifeq ($(WIRELESS_SDK_TOOLCHAIN),icx)
LD_FLAGS += -Wl,-lstdc++
endif

#Add CodeCoverage option
ifeq ($(CODE_COVERAGE),1)
    CC_FLAGS += -fprofile-instr-generate -fcoverage-mapping
    LD_FLAGS += -fprofile-instr-generate
endif

AS_FLAGS :=
AR_FLAGS := rc

PROJECT_OBJ_DIR := $(BUILDDIR)/obj
PROJECT_DEP_DIR := $(PROJECT_OBJ_DIR)/$(PROJECT_NAME).dep.d

CC_OBJS := $(patsubst %.c,%.o,$(CC_SRC))
AS_OBJS := $(patsubst %.s,%.o,$(AS_SRC))
OBJS    := $(CC_OBJS) $(AS_OBJS) $(LIBS)
DIRLIST := $(addprefix $(PROJECT_OBJ_DIR)/,$(sort $(dir $(OBJS)))) $(PROJECT_DEP_DIR)

CC_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(CC_OBJS))

AS_OBJTARGETS := $(addprefix $(PROJECT_OBJ_DIR)/,$(AS_OBJS))

CC_FLAGS_FULL  := $(CC_FLAGS)  $(INC) $(DEF)

AS_FLAGS := $(AS_FLAGS) $(INC)

ifeq ($(wildcard $(PROJECT_DEP_DIR)),$(PROJECT_DEP_DIR))
GENERATE_DEPS :=
else

CC_DEPS  := $(addprefix __dep__,$(subst ../,__up__,$(CC_SRC)))
GENERATE_DEPS := generate_deps
endif

all : welcome_line 	$(PROJECT_BINARY)
	@echo $(PROJECT_BINARY)

.PHONY : clear_dep
clear_dep:
	@echo [DEP] rm $(PROJECT_DEP_DIR)/$(@F).dep
	@$(RM) $(PROJECT_DEP_DIR)/$(@F).dep

$(CC_DEPS) : $(PROJECT_DEP_DIR)
	@$(CC) $(CC_FLAGS_FULL) -MM $(subst __up__,../,$(subst __dep__,,$@)) -MT $(PROJECT_OBJ_DIR)/$(patsubst %.c,%.o,$(subst __up__,../,$(subst __dep__,,$@))) >> $(PROJECT_DEP_DIR)/$(@F).dep

.PHONY : generate_deps
generate_deps : clear_dep $(CC_DEPS)


.PHONY : echo_start_build
echo_start_build :
	@echo [BUILD] $(PROJECT_TYPE) : $(PROJECT_NAME)

$(DIRLIST) :
	-@$(MD) $@

$(CC_OBJTARGETS) : $(GENERATE_DEPS)
	@echo [CC]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(CC) -c $(CC_FLAGS_FULL) -o"$@" $(patsubst %.o,%.c,$(subst $(PROJECT_OBJ_DIR)/,,$@))

$(AS_OBJTARGETS) : $(CC_OBJTARGETS)
	@echo [AS]    $(subst $(PROJECT_OBJ_DIR)/,,$@)
	@$(AS) $(AS_FLAGS) -o"$@" $(patsubst %.o,%.s,$(subst $(PROJECT_OBJ_DIR)/,,$@))

ifeq ($(wildcard $(PROJECT_DEP_DIR)),$(PROJECT_DEP_DIR))

include $(PROJECT_DEP_DIR)/*

endif

.PHONY: clean xclean
clean:
	@echo [CLEAN]  : $(PROJECT_NAME)
	@$(RM) $(CC_OBJTARGETS) $(AS_OBJTARGETS)
ifneq ($(wildcard $(PROJECT_DIR)/$(PROJECT_MAKE)),)
	@echo [CLEAN] : $(PROJECT_NAME)
	@$(RM) -rf $(PROJECT_BINARY) $(PROJECT_BINARY_LIB) $(PROJECT_DEP_DIR)
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

$(PROJECT_BINARY): $(DIRLIST) echo_start_build $(GENERATE_DEPS) $(CC_OBJTARGETS) $(AS_OBJTARGETS)
	@echo "[LD] $@ "
	@$(LD) -o $@ $(CC_OBJTARGETS) $(AS_OBJTARGETS) $(LD_FLAGS) -Wl,-L $(BUILDDIR)

#@echo [APP]   $@

