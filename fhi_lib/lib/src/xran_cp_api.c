/******************************************************************************
*
*   Copyright (c) 2019 Intel.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*       http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*
*******************************************************************************/


/**
 * @brief This file provides the API functions to build Control Plane Messages
 *      for XRAN Front Haul layer as defined in XRAN-FH.CUS.0-v02.01.
 *
 * @file xran_cp_api.c
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#include <rte_branch_prediction.h>

#include "xran_common.h"
#include "xran_transport.h"
#include "xran_cp_api.h"
#include "xran_hash.h"
#include "xran_printf.h"


struct xran_sectioninfo_db {
    uint32_t    max_num;
    uint32_t    cur_index;
#if defined(XRAN_CP_USES_HASHTABLE)
    struct rte_hash *hash;
#endif
    struct xran_section_info *list;
    };


static struct xran_sectioninfo_db *sectiondb[XRAN_DIR_MAX];

int xran_cp_init_sectiondb(void *pHandle)
{
  int i, j, k;
  uint32_t size;
  uint16_t cid;
  struct xran_sectioninfo_db *ptr;
  uint8_t num_eAxc;


#if !defined(PRACH_USES_SHARED_PORT)
    num_eAxc = xran_get_num_eAxc(pHandle) * 2;
#else
    num_eAxc = xran_get_num_eAxc(pHandle);
#endif

    for(i=0; i < XRAN_DIR_MAX; i++) {
        size = (xran_get_num_cc(pHandle) * num_eAxc * sizeof(struct xran_sectioninfo_db));
        print_log("Allocation Size for Section DB : %d (%dx%dx%ld)", size
                    , xran_get_num_cc(pHandle)
                    , num_eAxc
                    , sizeof(struct xran_sectioninfo_db));
        sectiondb[i] = malloc(size);

        if(sectiondb[i] == NULL) {
            print_err("Allocation Failed for Section DB!");
            return (-XRAN_ERRCODE_OUTOFMEMORY);
            }

        for(j=0; j < xran_get_num_cc(pHandle); j++) {         // CC
            for(k=0; k < num_eAxc; k++) {   // antenna
                ptr = sectiondb[i] + num_eAxc*j + k;

                ptr->max_num = xran_get_max_sections(pHandle);
                ptr->cur_index = 0;

                // allicate array to store section information
                size = sizeof(struct xran_section_info)*xran_get_max_sections(pHandle);
                print_log("Allocation Size for list : %d (%ldx%d)", size,
                            sizeof(struct xran_section_info),
                            xran_get_max_sections(pHandle));
                ptr->list = malloc(size);
                if(ptr-> list  == NULL) {
                    print_err("Allocation Failed for Section DB!");
                    return (-XRAN_ERRCODE_OUTOFMEMORY);
                    }

#if defined(XRAN_CP_USES_HASHTABLE)
                // Create hash table for section information
                cid = rte_be_to_cpu_16(xran_compose_cid(xran_get_llscuid(pHandle), xran_get_sectorid(pHandle), j, k));
                print_log("Creating hash for %04X", cid);
                ptr->hash = xran_section_init_hash(i, cid, xran_get_max_sections(pHandle));
#endif
                }
            }
        }

    return (XRAN_ERRCODE_OK);
}

int xran_cp_free_sectiondb(void *pHandle)
{
  int i, j, k;
  uint32_t size;
  struct xran_sectioninfo_db *ptr;
  uint8_t num_eAxc;

#if !defined(PRACH_USES_SHARED_PORT)
    num_eAxc = xran_get_num_eAxc(pHandle) * 2;
#else
    num_eAxc = xran_get_num_eAxc(pHandle);
#endif

    for(i=0; i < XRAN_DIR_MAX; i++) {
        for(j=0; j < xran_get_num_cc(pHandle); j++) {         // CC
            for(k=0; k < num_eAxc; k++) {   // antenna
                ptr = sectiondb[i] + num_eAxc*j + k;

#if defined(XRAN_CP_USES_HASHTABLE)
                xran_section_free_hash(ptr->hash);
#endif
                if(ptr->list != NULL)
                    free(ptr->list);
                else print_err("list is NULL");
                }
            }
        if(sectiondb[i] != NULL)
            free(sectiondb[i]);
        else print_err("sectiondb[%d] is NULL", i);
        }

    return (XRAN_ERRCODE_OK);
}

static struct xran_sectioninfo_db *xran_get_section_db(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id)
{
  struct xran_sectioninfo_db *ptr;
  uint8_t num_eAxc;

    if(unlikely(dir>=XRAN_DIR_MAX)) {
        print_err("Invalid direction - %d", dir);
        return (NULL);
        }

    if(unlikely(cc_id >= xran_get_num_cc(pHandle))) {
        print_err("Invalid CC id - %d", cc_id);
        return (NULL);
        }

#if !defined(PRACH_USES_SHARED_PORT)
    num_eAxc = xran_get_num_eAxc(pHandle) * 2;
#else
    num_eAxc = xran_get_num_eAxc(pHandle);
#endif

    if(unlikely(ruport_id >= num_eAxc)) {
        print_err("Invalid eAxC id - %d", ruport_id);
        return (NULL);
        }

    ptr = sectiondb[dir] + xran_get_num_eAxc(pHandle)*cc_id + ruport_id;

    return(ptr);
}

static struct xran_section_info *xran_get_section_info(struct xran_sectioninfo_db *ptr, uint16_t index)
{
    if(unlikely(ptr == NULL))
        return (NULL);

    if(unlikely(ptr->max_num < index)) {
        print_err("Index is out of range - %d", index);
        return (NULL);
        }

    return(&(ptr->list[index]));
}

int xran_cp_add_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id,
        struct xran_section_info *info)
{
  struct xran_sectioninfo_db *ptr;
  struct xran_section_info *list;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL)) {
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    if(unlikely(ptr->cur_index >= ptr->max_num)) {
        print_err("No more space to add section information!");
        return (-XRAN_ERRCODE_OUTOFMEMORY);
        }

    list = xran_get_section_info(ptr, ptr->cur_index);

    rte_memcpy(list, info, sizeof(struct xran_section_info));
#if defined(XRAN_CP_USES_HASHTABLE)
    xran_section_add_hash(ptr->hash, info->id, ptr->cur_index);
#endif

    ptr->cur_index++;

    return (XRAN_ERRCODE_OK);
}

int xran_cp_add_multisection_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id,
        uint8_t num_sections, struct xran_section_gen_info *gen_info)
{
  int i;
  struct xran_sectioninfo_db *ptr;
  struct xran_section_info *list;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL)) {
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    if(unlikely(ptr->cur_index >= (ptr->max_num+num_sections))) {
        print_err("No more space to add section information!");
        return (-XRAN_ERRCODE_OUTOFMEMORY);
        }

    list = xran_get_section_info(ptr, ptr->cur_index);

    for(i=0; i<num_sections; i++) {
        rte_memcpy(&list[i], &gen_info[i].info, sizeof(struct xran_section_info));
#if defined(XRAN_CP_USES_HASHTABLE)
        xran_section_add_hash(ptr->hash, gen_info[i].info.id, ptr->cur_index);
#endif
        ptr->cur_index++;
        }

    return (XRAN_ERRCODE_OK);
}

struct xran_section_info *xran_cp_find_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id,
        uint16_t section_id)
{
  int index;
  struct xran_sectioninfo_db *ptr;


    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL))
        return (NULL);

#if defined(XRAN_CP_USES_HASHTABLE)
    index = xran_section_lookup(ptr->hash, section_id);
    if(unlikely(index > ptr->max_num)) {
        print_err("Invalid index - %d", index);
        return (NULL);
        }

    if(index < 0) {
        print_dbg("No section ID in the list - %d", section_id);
        return (NULL);
        }

    return (xran_get_section_info(ptr, index));
#else
    for(index=0; index<ptr->cur_index; index++) {
        if(ptr->list[index].id == section_id) {
            print_dbg("Found section info %04X", section_id);
            return (xran_get_section_info(ptr, index));
            }
        }

    print_dbg("No section ID in the list - %d", section_id);
    return (NULL);
#endif

}

struct xran_section_info *xran_cp_iterate_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t subframe_id, uint8_t slot_id, uint32_t *next)
{
  int index;
  struct xran_sectioninfo_db *ptr;


    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL))
        return (NULL);

#if defined(XRAN_CP_USES_HASHTABLE)
    index = xran_section_iterate(ptr->hash, next);
    if(unlikely(index > ptr->max_num)) {
        print_err("Invalid index - %d", index);
        return (NULL);
        }

    if(index < 0) {
        print_dbg("No section ID in the list - %d", section_id);
        return (NULL);
        }

    return (xran_get_section_info(ptr, index));
#else
    index = *next;
    if(*next < ptr->cur_index) {
        (*next)++;
        return (xran_get_section_info(ptr, index));
        }
    else 
        print_dbg("No more sections in the list");

    return (NULL);
#endif
}

int xran_cp_getsize_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id)
{
  int i, index;
  struct xran_sectioninfo_db *ptr;


    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL))
        return (-1);

    return (ptr->cur_index);
}

int xran_cp_reset_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id)
{
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id);
    if(unlikely(ptr == NULL)) {
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    ptr->cur_index = 0;
#if defined(XRAN_CP_USES_HASHTABLE)
    xran_section_reset_hash(ptr->hash);
#endif

    return (XRAN_ERRCODE_OK);
}

int xran_dump_sectiondb(void)
{
    // TODO:
    return (0);
}


// Cyclic Prefix Length 5.4.4.14
// CP_length = cpLength * Ts * 2^u,  Ts = 1/30.72MHz, if u is N/A, it shall be zero
#define CPLEN_TS           (30720000)
inline uint16_t xran_get_cplength(int cpLength, int uval)    // uval = -1 for N/A
{
    return ((cpLength * ((uval<0)?0:(2<<uval))) / (CPLEN_TS));
}

// Frequency offset 5.4.5.11
// frequency_offset = freqOffset * SCS * 0.5
inline int32_t xran_get_freqoffset(int freqOffset, int scs)
{
    return ((freqOffset * scs)>>1);
}


/**
 * @brief Fill the section body of type 0 in C-Plane packet
 *
 * @param section
 *  A pointer to the section in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section0(
                struct xran_cp_radioapp_section0 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
#endif

    section->hdr.sectionId  = params->info.id;
    section->hdr.rb         = params->info.rb;
    section->hdr.symInc     = params->info.symInc;
    section->hdr.startPrbc  = params->info.startPrbc;
    section->hdr.numPrbc    = params->info.numPrbc;

    section->hdr.u.s0.reMask    = params->info.reMask;
    section->hdr.u.s0.numSymbol = params->info.numSymbol;
    section->hdr.u.s0.reserved  = 0;

    // for network byte order
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_ERRCODE_OK);
}
/**
 * @brief Fill the section header of type 0 in C-Plane packet
 *
 * @param s0hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section0_hdr(
                struct xran_cp_radioapp_section0_header *s0hdr,
                struct xran_cp_gen_params *params)

{
    s0hdr->timeOffset               = rte_cpu_to_be_16(params->hdr.timeOffset);
    s0hdr->frameStructure.fftSize   = params->hdr.fftSize;
    s0hdr->frameStructure.uScs      = params->hdr.scs;
    s0hdr->cpLength                 = rte_cpu_to_be_16(params->hdr.cpLength);
    s0hdr->reserved                 = 0;

    return (XRAN_ERRCODE_OK);
}

/**
 * @brief Fill the section body of type 1 in C-Plane packet
 *  Extension is not supported.
 *
 * @param section
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section1(
                struct xran_cp_radioapp_section1 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
#endif

    section->hdr.sectionId      = params->info.id;
    section->hdr.rb             = params->info.rb;
    section->hdr.symInc         = params->info.symInc;
    section->hdr.startPrbc      = params->info.startPrbc;
    section->hdr.numPrbc        = params->info.numPrbc;

    section->hdr.u.s1.reMask    = params->info.reMask;
    section->hdr.u.s1.numSymbol = params->info.numSymbol;
    section->hdr.u.s1.beamId    = params->info.beamId;

    if(params->info.ef) {
        // TODO: need to handle extension
        print_err("Extension is not supported!");
        section->hdr.u.s1.ef         = 0;
//        section->hdr.u.s1.ef         = params->info.ef;
        }
    else
        section->hdr.u.s1.ef         = 0;

    // for network byte order
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_ERRCODE_OK);
}
/**
 * @brief Fill the section header of type 1 in C-Plane packet
 *
 * @param s1hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section1_hdr(
                struct xran_cp_radioapp_section1_header *s1hdr,
                struct xran_cp_gen_params *params)
{
    s1hdr->udComp.udIqWidth         = params->hdr.iqWidth;
    s1hdr->udComp.udCompMeth        = params->hdr.compMeth;
    s1hdr->reserved                 = 0;

    return (XRAN_ERRCODE_OK);
}

/**
 * @brief Fill the section body of type 3 in C-Plane packet
 *  Extension is not supported.
 *
 * @param section
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section3(
                struct xran_cp_radioapp_section3 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
#endif

    section->hdr.sectionId      = params->info.id;
    section->hdr.rb             = params->info.rb;
    section->hdr.symInc         = params->info.symInc;
    section->hdr.startPrbc      = params->info.startPrbc;
    section->hdr.numPrbc        = params->info.numPrbc;

    section->hdr.u.s3.reMask    = params->info.reMask;
    section->hdr.u.s3.numSymbol = params->info.numSymbol;
    section->hdr.u.s3.beamId    = params->info.beamId;

    section->freqOffset         = rte_cpu_to_be_32(params->info.freqOffset)>>8;
    section->reserved           = 0;

    if(params->info.ef) {
        // TODO: need to handle extension
        print_err("Extension is not supported!");
        section->hdr.u.s3.ef         = 0;
//        section->hdr.u.s3.ef         = params->info.ef;
        }
    else
        section->hdr.u.s3.ef         = 0;

    // for network byte order (header, 8 bytes)
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_ERRCODE_OK);
}
/**
 * @brief Fill the section header of type 3 in C-Plane packet
 *
 * @param s3hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static int xran_prepare_section3_hdr(
                struct xran_cp_radioapp_section3_header *s3hdr,
                struct xran_cp_gen_params *params)

{
    s3hdr->timeOffset               = rte_cpu_to_be_16(params->hdr.timeOffset);
    s3hdr->frameStructure.fftSize   = params->hdr.fftSize;
    s3hdr->frameStructure.uScs      = params->hdr.scs;
    s3hdr->cpLength                 = rte_cpu_to_be_16(params->hdr.cpLength);
    s3hdr->udComp.udIqWidth         = params->hdr.iqWidth;
    s3hdr->udComp.udCompMeth        = params->hdr.compMeth;

    return (XRAN_ERRCODE_OK);
}

/**
 * @brief add sections to C-Plane packet
 *  Section type 1 and 3 are supported.
 *
 * @param mbuf
 *  A pointer to the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
int xran_append_control_section(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params)
{
  int i, ret;
  uint32_t totalen;
  void *section;
  int section_size;
  int (*xran_prepare_section_func)(void *section, void *params);


    totalen = 0;
    switch(params->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: /* Unused RB or Symbols in DL or UL, not supportted */
            section_size                = sizeof(struct xran_cp_radioapp_section0);
            xran_prepare_section_func   = (int (*)(void *, void *))xran_prepare_section0;
            break;

        case XRAN_CP_SECTIONTYPE_1: /* Most DL/UL Radio Channels */
            section_size                = sizeof(struct xran_cp_radioapp_section1);
            xran_prepare_section_func   = (int (*)(void *, void *))xran_prepare_section1;
            break;

        case XRAN_CP_SECTIONTYPE_3: /* PRACH and Mixed-numerology Channels */
            section_size                = sizeof(struct xran_cp_radioapp_section3);
            xran_prepare_section_func   = (int (*)(void *, void *))xran_prepare_section3;
            break;

        case XRAN_CP_SECTIONTYPE_5: /* UE scheduling information, not supported */
        case XRAN_CP_SECTIONTYPE_6: /* Channel Information, not supported */
        case XRAN_CP_SECTIONTYPE_7: /* LAA, not supported */
        default:
            section_size                = 0;
            xran_prepare_section_func   = NULL;
            print_err("Section Type %d is not supported!", params->sectionType);
            return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    if(unlikely(xran_prepare_section_func == NULL)) {
       print_err("Section Type %d is not supported!", params->sectionType);
       return (-XRAN_ERRCODE_INVALIDPARAM);
       }

    for(i=0; i<params->numSections; i++) {
        section = rte_pktmbuf_append(mbuf, section_size);
        if(section == NULL) {
            print_err("Fail to allocate the space for section[%d]!", i);
            return (-XRAN_ERRCODE_OUTOFMEMORY);
            }

        if(unlikely(xran_prepare_section_func((void *)section,
                            (void *)&params->sections[i]) < 0)) {
            return (-XRAN_ERRCODE_INVALIDPARAM);
            }

        totalen += section_size;
        }

    return (totalen);
}

/**
 * @brief fill the information of a radio application header in a C-Plane packet
 *
 * @param apphdr
 *  A pointer to the application header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
static inline int xran_prepare_radioapp_common_header(
                struct xran_cp_radioapp_common_header *apphdr,
                struct xran_cp_gen_params *params)
{

#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->dir != XRAN_DIR_DL && params->dir != XRAN_DIR_UL)) {
        print_err("Invalid direction!");
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
    if(unlikely(params->hdr.slotId > XRAN_SLOTID_MAX)) {
        print_err("Invalid Slot ID!");
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
    if(unlikely(params->hdr.startSymId > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid Symbol ID!");
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
#endif

    apphdr->dataDirection   = params->dir;
    apphdr->payloadVer      = XRAN_PAYLOAD_VER;
    apphdr->filterIndex     = params->hdr.filterIdx;
    apphdr->frameId         = params->hdr.frameId;
    apphdr->subframeId      = params->hdr.subframeId;
    apphdr->slotId          = params->hdr.slotId;
    apphdr->startSymbolId   = params->hdr.startSymId;
    apphdr->numOfSections   = params->numSections;
    apphdr->sectionType     = params->sectionType;

    // radio app header has common parts of 4bytes for all section types
    *((uint32_t *)apphdr) = rte_cpu_to_be_32(*((uint32_t *)apphdr));

    return (XRAN_ERRCODE_OK);
}

/**
 * @brief add a radio application header in a C-Plane packet
 *
 * @param mbuf
 *  A pointer to the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  0 on success; non zero on failure
 */
int xran_append_radioapp_header(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params)
{
  int ret;
  uint32_t totalen;
  struct xran_cp_radioapp_common_header *apphdr;
  int (*xran_prepare_radioapp_section_hdr_func)(void *hdr, void *params);


#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->sectionType >= XRAN_CP_SECTIONTYPE_MAX)) {
        print_err("Invalid Section Type - %d", params->sectionType);
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }
#endif

    switch(params->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: // Unused RB or Symbols in DL or UL, not supportted
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section0_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section0_header);
            break;

        case XRAN_CP_SECTIONTYPE_1: // Most DL/UL Radio Channels
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section1_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section1_header);
            break;

        case XRAN_CP_SECTIONTYPE_3: // PRACH and Mixed-numerology Channels
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section3_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section3_header);
            break;

        case XRAN_CP_SECTIONTYPE_5: // UE scheduling information, not supported
        case XRAN_CP_SECTIONTYPE_6: // Channel Information, not supported
        case XRAN_CP_SECTIONTYPE_7: // LAA, not supported
        default:
            print_err("Section Type %d is not supported!", params->sectionType);
            xran_prepare_radioapp_section_hdr_func = NULL;
            totalen = 0;
            return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    apphdr = (struct xran_cp_radioapp_common_header *)rte_pktmbuf_append(mbuf, totalen);
    if(unlikely(apphdr == NULL)) {
        print_err("Fail to reserve the space for radio application header!");
        return (-XRAN_ERRCODE_OUTOFMEMORY);
        }

    ret = xran_prepare_radioapp_common_header(apphdr, params);
    if(unlikely(ret < 0)) {
        return (ret);
        }

    if(likely(xran_prepare_radioapp_section_hdr_func)) {
        xran_prepare_radioapp_section_hdr_func(apphdr, params);
        }
    else {
        print_err("xran_prepare_radioapp_section_hdr_func is NULL!");
        return (-XRAN_ERRCODE_INVALIDPARAM);
        }

    return (totalen);
}

/**
 * @brief Create a C-Plane packet
 *  Transport layer fragmentation is not supported.
 *
 * @ingroup xran_cp_pkt
 *
 * @param mbuf
 *  A pointer to the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @param CC_ID
 *  Component Carrier ID for this C-Plane message
 * @param Ant_ID
 *  Antenna ID(RU Port ID) for this C-Plane message
 * @param seq_id
 *  Sequence ID for this C-Plane message
 * @return
 *  0 on success; non zero on failure
 */
int xran_prepare_ctrl_pkt(struct rte_mbuf *mbuf,
                        struct xran_cp_gen_params *params,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id)
{
  int ret;
  uint32_t payloadlen;
  struct xran_ecpri_hdr *ecpri_hdr;


    ecpri_hdr = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
    if(unlikely(ecpri_hdr == NULL)) {
        print_err("Fail to allocate the space for eCPRI hedaer!");
        return (-XRAN_ERRCODE_OUTOFMEMORY);
        }

    ecpri_hdr->ecpri_ver            = XRAN_ECPRI_VER;
    ecpri_hdr->ecpri_resv           = 0;     // should be zero
    ecpri_hdr->ecpri_concat         = 0;
    ecpri_hdr->ecpri_mesg_type      = ECPRI_RT_CONTROL_DATA;
    ecpri_hdr->ecpri_xtc_id         = xran_compose_cid(0, 0, CC_ID, Ant_ID);
    ecpri_hdr->ecpri_seq_id.seq_id  = seq_id;

    /* TODO: Transport layer fragmentation is not supported */
    ecpri_hdr->ecpri_seq_id.sub_seq_id  = 0;
    ecpri_hdr->ecpri_seq_id.e_bit       = 1;

    payloadlen = 0;

    ret = xran_append_radioapp_header(mbuf, params);
    if(ret < 0) {
        return (ret);
        }
    payloadlen += ret;

    ret = xran_append_control_section(mbuf, params);
    if(ret < 0) {
        return (ret);
        }
    payloadlen += ret;

//    printf("Total Payload length = %d\n", payloadlen);
    ecpri_hdr->ecpri_payl_size = rte_cpu_to_be_16(payloadlen);

    return (XRAN_ERRCODE_OK);
}

///////////////////////////////////////
// for Debug
int xran_parse_cp_pkt(struct rte_mbuf *mbuf, struct xran_cp_gen_params *result)
{
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_cp_radioapp_common_header *apphdr;
  int i, ret;
  int extlen;


    ret = 0;
    ecpri_hdr = rte_pktmbuf_mtod(mbuf, void *);
    if(ecpri_hdr == NULL) {
        print_err("Invalid packet - eCPRI hedaer!");
        return (-XRAN_ERRCODE_INVALIDPACKET);
        }

    /* Process eCPRI header. */
    if(ecpri_hdr->ecpri_ver != XRAN_ECPRI_VER) {
        print_err("Invalid eCPRI version - %d", ecpri_hdr->ecpri_ver);
        ret = -XRAN_ERRCODE_INVALIDPACKET;
        }

    if(ecpri_hdr->ecpri_resv != 0) {
        print_err("Invalid reserved field - %d", ecpri_hdr->ecpri_resv);
        ret = -XRAN_ERRCODE_INVALIDPACKET;
        }

    if(ecpri_hdr->ecpri_mesg_type != ECPRI_RT_CONTROL_DATA) {
        print_err("Not C-Plane Message - %d", ecpri_hdr->ecpri_mesg_type);
        ret = -XRAN_ERRCODE_INVALIDPACKET;
        }
#if 0
    printf("[CPlane] [%04X:%03d-%3d-%d] len=%5d\n",
            rte_be_to_cpu_16(ecpri_hdr->ecpri_xtc_id),
            ecpri_hdr->ecpri_seq_id.seq_id, ecpri_hdr->ecpri_seq_id.sub_seq_id,
            ecpri_hdr->ecpri_seq_id.e_bit,
            rte_be_to_cpu_16(ecpri_hdr->ecpri_payl_size));
#endif

    /* Process radio header. */
    apphdr = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_ecpri_hdr));
    if(apphdr == NULL) {
        print_err("Invalid packet - radio app hedaer!");
        return (-XRAN_ERRCODE_INVALIDPACKET);
        }

    *((uint32_t *)apphdr) = rte_cpu_to_be_32(*((uint32_t *)apphdr));

    if(apphdr->payloadVer != XRAN_PAYLOAD_VER) {
        print_err("Invalid Payload version - %d", apphdr->payloadVer);
        ret = -XRAN_ERRCODE_INVALIDPACKET;
        }

    result->dir             = apphdr->dataDirection;
    result->hdr.filterIdx   = apphdr->filterIndex;
    result->hdr.frameId     = apphdr->frameId;
    result->hdr.subframeId  = apphdr->subframeId;
    result->hdr.slotId      = apphdr->slotId;
    result->hdr.startSymId  = apphdr->startSymbolId;
    result->sectionType     = apphdr->sectionType;
    result->numSections     = apphdr->numOfSections;

    switch(apphdr->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: // Unused RB or Symbols in DL or UL, not supportted
            {
            struct xran_cp_radioapp_section0_header *hdr;
            struct xran_cp_radioapp_section0 *section;

                hdr = (struct xran_cp_radioapp_section0_header*)apphdr;

                result->hdr.fftSize     = rte_be_to_cpu_16(hdr->timeOffset);
                result->hdr.scs         = hdr->frameStructure.fftSize;
                result->hdr.timeOffset  = hdr->frameStructure.uScs;
                result->hdr.cpLength    = rte_be_to_cpu_16(hdr->cpLength);
                //hdr->reserved;    /* should be zero */
                
                section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section0_header));
                if(section == NULL) {
                    print_err("Invalid packet 0 - radio app hedaer!");
                    return (-XRAN_ERRCODE_INVALIDPACKET);
                    }
                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.sectionId;
                    result->sections[i].info.rb         = section->hdr.rb;
                    result->sections[i].info.symInc     = section->hdr.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s0.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s0.reMask;
                    //section->hdr.u.s0.reserved;   /* should be zero */

                    section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section0));
                    if(section == NULL) {
                        print_err("Invalid packet 0 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = (-XRAN_ERRCODE_INVALIDPACKET);
                        break;
                        }
                    }
            }
            break;

        case XRAN_CP_SECTIONTYPE_1: // Most DL/UL Radio Channels
            {
            struct xran_cp_radioapp_section1_header *hdr;
            struct xran_cp_radioapp_section1 *section;

                hdr = (struct xran_cp_radioapp_section1_header*)apphdr;

                result->hdr.iqWidth     = hdr->udComp.udIqWidth;
                result->hdr.compMeth    = hdr->udComp.udCompMeth;

                section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section1_header));
                if(section == NULL) {
                    print_err("Invalid packet 1 - radio app hedaer!");
                    return (-XRAN_ERRCODE_INVALIDPACKET);
                    }

                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.sectionId;
                    result->sections[i].info.rb         = section->hdr.rb;
                    result->sections[i].info.symInc     = section->hdr.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s1.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s1.reMask;
                    result->sections[i].info.beamId     = section->hdr.u.s1.beamId;
                    result->sections[i].info.ef         = section->hdr.u.s1.ef;

                    if(section->hdr.u.s1.ef) {
                        // TODO: handle section extension
                        extlen = 0;
                        }
                    else extlen = 0;

                    section = (void *)rte_pktmbuf_adj(mbuf,
                                    sizeof(struct xran_cp_radioapp_section1)+extlen);
                    if(section == NULL) {
                        print_err("Invalid packet 1 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = (-XRAN_ERRCODE_INVALIDPACKET);
                        break;
                        }
                    }
            }
            break;

        case XRAN_CP_SECTIONTYPE_3: // PRACH and Mixed-numerology Channels
            {
            struct xran_cp_radioapp_section3_header *hdr;
            struct xran_cp_radioapp_section3 *section;

                hdr = (struct xran_cp_radioapp_section3_header*)apphdr;

                result->hdr.timeOffset  = rte_be_to_cpu_16(hdr->timeOffset);
                result->hdr.scs         = hdr->frameStructure.uScs;
                result->hdr.fftSize     = hdr->frameStructure.fftSize;
                result->hdr.cpLength    = rte_be_to_cpu_16(hdr->cpLength);
                result->hdr.iqWidth     = hdr->udComp.udIqWidth;
                result->hdr.compMeth    = hdr->udComp.udCompMeth;

                section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section3_header));
                if(section == NULL) {
                    print_err("Invalid packet 3 - radio app hedaer!");
                    return (-XRAN_ERRCODE_INVALIDPACKET);
                    }

                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.sectionId;
                    result->sections[i].info.rb         = section->hdr.rb;
                    result->sections[i].info.symInc     = section->hdr.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s3.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s3.reMask;
                    result->sections[i].info.beamId     = section->hdr.u.s3.beamId;
                    result->sections[i].info.ef         = section->hdr.u.s3.ef;
                    result->sections[i].info.freqOffset = ((int32_t)rte_be_to_cpu_32(section->freqOffset))>>8;

                    if(section->reserved) {
                        print_err("Invalid packet 3 - section[%d:%d]", i, section->reserved);
                        ret = -XRAN_ERRCODE_INVALIDPACKET;
                        }

                    if(section->hdr.u.s3.ef) {
                        // TODO: handle section extension
                        extlen = 0;
                        }
                    else extlen = 0;

                    section = (void *)rte_pktmbuf_adj(mbuf,
                                    sizeof(struct xran_cp_radioapp_section3)+extlen);
                    if(section == NULL) {
                        print_err("Invalid packet 3 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = (-XRAN_ERRCODE_INVALIDPACKET);
                        break;
                        }
                    }
            }
            break;

        case XRAN_CP_SECTIONTYPE_5: // UE scheduling information, not supported
        case XRAN_CP_SECTIONTYPE_6: // Channel Information, not supported
        case XRAN_CP_SECTIONTYPE_7: // LAA, not supported
        default:
            ret = -XRAN_ERRCODE_INVALIDPARAM;
            print_err("Non-supported Section Type - %d", apphdr->sectionType);
        }

#if 0
    printf("[CP-%s] [%3d:%2d:%2d] section%d[%d] startSym=%d filterIdx=%X IQwidth=%d CompMeth=%d\n",
            result->dir?"DL":"UL",
            result->hdr.frameId, result->hdr.subframeId, result->hdr.slotId,
            result->sectionType, result->numSections,
            result->hdr.startSymId,
            result->hdr.filterIdx,
            result->hdr.iqWidth, result->hdr.compMeth);

    for(i=0; i<result->numSections; i++) {
        printf("  >> %3d:%04X| rb=%d symInc=%d numSym=%d startPrbc=%02X numPrbc=%d reMask=%03X beamId=%04X freqOffset=%d ef=%d\n",
            i, result->sections[i].info.id,
            result->sections[i].info.rb,
            result->sections[i].info.symInc, result->sections[i].info.numSymbol,
            result->sections[i].info.startPrbc, result->sections[i].info.numPrbc,
            result->sections[i].info.reMask,
            result->sections[i].info.beamId,
            result->sections[i].info.freqOffset,
            result->sections[i].info.ef);
//                    result->sections[i].info.type
        }
#endif

    return(ret);
}

