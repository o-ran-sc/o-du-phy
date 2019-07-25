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
CC := icc

ifeq ($(RTE_SDK),)
    $(error "Please define RTE_SDK environment variable")
endif

RTE_TARGET := x86_64-native-linuxapp-icc
RTE_INC := $(RTE_SDK)/$(RTE_TARGET)/include
#include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(XRAN_DIR),)
    XRAN_DIR=$(PWD)/../..
endif

COMMON_SRC=$(XRAN_DIR)/app/common
SRC_SRC=$(XRAN_DIR)/app/src

ifeq ($(MLOG_DIR),)
    MLOG_DIR=$(XRAN_DIR)/../mlog
endif

APP = sample-lls-cu
SRC = $(COMMON_SRC)/common.c \
	./sample-lls-cu.c \
	$(SRC_SRC)/config.c 

CFLAGS += -std=gnu11 -Wall -wd9 -Wextra -Werror -I$(XRAN_DIR)/lib/api -I$(COMMON_SRC) -I$(SRC_SRC) -I$(MLOG_DIR)/source -I$(RTE_INC)
ifeq ($(ME),1)
    CFLAGS += -DMLOG_ENABLED
endif
ifeq ($(NB),1)
    CFLAGS += -DNightly_build
endif

ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG -O0 -g
else
    CFLAGS += -O3
endif

ifeq ($(MULTI_SECTION),1)
    CFLAGS += -DMULTI_SECTION
endif

LDFLAGS += -pthread -lrt

RTE_LIBS = -L$(RTE_SDK)/$(RTE_TARGET)/lib -Wl,-lrte_flow_classify -Wl,--whole-archive -Wl,-lrte_pipeline -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_table -Wl,--no-whole-archive -Wl,--whole-archive -Wl,-lrte_port -Wl,--no-whole-archive -Wl,-lrte_pdump -Wl,-lrte_distributor -Wl,-lrte_ip_frag -Wl,-lrte_meter -Wl,-lrte_lpm -Wl,--whole-archive -Wl,-lrte_acl -Wl,--no-whole-archive -Wl,-lrte_jobstats -Wl,-lrte_metrics -Wl,-lrte_bitratestats -Wl,-lrte_latencystats -Wl,-lrte_power -Wl,-lrte_efd -Wl,-lrte_bpf -Wl,--whole-archive -Wl,-lrte_cfgfile -Wl,-lrte_gro -Wl,-lrte_gso -Wl,-lrte_hash -Wl,-lrte_member -Wl,-lrte_vhost -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_net -Wl,-lrte_ethdev -Wl,-lrte_bbdev -Wl,-lrte_cryptodev -Wl,-lrte_security -Wl,-lrte_compressdev -Wl,-lrte_eventdev -Wl,-lrte_rawdev -Wl,-lrte_timer -Wl,-lrte_mempool -Wl,-lrte_mempool_ring -Wl,-lrte_ring -Wl,-lrte_pci -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_reorder -Wl,-lrte_sched -Wl,-lrte_kni -Wl,-lrte_common_octeontx -Wl,-lrte_bus_pci -Wl,-lrte_bus_vdev -Wl,-lrte_bus_dpaa -Wl,-lrte_bus_fslmc -Wl,-lrte_mempool_bucket -Wl,-lrte_mempool_stack -Wl,-lrte_mempool_dpaa -Wl,-lrte_mempool_dpaa2 -Wl,-lrte_pmd_af_packet -Wl,-lrte_pmd_ark -Wl,-lrte_pmd_avf -Wl,-lrte_pmd_avp -Wl,-lrte_pmd_axgbe -Wl,-lrte_pmd_bnxt -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_cxgbe -Wl,-lrte_pmd_dpaa -Wl,-lrte_pmd_dpaa2 -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ena -Wl,-lrte_pmd_enic -Wl,-lrte_pmd_fm10k -Wl,-lrte_pmd_failsafe -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_kni -Wl,-lrte_pmd_lio -Wl,-lrte_pmd_nfp -Wl,-lrte_pmd_null -Wl,-lrte_pmd_qede -Wl,-lrte_pmd_ring -Wl,-lrte_pmd_softnic -Wl,-lrte_pmd_tap -Wl,-lrte_pmd_thunderx_nicvf -Wl,-lrte_pmd_vdev_netvsc -Wl,-lrte_pmd_virtio -Wl,-lrte_pmd_vhost -Wl,-lrte_pmd_ifc -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_bus_vmbus -Wl,-lrte_pmd_netvsc -Wl,-lrte_pmd_bbdev_null -Wl,-lrte_pmd_null_crypto -Wl,-lrte_pmd_crypto_scheduler -Wl,-lrte_pmd_dpaa2_sec -Wl,-lrte_pmd_dpaa_sec -Wl,-lrte_pmd_virtio_crypto -Wl,-lrte_pmd_octeontx_zip -Wl,-lrte_pmd_qat -Wl,-lrte_pmd_skeleton_event -Wl,-lrte_pmd_sw_event -Wl,-lrte_pmd_octeontx_ssovf -Wl,-lrte_pmd_dpaa_event -Wl,-lrte_pmd_dpaa2_event -Wl,-lrte_mempool_octeontx -Wl,-lrte_pmd_octeontx -Wl,-lrte_pmd_opdl_event -Wl,-lrte_pmd_skeleton_rawdev -Wl,-lrte_pmd_dpaa2_cmdif -Wl,-lrte_pmd_dpaa2_qdma -Wl,-lrte_bus_ifpga -Wl,-lrte_pmd_ifpga_rawdev -Wl,--no-whole-archive -Wl,-lrt -Wl,-lm -Wl,-lnuma -Wl,-ldl -Wl,

XRAN_LIB_DIR=$(XRAN_DIR)/lib/bin
LDFLAGS += -L$(XRAN_LIB_DIR) -Wl, -lxran -Wl, -L$(MLOG_DIR)/bin -Wl, $(RTE_LIBS)
ifeq ($(ME),1)
    LDFLAGS += -Wl,-lmlog
endif

OBJ = $(foreach file,$(SRC),$(file:.c=.o))


all: $(APP) install
	
$(OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

$(APP): $(OBJ)
	$(CC) -o $(APP) $(CFLAGS) $(OBJ)  $(LDFLAGS)

install: $(APP)
	@mkdir -p bin
	@cp $(APP) ./bin

clean:
	@rm -rf $(APP) $(OBJ) ./bin/$(APP)

#include $(RTE_SDK)/mk/rte.extapp.mk
