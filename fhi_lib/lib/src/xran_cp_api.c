/******************************************************************************
*
*   Copyright (c) 2020 Intel.
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
#include <immintrin.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>

#include "ethdi.h"
#include "xran_common.h"
#include "xran_transport.h"
#include "xran_cp_api.h"
#include "xran_printf.h"
#include "xran_compression.h"
#include "xran_dev.h"

PSECTION_DB_TYPE p_sectiondb[XRAN_PORTS_NUM] = {NULL, NULL, NULL, NULL,NULL, NULL, NULL, NULL};

static const uint8_t zeropad[XRAN_SECTIONEXT_ALIGN] = { 0, 0, 0, 0 };
static const uint8_t bitmask[] = { 0x00, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };


/**
 * @brief Initialize section database.
 *   Allocate required memory space to store section information.
 *   Each eAxC allocates dedicated storage and the entry size is the maximum number of sections.
 *   Total entry size : number of CC * number of antenna * max number of sections * 2(direction)
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_RESOURCE, if memory is not enough to allocate database area
 */
int32_t
xran_cp_init_sectiondb(void *pHandle)
{
    int32_t ctx, dir, cc, ant;
    struct xran_device_ctx* p_dev = NULL;
    uint8_t xran_port_id = 0;
    PSECTION_DB_TYPE p_sec_db =  NULL;
    struct xran_sectioninfo_db* p_sec_db_elm = NULL;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    if (p_sectiondb[xran_port_id] == NULL){
        p_sec_db = rte_zmalloc(NULL,sizeof(SECTION_DB_TYPE), 0);
        if(p_sec_db){
            p_sectiondb[xran_port_id] = p_sec_db;
            memset(p_sec_db, 0, sizeof(SECTION_DB_TYPE));
            print_dbg("xran_port_id %d %p\n",xran_port_id,  p_sectiondb[xran_port_id]);
            for (ctx = 0; ctx < XRAN_MAX_SECTIONDB_CTX; ctx++) {
                for (dir = 0; dir < XRAN_DIR_MAX; dir++) {
                    for (cc = 0; cc < p_dev->fh_cfg.nCC && cc < XRAN_COMPONENT_CARRIERS_MAX; cc++) {
                        for (ant = 0; ant < (p_dev->fh_cfg.neAxc*2 + p_dev->fh_cfg.nAntElmTRx) && ant < (XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR); ant++) {
                            p_sec_db_elm = (struct xran_sectioninfo_db*)rte_zmalloc(NULL,sizeof(struct xran_sectioninfo_db), 0);
                            if(p_sec_db_elm) {
                                memset(p_sec_db_elm, 0, sizeof(struct xran_sectioninfo_db));
                                p_sec_db->p_sectiondb_elm[ctx][dir][cc][ant] = p_sec_db_elm;
                            } else {
                                print_err("Memory Allocation Failed [port %d sz %ld]\n", xran_port_id, sizeof(struct xran_sectioninfo_db));
                                return (XRAN_STATUS_RESOURCE);
                            }
                        }
                    }
                }
            }
        } else {
            print_err("Memory Allocation Failed [port %d sz %ld]\n", xran_port_id, sizeof(SECTION_DB_TYPE));
            return (XRAN_STATUS_RESOURCE);
        }
    }else {
        p_sec_db = p_sectiondb[xran_port_id];
    }

    for(ctx=0; ctx < XRAN_MAX_SECTIONDB_CTX; ctx++)
        for(dir=0; dir < XRAN_DIR_MAX; dir++)
            for (cc = 0; cc < p_dev->fh_cfg.nCC && cc < XRAN_COMPONENT_CARRIERS_MAX; cc++)
                for (ant = 0; ant < (p_dev->fh_cfg.neAxc*2 + p_dev->fh_cfg.nAntElmTRx) && ant < (XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR); ant++)
                    if(p_sec_db->p_sectiondb_elm[ctx][dir][cc][ant])
                        p_sec_db->p_sectiondb_elm[ctx][dir][cc][ant]->cur_index = 0;

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Release and free section database
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @return
 *  XRAN_STATUS_SUCCESS on success
 */
int32_t
xran_cp_free_sectiondb(void *pHandle)
{
    int32_t ctx, dir, cc, ant;
    struct xran_device_ctx* p_dev = NULL;
    uint8_t xran_port_id = 0;
    PSECTION_DB_TYPE p_sec_db =  NULL;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (XRAN_STATUS_FAIL);
    }

    if (p_sectiondb[xran_port_id] == NULL){
        return (XRAN_STATUS_INVALID_PARAM);
    } else {
        p_sec_db = p_sectiondb[xran_port_id];
        for (ctx = 0; ctx < XRAN_MAX_SECTIONDB_CTX; ctx++) {
            for (dir = 0; dir < XRAN_DIR_MAX; dir++) {
                for (cc = 0; cc < XRAN_COMPONENT_CARRIERS_MAX; cc++) {
                    for (ant = 0; ant < (XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR); ant++) {
                        if(p_sec_db->p_sectiondb_elm[ctx][dir][cc][ant])
                            rte_free(p_sec_db->p_sectiondb_elm[ctx][dir][cc][ant]);
                    }
                }
            }
        }
        rte_free(p_sec_db);
        p_sectiondb[xran_port_id] = NULL;
    }

    return (XRAN_STATUS_SUCCESS);
}

static inline struct xran_sectioninfo_db *
xran_get_section_db(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  struct xran_sectioninfo_db *ptr;
    struct xran_device_ctx* p_dev = NULL;
    uint8_t xran_port_id = 0;
    PSECTION_DB_TYPE p_sec_db =  NULL;

    if(pHandle) {
        p_dev = (struct xran_device_ctx* )pHandle;
        xran_port_id = p_dev->xran_port_id;
    } else {
        print_err("Invalid pHandle - %p", pHandle);
        return (NULL);
    }

    if(unlikely(xran_port_id >= XRAN_PORTS_NUM)) {
        print_err("Invalid Port id - %d", p_dev->xran_port_id);
        return (NULL);
    }

    if (p_sectiondb[xran_port_id] == NULL){
        print_err("p_sectiondb xran_port %d\n", xran_port_id);
        return (NULL);
    }else {
        p_sec_db = p_sectiondb[xran_port_id];
    }
    if(unlikely(ctx_id >= XRAN_MAX_SECTIONDB_CTX)) {
        print_err("Invalid Context id - %d", ctx_id);
        return (NULL);
        }

    if(unlikely(dir >= XRAN_DIR_MAX)) {
        print_err("Invalid direction - %d", dir);
        return (NULL);
        }

    if(unlikely(cc_id >= XRAN_COMPONENT_CARRIERS_MAX)) {
        print_err("Invalid CC id - %d", cc_id);
        return (NULL);
        }

    if(unlikely(ruport_id >= XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR)) {
        print_err("Invalid eAxC id - %d", ruport_id);
        return (NULL);
        }

    ptr = p_sec_db->p_sectiondb_elm[ctx_id][dir][cc_id][ruport_id];

    return(ptr);
}

static inline struct xran_section_info *
xran_get_section_info(struct xran_sectioninfo_db *ptr, uint16_t index)
{
    if(unlikely(ptr == NULL))
        return (NULL);

    if(unlikely(index > XRAN_MAX_NUM_SECTIONS)) {
        print_err("Index is out of range - %d", index);
        return (NULL);
        }

    return(&(ptr->list[index]));
}

/**
 * @brief Add a section information of C-Plane to dabase.
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @param dir
 *  Direction of C-Plane message for the section to store
 * @param cc_id
 *  CC ID of C-Plane message for the section to store
 * @param ruport_id
 *  RU port ID of C-Plane message for the section to store
 * @param ctx_id
 *  Context index for the section database
 * @param info
 *  The information of this section to store
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARAM, if direction, CC ID or RU port ID is incorrect
 *  XRAN_STATUS_RESOURCE, if no more space to add on database
 */
int32_t
xran_cp_add_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id, struct xran_section_info *info)
{
  struct xran_sectioninfo_db *ptr;
  struct xran_section_info *list;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL)) {
        return (XRAN_STATUS_INVALID_PARAM);
        }

    if(unlikely(ptr->cur_index >= XRAN_MAX_NUM_SECTIONS)) {
        print_err("No more space to add section information!");
        return (XRAN_STATUS_RESOURCE);
        }

    list = xran_get_section_info(ptr, ptr->cur_index);
    if (list)
        memcpy(list, info, sizeof(struct xran_section_info));
    else
    {
        print_err("Null list in section db\n!");
        return (XRAN_STATUS_INVALID_PARAM);
    }

    ptr->cur_index++;

    return (XRAN_STATUS_SUCCESS);
}


struct xran_section_info *
xran_cp_get_section_info_ptr(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
    struct xran_sectioninfo_db *ptr;
    struct xran_section_info *list;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL)) {
        return NULL;
        }

    if(unlikely(ptr->cur_index >= XRAN_MAX_NUM_SECTIONS)) {
        print_err("No more space to add section information!");
        return NULL;
        }

    list = xran_get_section_info(ptr, ptr->cur_index);
    if (list)
    {
        ptr->cur_index++;
        return list;
    }
    else
    {
        print_err("Null list in section db\n!");
        return NULL;
    }

}



int32_t
xran_cp_add_multisection_info(void *pHandle, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id, struct xran_cp_gen_params *gen_info)
{
    int32_t i;
  uint8_t dir, num_sections;
  struct xran_sectioninfo_db *ptr;
  struct xran_section_info *list;

    dir             = gen_info->dir;
    num_sections    = gen_info->numSections;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL)) {
        return (XRAN_STATUS_INVALID_PARAM);
        }

    if(unlikely(ptr->cur_index+num_sections >= XRAN_MAX_NUM_SECTIONS)) {
        print_err("No more space to add section information!");
        return (XRAN_STATUS_RESOURCE);
        }

    list = xran_get_section_info(ptr, ptr->cur_index);
    if (list)
    {
    for(i=0; i<num_sections; i++) {
            memcpy(&list[i], gen_info->sections[i].info, sizeof(struct xran_section_info));
        ptr->cur_index++;
        }
    }
    else
    {
        print_err("Null list in section db\n!");
        return (XRAN_STATUS_INVALID_PARAM);
    }

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Find a section information of C-Plane from database
 *   by given information
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @param dir
 *  The direction of the section to find
 * @param cc_id
 *  The CC ID of the section to find
 * @param ruport_id
 *  RU port ID of the section to find
 * @param ctx_id
 *  Context index for the section database
 * @param section_id
 *  The ID of section to find
 * @return
 *  The pointer of section information if matched section is found
 *  NULL if failed to find matched section
 */
struct xran_section_info *
xran_cp_find_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id, uint16_t section_id)
{
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL))
        return (NULL);

    if(section_id > ptr->cur_index || section_id < 0)
    {
        print_err("No section ID in the list - %d, ptr->cur_index is %d", section_id, ptr->cur_index);
            }
    return (xran_get_section_info(ptr, section_id));
}

/**
 * @brief Iterate each section information of C-Plane
 *  from the database of eAxC by given information
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @param dir
 *  The direction of the section to find
 * @param cc_id
 *  The CC ID of the section to find
 * @param ruport_id
 *  RU port ID of the section to find
 * @param ctx_id
 *  Context index for the section database
 * @param next
 *  The pointer to store the position of next entry
 * @return
 *  The pointer of section information in the list
 *  NULL if reached at the end of the list
 */
struct xran_section_info *
xran_cp_iterate_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id, uint32_t *next)
{
    int32_t index;
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL))
        return (NULL);

    index = *next;
    if(*next < ptr->cur_index) {
        (*next)++;
        return (xran_get_section_info(ptr, index));
        }
    else {
        print_dbg("No more sections in the list");
        return (NULL);
        }
}

/**
 * @brief Get the size of stored entries
 *  for the database of eAxC by given information
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @param dir
 *  The direction of the section to find
 * @param cc_id
 *  The CC ID of the section to find
 * @param ruport_id
 *  RU port ID of the section to find
 * @param ctx_id
 *  Context index for the section database
 * @return
 *  The size of stored entries
 *  -1 if failed to find matched database
 */
int32_t
xran_cp_getsize_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL))
        return (-1);

    return (ptr->cur_index);
}

/**
 * @brief Reset a database of eAxC by given information
 *
 * @ingroup xran_cp_pkt
 *
 * @param pHandle
 *  handle for xRAN interface, currently not being used
 * @param dir
 *  The direction of the section to find
 * @param cc_id
 *  The CC ID of the section to find
 * @param ruport_id
 *  RU port ID of the section to find
 * @param ctx_id
 *  Context index for the section database
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if failed to find matched database
 */
int32_t
xran_cp_reset_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL)) {
        return (XRAN_STATUS_INVALID_PARAM);
        }

    ptr->cur_index = 0;

    return (XRAN_STATUS_SUCCESS);
}

int32_t xran_cp_populate_section_ext_1(int8_t  *p_ext1_dst,    /**< destination buffer */
                                       uint16_t  ext1_dst_len, /**< dest buffer size */
                                       int16_t  *p_bfw_iq_src, /**< source buffer of IQs */
                                       struct xran_prb_elm *p_pRbMapElm)
{
    struct xran_cp_radioapp_section_ext1 *p_ext1;
    uint8_t *p_bfw_content = NULL;
    int32_t parm_size   = 0;
    int32_t bfw_iq_bits = 0;
    int32_t total_len   = 0;
    uint16_t idxSection  = 0;
    int32_t section_len = 0;
    int16_t numCPSections = (p_pRbMapElm->bf_weight.numSetBFWs == 0 ? 1 : p_pRbMapElm->bf_weight.numSetBFWs);

    int16_t cur_ext_len = 0;
    int8_t  *p_ext1_dst_cur = NULL;
    int16_t  bfwNumPerRb = p_pRbMapElm->bf_weight.nAntElmTRx;
    uint8_t   bfwiqWidth = p_pRbMapElm->bf_weight.bfwIqWidth;
    uint8_t   bfwCompMeth = p_pRbMapElm->bf_weight.bfwCompMeth;
    struct xran_cp_radioapp_section1 *p_section1;

    struct xranlib_compress_request  bfp_com_req;
    struct xranlib_compress_response bfp_com_rsp;

    memset(&bfp_com_req, 0, sizeof(struct xranlib_compress_request));
    memset(&bfp_com_rsp, 0, sizeof(struct xranlib_compress_response));

    print_dbg("%s comp %d\n", __FUNCTION__, bfwCompMeth);
    print_dbg("bfwNumPerRb %d bfwiqWidth %d\n", bfwNumPerRb, bfwiqWidth);

    if(p_ext1_dst)
        p_ext1_dst_cur = p_ext1_dst;
    else
        return (XRAN_STATUS_INVALID_PARAM);

    /* create section for each PRB bundle */
    for (idxSection = 0; idxSection < numCPSections ; idxSection++) {
        print_dbg("%s Section %d\n", __FUNCTION__, idxSection);

        if(total_len >= ext1_dst_len){
            print_err("p_ext1_dst overflow\n");
            return XRAN_STATUS_RESOURCE;
        }

        cur_ext_len = 0;
        p_section1 = (struct xran_cp_radioapp_section1 *)p_ext1_dst_cur;
        if(p_section1 == NULL) {
            print_err("p_section is null!\n");
            return (XRAN_STATUS_INVALID_PARAM);
        }

        section_len = sizeof(struct xran_cp_radioapp_section1);
        
        p_ext1_dst_cur = p_ext1_dst_cur + section_len;
        total_len += section_len;

        parm_size = sizeof(struct xran_cp_radioapp_section_ext1);
        p_ext1 = (struct xran_cp_radioapp_section_ext1 *)p_ext1_dst_cur;
        if(p_ext1 == NULL) {
            print_err("p_ext1 is null!\n");
            return (XRAN_STATUS_INVALID_PARAM);
        }

        cur_ext_len += parm_size;

        p_ext1->extType       = XRAN_CP_SECTIONEXTCMD_1;
        p_ext1->ef            = XRAN_EF_F_LAST; //only one ext-1 per CP section
        p_ext1->bfwCompMeth   = bfwCompMeth;
        p_ext1->bfwIqWidth    = XRAN_CONVERT_BFWIQWIDTH(bfwiqWidth);

        switch(bfwCompMeth) {
            case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                p_bfw_content = (uint8_t *)(p_ext1+1);
                if(p_bfw_content == NULL) {
                    print_err("Fail to allocate the space for section extension 1");
                    return (XRAN_STATUS_RESOURCE);
                }
                bfp_com_req.data_in         = (int16_t*)p_bfw_iq_src;
                bfp_com_req.numRBs          = 1;
                bfp_com_req.numDataElements = bfwNumPerRb*2;
                bfp_com_req.len             = bfwNumPerRb*4;
                bfp_com_req.compMethod      = p_ext1->bfwCompMeth;
                bfp_com_req.iqWidth         = p_ext1->bfwIqWidth;

                print_dbg("req 0x%08p iqWidth %d\n",bfp_com_req.data_in, bfp_com_req.iqWidth);

                parm_size = 1; /* (reserved + exponent) as part of bfwCompParam 1 octet */
                break;
            case XRAN_BFWCOMPMETHOD_BLKSCALE:
                rte_panic("XRAN_BFWCOMPMETHOD_BLKSCALE");
                break;

            case XRAN_BFWCOMPMETHOD_ULAW:
                rte_panic("XRAN_BFWCOMPMETHOD_ULAW");
                break;

            case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                rte_panic("XRAN_BFWCOMPMETHOD_BEAMSPACE");
                break;

            case XRAN_BFWCOMPMETHOD_NONE:
            default:
                p_bfw_content = (uint8_t *)(p_ext1+1);
                /* bfwCompParam is absent for no compression case */
                parm_size = 0;
        }

        if(p_bfw_content == NULL) {
            print_err("Fail to allocate the space for section extension 1");
            return (XRAN_STATUS_RESOURCE);
            }

        bfw_iq_bits = bfwNumPerRb* bfwiqWidth * 2;

        parm_size += bfw_iq_bits>>3;
        if(bfw_iq_bits%8)
            parm_size++;

        print_dbg("copy BF W %p -> %p size %d \n", p_bfw_iq_src, p_bfw_content, parm_size);

        if (p_ext1->bfwCompMeth == XRAN_BFWCOMPMETHOD_NONE){ //5.4.7.1.1 
            memcpy(p_bfw_content, p_bfw_iq_src, parm_size);
        } else {
            bfp_com_rsp.data_out = (int8_t*)p_bfw_content;
            if(xranlib_compress_bfw(&bfp_com_req, &bfp_com_rsp) == 0){
                print_dbg("comp_len %d parm_size %d\n", bfp_com_rsp.len, parm_size);
            } else {
                print_err("compression failed\n");
                return (XRAN_STATUS_FAIL);
            }
        }

        p_bfw_content = (uint8_t *)(p_bfw_content + parm_size);

        cur_ext_len += parm_size;
        parm_size = cur_ext_len % XRAN_SECTIONEXT_ALIGN;
        if(parm_size) {
            parm_size = XRAN_SECTIONEXT_ALIGN - parm_size;
            memcpy(p_bfw_content, zeropad, RTE_MIN(parm_size, sizeof(zeropad)));
            p_bfw_content += parm_size;
            cur_ext_len += parm_size;
            print_dbg("zeropad %d cur_ext_len %d\n", parm_size, cur_ext_len);
        }

        if(cur_ext_len % XRAN_SECTIONEXT_ALIGN)
            rte_panic("ext1 should be aligned on 4-bytes boundary");

        p_ext1->extLen = cur_ext_len / XRAN_SECTIONEXT_ALIGN;
        print_dbg("%p iq %p p_ext1->extLen %d\n",p_ext1, p_ext1+1,  p_ext1->extLen);

        /* update for next RB */
        p_ext1_dst_cur += cur_ext_len;
        p_bfw_iq_src   = p_bfw_iq_src + bfwNumPerRb*2;

        total_len += cur_ext_len;
    } /*for(idxSection < numCPSections */

    print_dbg("total_len %d\n", total_len);
    return (total_len);
}

// TODO: Need to decide to add calculation or not
// Frequency offset 5.4.5.11
//   frequency_offset = freqOffset * SCS * 0.5
//    i.e freqOffset = (frequency_offset *2 )/ SCS ?
inline int32_t
xran_get_freqoffset(int32_t freqOffset, int32_t scs)
{
    return (freqOffset);
}

static int32_t
xran_append_sectionext_1(struct rte_mbuf *mbuf, struct xran_sectionext1_info *params, int32_t last_flag)
{
    int32_t total_len = 0;

    if(params->bfwIQ_sz) {
        int8_t *p_dst = (int8_t *)rte_pktmbuf_append(mbuf, params->bfwIQ_sz);

        if(p_dst == NULL) {
            print_err("Fail to allocate the space for section extension 1 [%d]", params->bfwIQ_sz);
            return (XRAN_STATUS_RESOURCE);
        }

        /* extType1 with all the headers created by xran_cp_populate_section_ext_1() earlier */
        total_len = params->bfwIQ_sz;
    }

    return (total_len);
}

static int32_t
xran_prepare_sectionext_2(struct rte_mbuf *mbuf, struct xran_sectionext2_info *params, int32_t last_flag)
{
  struct xran_cp_radioapp_section_ext2 *ext2;
  uint8_t *data;
    int32_t total_len;
    int32_t parm_size;
  uint32_t val, shift_val;
    int32_t val_size, pad_size;

    total_len = 0;

    parm_size = sizeof(struct xran_cp_radioapp_section_ext2);
    ext2 = (struct xran_cp_radioapp_section_ext2 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext2 == NULL) {
        print_err("Fail to allocate the space for section extension 2");
        return (XRAN_STATUS_RESOURCE);
        }
    total_len += parm_size;

    ext2->extType           = XRAN_CP_SECTIONEXTCMD_2;
    ext2->ef                = last_flag;
    ext2->bfZe3ddWidth      = params->bfZe3ddWidth;
    ext2->bfAz3ddWidth      = params->bfAz3ddWidth;
    ext2->bfZePtWidth       = params->bfZePtWidth;
    ext2->bfAzPtWidth       = params->bfAzPtWidth;
    ext2->bfaCompResv0      = 0;
    ext2->bfaCompResv1      = 0;

    val = 0;
    shift_val = 0;
    if(params->bfAzPtWidth) {
        val += params->bfAzPt & bitmask[params->bfAzPtWidth];
        shift_val += 8 - (params->bfAzPtWidth+1);
    } else
        shift_val += 8;

    if(params->bfZePtWidth) {
        val = val << (params->bfZePtWidth+1);
        val += params->bfZePt & bitmask[params->bfZePtWidth];
        shift_val += 8 - (params->bfZePtWidth+1);
    } else
        shift_val += 8;

    if(params->bfAz3ddWidth) {
        val = val << (params->bfAz3ddWidth+1);
        val += params->bfAz3dd & bitmask[params->bfAz3ddWidth];
        shift_val += 8 - (params->bfAz3ddWidth+1);
    } else
        shift_val += 8;

    if(params->bfZe3ddWidth) {
        val = val << (params->bfZe3ddWidth+1);
        val += params->bfZe3dd & bitmask[params->bfZe3ddWidth];
        shift_val += 8 - (params->bfZe3ddWidth+1);
    } else
        shift_val += 8;

    if(val) {
        val = val << shift_val;
        val = rte_cpu_to_be_32(val);
        }

    val_size = 4 - (shift_val/8);   /* ceil(total bit/8) */
    parm_size = val_size + 1;       /* additional 1 byte for bfxxSI */

    // alignment
    total_len += parm_size;
    pad_size = total_len % XRAN_SECTIONEXT_ALIGN;
    if(pad_size) {
        pad_size = XRAN_SECTIONEXT_ALIGN - pad_size;
        parm_size += pad_size;
        total_len += pad_size;
        }

    data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
    if(data == NULL) {
        print_err("Fail to allocate the space for section extension 2");
        return (XRAN_STATUS_RESOURCE);
        }

    memcpy(data, &val, val_size);
    data += val_size;
    *data = ((params->bfAzSI) << 3) + (params->bfZeSI);
    data++;
    memcpy(data, zeropad, pad_size);

    ext2->extLen = total_len / XRAN_SECTIONEXT_ALIGN;
    *(uint32_t *)ext2 = rte_cpu_to_be_32(*(uint32_t *)ext2);

    return (total_len);
}

static int32_t
xran_prepare_sectionext_3(struct rte_mbuf *mbuf, struct xran_sectionext3_info *params, int32_t last_flag)
{
    int32_t total_len;
    int32_t adj;
    int32_t data_first_byte, data_second_byte;
    int32_t data_third_byte, data_fourth_byte;
    int32_t extLen;

    if(params->layerId == XRAN_LAYERID_0
        || params->layerId == XRAN_LAYERID_TXD) {   /* first data layer */

        union xran_cp_radioapp_section_ext3_first *ext3_f;
        uint64_t *tmp;

        total_len = sizeof(union xran_cp_radioapp_section_ext3_first);
        ext3_f = (union xran_cp_radioapp_section_ext3_first *)rte_pktmbuf_append(mbuf, total_len);
        if(ext3_f == NULL) {
            print_err("Fail to allocate the space for section extension 3");
            return (XRAN_STATUS_RESOURCE);
            }

        /*ext3_f->data_field.data_field1 = _mm_setzero_si128();

        ext3_f->all_bits.layerId         = params->layerId;
        ext3_f->all_bits.ef              = last_flag;
        ext3_f->all_bits.extType         = XRAN_CP_SECTIONEXTCMD_3;
        ext3_f->all_bits.crsSymNum       = params->crsSymNum;
        ext3_f->all_bits.crsShift        = params->crsShift;
        ext3_f->all_bits.crsReMask       = params->crsReMask;
        ext3_f->all_bits.txScheme        = params->txScheme;
        ext3_f->all_bits.numLayers       = params->numLayers;
        ext3_f->all_bits.codebookIndex   = params->codebookIdx;

        if(params->numAntPort == 2) {
            ext3_f->all_bits.beamIdAP3   = params->beamIdAP1;
            ext3_f->all_bits.extLen      = 3;
            adj = 4;
            total_len -= adj;
            }
        else {
            ext3_f->all_bits.beamIdAP3   = params->beamIdAP1;
            ext3_f->all_bits.beamIdAP2   = params->beamIdAP2;
            ext3_f->all_bits.beamIdAP1   = params->beamIdAP3;
            ext3_f->all_bits.extLen      = 4;
            adj = 0;
            }*/

        if(params->numAntPort == 2) {
            data_third_byte = 0;
            extLen = 3;
            adj = 4;
            total_len -= adj;
        }else
        {
            data_third_byte = (params->beamIdAP2 << 16) | params->beamIdAP3;
            extLen = 4;
            adj = 0;
            }

        data_first_byte  = (params->txScheme << xran_cp_radioapp_sec_ext3_TxScheme)
                         | (params->crsReMask << xran_cp_radioapp_sec_ext3_CrcReMask)
                         | (params->crsShift << xran_cp_radioapp_sec_ext3_CrcShift)
                         | (params->crsSymNum << xran_cp_radioapp_sec_ext3_CrcSymNum);
        data_second_byte = (last_flag << xran_cp_radioapp_sec_ext3_EF)
                         | (XRAN_CP_SECTIONEXTCMD_3 << xran_cp_radioapp_sec_ext3_ExtType)
                         | (extLen << xran_cp_radioapp_sec_ext3_ExtLen)
                         | (params->codebookIdx << xran_cp_radioapp_sec_ext3_CodebookIdx)
                         | (params->layerId << xran_cp_radioapp_sec_ext3_LayerId)
                         | (params->numLayers << xran_cp_radioapp_sec_ext3_NumLayers);
        data_fourth_byte  = params->beamIdAP1;
        ext3_f->data_field.data_field1 = _mm_set_epi32(data_fourth_byte, data_third_byte, data_second_byte, data_first_byte);

        /* convert byte order */
        tmp = (uint64_t *)ext3_f;
        *tmp = rte_cpu_to_be_64(*tmp); tmp++;
        *tmp = rte_cpu_to_be_64(*tmp);

        if(adj)
            rte_pktmbuf_trim(mbuf, adj);
        }
    else {  /* non-first data layer */
        union xran_cp_radioapp_section_ext3_non_first *ext3_nf;

        total_len = sizeof(union xran_cp_radioapp_section_ext3_non_first);
        ext3_nf = (union xran_cp_radioapp_section_ext3_non_first *)rte_pktmbuf_append(mbuf, total_len);
        if(ext3_nf == NULL) {
            print_err("Fail to allocate the space for section extension 3");
            return (XRAN_STATUS_RESOURCE);
            }

        /*ext3_nf->all_bits.layerId        = params->layerId;
        ext3_nf->all_bits.ef             = last_flag;
        ext3_nf->all_bits.extType        = XRAN_CP_SECTIONEXTCMD_3;
        ext3_nf->all_bits.numLayers      = params->numLayers;
        ext3_nf->all_bits.codebookIndex  = params->codebookIdx;

        ext3_nf->all_bits.extLen         = sizeof(union xran_cp_radioapp_section_ext3_non_first)/XRAN_SECTIONEXT_ALIGN;*/

        ext3_nf->data_field = (last_flag << xran_cp_radioapp_sec_ext3_EF)
                            | (XRAN_CP_SECTIONEXTCMD_3 << xran_cp_radioapp_sec_ext3_ExtType)
                            | ((sizeof(union xran_cp_radioapp_section_ext3_non_first)/XRAN_SECTIONEXT_ALIGN) << xran_cp_radioapp_sec_ext3_ExtLen)
                            | (params->codebookIdx << xran_cp_radioapp_sec_ext3_CodebookIdx)
                            | (params->layerId << xran_cp_radioapp_sec_ext3_LayerId)
                            | (params->numLayers << xran_cp_radioapp_sec_ext3_NumLayers);

        *(uint32_t *)ext3_nf = rte_cpu_to_be_32(*(uint32_t *)ext3_nf);
        }

    return (total_len);
}

static int32_t
xran_prepare_sectionext_4(struct rte_mbuf *mbuf, struct xran_sectionext4_info *params, int32_t last_flag)
{
  struct xran_cp_radioapp_section_ext4 *ext4;
    int32_t parm_size;

    parm_size = sizeof(struct xran_cp_radioapp_section_ext4);
    ext4 = (struct xran_cp_radioapp_section_ext4 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext4 == NULL) {
        print_err("Fail to allocate the space for section extension 4");
        return(XRAN_STATUS_RESOURCE);
        }

        ext4->extType       = XRAN_CP_SECTIONEXTCMD_4;
        ext4->ef            = last_flag;
        ext4->modCompScaler = params->modCompScaler;
        ext4->csf           = params->csf?1:0;
    ext4->extLen        = parm_size / XRAN_SECTIONEXT_ALIGN;

        *(uint32_t *)ext4 = rte_cpu_to_be_32(*(uint32_t*)ext4);

    return (parm_size);
}

static int32_t
xran_prepare_sectionext_9(struct rte_mbuf *mbuf, struct xran_sectionext9_info * params, int32_t last_flag)
{
    struct xran_cp_radioapp_section_ext9 *ext9;
    int32_t parm_size;

    parm_size = sizeof(struct xran_cp_radioapp_section_ext9);
    ext9 = (struct xran_cp_radioapp_section_ext9 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext9 == NULL) {
        print_err("Fail to allocate the space for section extension 9");
        return(XRAN_STATUS_RESOURCE);
    }

    ext9->extType       = XRAN_CP_SECTIONEXTCMD_9;
    ext9->ef            = last_flag;
    ext9->extLen        = parm_size / XRAN_SECTIONEXT_ALIGN;
    ext9->technology    = params->technology;
    ext9->reserved      = params->reserved;
    
    *(uint32_t *)ext9 = rte_cpu_to_be_32(*(uint32_t*)ext9);

    return (parm_size);
}

static int32_t
xran_prepare_sectionext_5(struct rte_mbuf *mbuf, struct xran_sectionext5_info *params, int32_t last_flag)
{
  struct xran_cp_radioapp_section_ext_hdr *ext_hdr;
  struct xran_cp_radioapp_section_ext5 ext5;
    int32_t padding;
    int32_t total_len;
  uint8_t *data;
    int32_t i;

    if(params->num_sets > XRAN_MAX_MODCOMP_ADDPARMS) {
        print_err("Exceeds maximum number of parameters(%d). Skipping.", params->num_sets);
        return (0);
        }

    total_len = sizeof(struct xran_cp_radioapp_section_ext_hdr)
                + (sizeof(struct xran_cp_radioapp_section_ext5)*params->num_sets)/2
                - (params->num_sets>>1); // 8bits are added by every two sets, so needs to adjust

    /* for alignment */
    padding = total_len % XRAN_SECTIONEXT_ALIGN;
    if(padding) {
        padding = XRAN_SECTIONEXT_ALIGN - padding;
        total_len += padding;
        }

    ext_hdr = (struct xran_cp_radioapp_section_ext_hdr *)rte_pktmbuf_append(mbuf, total_len);
    if(ext_hdr == NULL) {
        print_err("Fail to allocate the space for section extension 5");
        return (XRAN_STATUS_RESOURCE);
        }

    ext_hdr->extType    = XRAN_CP_SECTIONEXTCMD_5;
    ext_hdr->ef         = last_flag;
    ext_hdr->extLen     = total_len / XRAN_SECTIONEXT_ALIGN;

    *(uint16_t *)ext_hdr    = rte_cpu_to_be_16(*((uint16_t *)ext_hdr));

    data = (uint8_t *)(ext_hdr + 1);
    i = 0;
    while(i < params->num_sets) {
        if(i%2) { // odd index
            ext5.mcScaleOffset2 = params->mc[i].mcScaleOffset;
            ext5.csf2           = params->mc[i].csf;
            ext5.mcScaleReMask2 = params->mc[i].mcScaleReMask;
            ext5.reserved0      = 0;
            i++;

            // adding two sets at once (due to the definition of structure)
            *((uint64_t *)&ext5) = rte_cpu_to_be_64(*((uint64_t *)&ext5));
            memcpy(data, &ext5, sizeof(struct xran_cp_radioapp_section_ext5));
            data += sizeof(struct xran_cp_radioapp_section_ext5);
            }
        else { // even index
            ext5.mcScaleOffset1 = params->mc[i].mcScaleOffset;
            ext5.csf1           = params->mc[i].csf;
            ext5.mcScaleReMask1 = params->mc[i].mcScaleReMask;
            ext5.mcScaleReMask2 = 0;
            i++;

            if(i == params->num_sets) { // adding last even index
                *((uint64_t *)&ext5) = rte_cpu_to_be_64(*((uint64_t *)&ext5));
                memcpy(data, &ext5, sizeof(struct xran_cp_radioapp_section_ext5)/2);
                data += sizeof(struct xran_cp_radioapp_section_ext5)/2;
                break;
                }
            }
        }

    /* zero padding */
    if(padding)
        memcpy(data, zeropad, padding);

    return (total_len);
}

static int32_t
xran_prepare_sectionext_6(struct rte_mbuf *mbuf,
                struct xran_sectionext6_info *params, int32_t last_flag)
{
    union xran_cp_radioapp_section_ext6 *ext6;
    int32_t parm_size;

    parm_size = sizeof(union xran_cp_radioapp_section_ext6);
    ext6 = (union xran_cp_radioapp_section_ext6 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext6 == NULL) {
        print_err("Fail to allocate the space for section extension 6");
        return(XRAN_STATUS_RESOURCE);
        }

    ext6->data_field.data_field1 = 0x0LL;
    ext6->all_bits.extType       = XRAN_CP_SECTIONEXTCMD_6;
    ext6->all_bits.ef            = last_flag;
    ext6->all_bits.rbgSize       = params->rbgSize;
    ext6->all_bits.rbgMask       = params->rbgMask;
    ext6->all_bits.symbolMask    = params->symbolMask;
    ext6->all_bits.extLen        = parm_size / XRAN_SECTIONEXT_ALIGN;
    //ext6->reserved0     = 0;
    //ext6->reserved1     = 0;

    *(uint64_t *)ext6 = rte_cpu_to_be_64(*(uint64_t*)ext6);

    return (parm_size);
}

static int32_t
xran_prepare_sectionext_10(struct rte_mbuf *mbuf,
                struct xran_sectionext10_info *params, int32_t last_flag)
{
  union xran_cp_radioapp_section_ext10 *ext10;
  int32_t parm_size;
  int32_t total_len;
  int32_t padding;
  int32_t i;
  uint16_t *id_ptr;


#if (XRAN_STRICT_PARM_CHECK)
    if(params->beamGrpType != XRAN_BEAMGT_COMMON
        && params->beamGrpType != XRAN_BEAMGT_MATRIXIND
        && params->beamGrpType != XRAN_BEAMGT_VECTORLIST) {
        print_err("Invalid beam group Type - %d\n", params->beamGrpType);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif
    /* should be checked since it will be used for the index of array */
    if(params->numPortc > XRAN_MAX_NUMPORTC_EXT10) {
        print_err("Invalid Number of eAxC in extension 10 - %d\n", params->numPortc);
        return (XRAN_STATUS_INVALID_PARAM);
        }

    parm_size = sizeof(union xran_cp_radioapp_section_ext10);
    ext10 = (union xran_cp_radioapp_section_ext10 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext10 == NULL) {
        print_err("Fail to allocate the space for section extension 10");
        return(XRAN_STATUS_RESOURCE);
        }

    ext10->all_bits.extType          = XRAN_CP_SECTIONEXTCMD_10;
    ext10->all_bits.ef               = last_flag;
    ext10->all_bits.numPortc         = params->numPortc;
    ext10->all_bits.beamGroupType    = params->beamGrpType;
    ext10->all_bits.reserved         = 0;

    total_len = parm_size;

    if(params->beamGrpType == XRAN_BEAMGT_VECTORLIST) {
        /* Calculate required size, it needs to be reduced by one byte
         * since beam ID starts from reserved field(fourth octet). */
        parm_size = params->numPortc * 2 - 1;

        /* for alignment */
        padding = (parm_size + total_len) % XRAN_SECTIONEXT_ALIGN;
        if(padding) {
            padding = XRAN_SECTIONEXT_ALIGN - padding;
            parm_size += padding;
            }

        id_ptr = (uint16_t *)rte_pktmbuf_append(mbuf, parm_size);
        if(id_ptr == NULL) {
            print_err("Fail to allocate the space for beam IDs in section extension 10");
            return(XRAN_STATUS_RESOURCE);
            }

        /* Need to advance pointer by one-byte since beam IDs start from fourth octet */
        id_ptr = (uint16_t *)(((uint8_t *)id_ptr) - 1);

        /* this might not be optimal since the alignment is broken */
        for(i = 0; i < params->numPortc; i++)
            id_ptr[i] = rte_cpu_to_be_16(params->beamID[i]);

        /* zero padding */
        if(padding)
            memcpy((uint8_t *)&id_ptr[params->numPortc], zeropad, padding);
        }

    total_len += parm_size;
    ext10->all_bits.extLen = total_len / XRAN_SECTIONEXT_ALIGN;

    ext10->data_field = 0;
    ext10->data_field = (XRAN_CP_SECTIONEXTCMD_10 << xran_cp_radioapp_sec_ext10_ExtType)
                      | (last_flag << xran_cp_radioapp_sec_ext10_EF)
                      | ((total_len / XRAN_SECTIONEXT_ALIGN) << xran_cp_radioapp_sec_ext10_ExtLen)
                      | (params->numPortc << xran_cp_radioapp_sec_ext10_NumPortc)
                      | (params->beamGrpType << xran_cp_radioapp_sec_ext10_BeamGroupType);


    return (total_len);
}

/**
 * @brief Estimates how many BFW sets can be fit to given MTU size
 *
 * @ingroup xran_cp_pkt
 *
 * @param numBFW        the number of BFW I/Qs
 * @param iqWidth       the bitwidth of BFW
 * @param compMeth      Compression method for BFW
 * @param mtu           MTU size
 *
 * @return
 *  the number of maximum set of BFWs on success
 *  XRAN_STATUS_INVALID_PARAM, if compression method is not supported.
 */
int32_t
xran_cp_estimate_max_set_bfws(uint8_t numBFWs, uint8_t iqWidth, uint8_t compMeth, uint16_t mtu)
{
    int32_t avail_len;
    int32_t bfw_bitsize;
    int32_t bundle_size;

    /* Exclude headers can be present */
    avail_len = mtu - ( RTE_PKTMBUF_HEADROOM \
                        + sizeof(struct xran_ecpri_hdr)                    \
                        + sizeof(struct xran_cp_radioapp_section1_header)   \
                        + sizeof(struct xran_cp_radioapp_section1)         \
                        + sizeof(union xran_cp_radioapp_section_ext6)     \
                        + sizeof(union xran_cp_radioapp_section_ext10) );

    /* Calculate the size of BFWs I/Q in bytes */
    bfw_bitsize = numBFWs * iqWidth * 2;
    bundle_size = bfw_bitsize>>3;
    if(bfw_bitsize%8) bundle_size++;

    bundle_size += 2;           /* two bytes for Beam ID */
    switch(compMeth) {
        case XRAN_BFWCOMPMETHOD_NONE:
            break;

        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            bundle_size += 1;   /* for bfwCompParam */
            break;

        default:
            print_err("Compression method %d is not supported!", compMeth);
            return (XRAN_STATUS_INVALID_PARAM);
        }

    return (avail_len / bundle_size);
}

inline static uint32_t
xran_cp_get_hdroffset_section1(uint32_t exthdr_size)
{
  uint32_t hdr_len;

    hdr_len = ( RTE_PKTMBUF_HEADROOM                                \
                + sizeof(struct xran_ecpri_hdr)                     \
                + sizeof(struct xran_cp_radioapp_section1_header)   \
                + sizeof(struct xran_cp_radioapp_section1)          \
                + exthdr_size );
    return (hdr_len);
}

/**
 * @brief Prepare Beam Forming Weights(BFWs) for Section Extension 11
 *   Copy sets of BFWs to buffer after compression if required.
 *
 * @ingroup xran_cp_pkt
 *
 * @param numSetBFW     the number of set of BFWs
 * @param numBFW        the number of BFWs in a set
 * @param iqWidth       the bitwidth of BFW
 * @param compMeth      Compression method for BFW
 * @param bfwIQ         the array of BFW I/Q source
 * @param dst           the pointer of destination buffer (external buffer)
 * @param dst_maxlen    the maximum length of destination buffer
 *                      need to exclude headroom from MTU
 * @param bfwInfo       Extension 11 PRB bundle information array.
 *                      BFW size, offset and pointer will be set.
 *
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_RESOURCE, if destination memory is not enough to store all BFWs
 */
int32_t xran_cp_prepare_ext11_bfws(uint8_t numSetBFW, uint8_t numBFW,
                        uint8_t iqWidth, uint8_t compMeth,
                        uint8_t *dst, int16_t dst_maxlen,
                        struct xran_ext11_bfw_info bfwInfo[])
{
    int32_t   i;
    int32_t   iq_bitsize, iq_size;
    int32_t   parm_size;
    int32_t   total_len;
    uint32_t  hdr_offset;
    uint8_t   *ptr;

    struct xranlib_compress_request  bfpComp_req;
    struct xranlib_compress_response bfpComp_rsp;

    if(dst == NULL) {
        print_err("Invalid destination pointer!");
        return (XRAN_STATUS_INVALID_PARAM);
    }

    /* Calculate the size of BFWs I/Q in bytes */
    iq_bitsize = numBFW * iqWidth * 2;
    iq_size = iq_bitsize>>3;
    if(iq_bitsize%8)
        iq_size++;

    /* Check maximum size */
    parm_size = ((compMeth == XRAN_BFWCOMPMETHOD_NONE)?0:1) + 2; /* bfwCompParam + beamID(2) */
    total_len = numSetBFW * (parm_size + iq_size);

    if(total_len >= dst_maxlen) {
        print_err("Exceed maximum length to fit the set of BFWs - (%d/%d)",
                    total_len, dst_maxlen);
        return (XRAN_STATUS_RESOURCE);
    }

    hdr_offset = xran_cp_get_hdroffset_section1(sizeof(union xran_cp_radioapp_section_ext11));

    /* Copy BFWs to destination buffer */
    ptr = dst + hdr_offset;
    switch(compMeth) {
        /* No compression */
        case XRAN_BFWCOMPMETHOD_NONE:
            for(i = 0; i < numSetBFW; i++) {
                *((uint16_t *)ptr) = rte_cpu_to_be_16((bfwInfo[i].beamId & 0x7fff));
                memcpy((ptr + 2), bfwInfo[i].pBFWs, iq_size);
                ptr += iq_size + 2; /* beam ID + IQ size */
            }
            break;

        /* currently only supports BFP compression */
        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            memset(&bfpComp_req, 0, sizeof(struct xranlib_compress_request));
            memset(&bfpComp_rsp, 0, sizeof(struct xranlib_compress_response));

            for(i = 0; i < numSetBFW; i++) {
                bfpComp_req.numRBs          = 1;
                bfpComp_req.numDataElements = numBFW*2;
                bfpComp_req.len             = numBFW*2*2;
                bfpComp_req.compMethod      = compMeth;
                bfpComp_req.iqWidth         = iqWidth;
                bfpComp_req.data_in         = (int16_t *)bfwInfo[i].pBFWs;
                bfpComp_rsp.data_out        = (int8_t*)(ptr + 2);   /* exponent will be stored at first byte */

                if(xranlib_compress_bfw(&bfpComp_req, &bfpComp_rsp) == 0) {
                    print_dbg("comp_len %d iq_size %d\n", bfpComp_rsp.len, iq_size);
                } else {
                    print_err("compression failed\n");
                    return (XRAN_STATUS_FAIL);
                    }
                /* move exponent, it is stored at first byte of output */
                *ptr = *(ptr + 2);

                /* beamId */
                *((uint16_t *)(ptr+1)) = rte_cpu_to_be_16((bfwInfo[i].beamId & 0x7fff));
                ptr += iq_size + 3;
            }
            break;

        default:
            print_err("Compression method %d is not supported!", compMeth);
            return (XRAN_STATUS_INVALID_PARAM);
    }

    /* Update the length of extension with padding */
    parm_size = (total_len + sizeof(union xran_cp_radioapp_section_ext11))
                    % XRAN_SECTIONEXT_ALIGN;
    if(parm_size) {
        /* Add padding */
        parm_size = XRAN_SECTIONEXT_ALIGN - parm_size;
        memcpy(ptr, zeropad, parm_size);
        total_len += parm_size;
        }

    return (total_len);
}


static void free_ext_buf(void *addr, void *opaque)
{
    /* free is not required for external buffers */
}

/*
 * extbuf_start : the pointer of the external buffer,
 *          It can be the start address of whole external buffer.
 * extbuf_len : total length of the external buffer (available space to access)
 *          To use the length of the data, offset2data should be zero.
 * */
int32_t xran_cp_attach_ext_buf(struct rte_mbuf *mbuf,
                uint8_t *extbuf_start, uint16_t extbuf_len,
                struct rte_mbuf_ext_shared_info *shinfo)
{
    rte_iova_t extbuf_iova;


    if(unlikely(mbuf == NULL)) {
        print_err("Invalid mbuf to attach!\n");
        return (XRAN_STATUS_INVALID_PARAM);
        }

    /* Update shared information */
    shinfo->free_cb = free_ext_buf;
    rte_mbuf_ext_refcnt_update(shinfo, 1);

    extbuf_iova = rte_malloc_virt2iova(extbuf_start);
    if(unlikely(extbuf_iova == RTE_BAD_IOVA)) {
        print_err("Failed rte_mem_virt2iova RTE_BAD_IOVA \n");
        return (XRAN_STATUS_INVALID_PARAM);
        }

    rte_pktmbuf_attach_extbuf(mbuf, extbuf_start, extbuf_iova, extbuf_len, shinfo);

    rte_pktmbuf_reset_headroom(mbuf);

    return (XRAN_STATUS_SUCCESS);
}


static int32_t
xran_prepare_sectionext_11(struct rte_mbuf *mbuf,
                struct xran_sectionext11_info *params, int32_t last_flag)
{
    union xran_cp_radioapp_section_ext11 *ext11;
    int32_t total_len;


#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely((params->numSetBFWs == 0)
            || (params->numSetBFWs > XRAN_MAX_SET_BFWS))) {
        print_err("Invalid number of the set of PRB bundle [%d]", params->numSetBFWs);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    /* BFWs are already present in the external buffer, just update the length */
    total_len = sizeof(union xran_cp_radioapp_section_ext11) + params->totalBfwIQLen;

    ext11 = (union xran_cp_radioapp_section_ext11 *)rte_pktmbuf_append(mbuf, total_len);
    if(ext11 == NULL) {
        print_err("Fail to allocate the space for section extension 11 [%d]", total_len);
        return (XRAN_STATUS_RESOURCE);
        }

    /*ext11->all_bits.extType      = XRAN_CP_SECTIONEXTCMD_11;
    ext11->all_bits.ef           = last_flag;
    ext11->all_bits.reserved     = 0;
    ext11->all_bits.RAD          = params->RAD;
    ext11->all_bits.disableBFWs  = params->disableBFWs;
    ext11->all_bits.numBundPrb   = params->numBundPrb;
    ext11->all_bits.bfwCompMeth  = params->bfwCompMeth;
    ext11->all_bits.bfwIqWidth   = XRAN_CONVERT_BFWIQWIDTH(params->bfwIqWidth);

    ext11->all_bits.extLen        = total_len / XRAN_SECTIONEXT_ALIGN;*/

    ext11->data_field.data_field1 = (last_flag << xran_cp_radioapp_sec_ext11_bitfield_Ef)
                                  | (XRAN_CP_SECTIONEXTCMD_11 << xran_cp_radioapp_sec_ext11_bitfield_ExtType)
                                  | ((total_len / XRAN_SECTIONEXT_ALIGN) << xran_cp_radioapp_sec_ext11_bitfield_ExtLen)
                                  | (params->disableBFWs << xran_cp_radioapp_sec_ext11_bitfield_DisBFWs)
                                  | (params->RAD << xran_cp_radioapp_sec_ext11_bitfield_RAD);
    ext11->data_field.data_field2 = ((XRAN_CONVERT_BFWIQWIDTH(params->bfwIqWidth)) << xran_cp_radioapp_sec_ext11_bitfield_BFWIQWidth)
                                  | (params->bfwCompMeth << xran_cp_radioapp_sec_ext11_bitfield_BFWCompMeth)
                                  | params->numBundPrb;

    *(uint32_t *)ext11 = rte_cpu_to_be_32(*(uint32_t*)ext11);

    return (total_len);
}

/**
 * @brief add section extension to C-Plane packet
 *
 * @param mbuf
 *  A pointer to the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM
 *  XRAN_STATUS_RESOURCE if failed to allocate the space to packet buffer
 */
int32_t xran_append_section_extensions(struct rte_mbuf *mbuf, struct xran_section_gen_info *params)
{
    int32_t i;
    uint32_t totalen;
    int32_t last_flag;
    int32_t ext_size;

    if(unlikely(params->exDataSize > XRAN_MAX_NUM_EXTENSIONS)) {
        print_err("Invalid total number of extensions - %d", params->exDataSize);
        return (XRAN_STATUS_INVALID_PARAM);
    }

    totalen = 0;

    print_dbg("params->exDataSize %d\n", params->exDataSize);
    for(i=0; i < params->exDataSize; i++) {
        if(params->exData[i].data == NULL) {
            print_err("Invalid parameter - extension data %d is NULL", i);
            continue;
        }

        last_flag = (params->exDataSize == (i+1))?0:1;

        switch(params->exData[i].type) {
            case XRAN_CP_SECTIONEXTCMD_1:
                ext_size = xran_append_sectionext_1(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_2:
                ext_size = xran_prepare_sectionext_2(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_3:
                ext_size = xran_prepare_sectionext_3(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_4:
                ext_size = xran_prepare_sectionext_4(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_5:
                ext_size = xran_prepare_sectionext_5(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_6:
                ext_size = xran_prepare_sectionext_6(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_9:
                ext_size = xran_prepare_sectionext_9(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_10:
                ext_size = xran_prepare_sectionext_10(mbuf, params->exData[i].data, last_flag);
                break;
            case XRAN_CP_SECTIONEXTCMD_11:
                ext_size = xran_prepare_sectionext_11(mbuf, params->exData[i].data, last_flag);
                break;
            default:
                print_err("Extension Type %d is not supported!", params->exData[i].type);
                ext_size = 0;
            }

        if(ext_size == XRAN_STATUS_RESOURCE) {
            break;
        }

        totalen += ext_size;
    }

    return (totalen);
}


/**
 * @brief Fill the section body of type 0 in C-Plane packet
 *
 * @param section
 *  A pointer to the section in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if the number of symbol is invalid
 */
static int32_t
xran_prepare_section0(struct xran_cp_radioapp_section0 *section, struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info->numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info->numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    section->hdr.u1.common.sectionId      = params->info->id;
    section->hdr.u1.common.rb             = params->info->rb;
    section->hdr.u1.common.symInc         = params->info->symInc;
    section->hdr.u1.common.startPrbc      = params->info->startPrbc;
    section->hdr.u1.common.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info->numPrbc);

    section->hdr.u.s0.reMask    = params->info->reMask;
    section->hdr.u.s0.numSymbol = params->info->numSymbol;
    section->hdr.u.s0.reserved  = 0;

    // for network byte order
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Fill the section header of type 0 in C-Plane packet
 *
 * @param s0hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  XRAN_STATUS_SUCCESS always
 */
static int32_t
xran_prepare_section0_hdr( struct xran_cp_radioapp_section0_header *s0hdr,
                struct xran_cp_gen_params *params)
{
    s0hdr->timeOffset               = rte_cpu_to_be_16(params->hdr.timeOffset);
    s0hdr->frameStructure.fftSize   = params->hdr.fftSize;
    s0hdr->frameStructure.uScs      = params->hdr.scs;
    s0hdr->cpLength                 = rte_cpu_to_be_16(params->hdr.cpLength);
    s0hdr->reserved                 = 0;

    return (XRAN_STATUS_SUCCESS);
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
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if the number of symbol is invalid
 */
static int32_t
xran_prepare_section1(struct xran_cp_radioapp_section1 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info->numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info->numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    /*section->hdr.u1.common.sectionId      = params->info->id;
    section->hdr.u1.common.rb             = params->info->rb;
    section->hdr.u1.common.symInc         = params->info->symInc;
    section->hdr.u1.common.startPrbc      = params->info->startPrbc;
    section->hdr.u1.common.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info->numPrbc);

    section->hdr.u.s1.reMask    = params->info->reMask;
    section->hdr.u.s1.numSymbol = params->info->numSymbol;
    section->hdr.u.s1.beamId    = params->info->beamId;
    section->hdr.u.s1.ef        = params->info->ef;*/

    section->hdr.u.first_4byte   = (params->info->reMask << xran_cp_radioapp_sec_hdr_sc_ReMask)
                                 | (params->info->numSymbol << xran_cp_radioapp_sec_hdr_sc_NumSym)
                                 | (params->info->ef << xran_cp_radioapp_sec_hdr_sc_Ef)
                                 | (params->info->beamId << xran_cp_radioapp_sec_hdr_sc_BeamID);
    section->hdr.u1.second_4byte = (params->info->id << xran_cp_radioapp_sec_hdr_c_SecId)
                                 | (params->info->rb << xran_cp_radioapp_sec_hdr_c_RB)
                                 | (params->info->symInc << xran_cp_radioapp_sec_hdr_c_SymInc)
                                 | (params->info->startPrbc << xran_cp_radioapp_sec_hdr_c_StartPrbc)
                                 | ((XRAN_CONVERT_NUMPRBC(params->info->numPrbc)) << xran_cp_radioapp_sec_hdr_c_NumPrbc);

    // for network byte order
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Fill the section header of type 1 in C-Plane packet
 *
 * @param s1hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  XRAN_STATUS_SUCCESS always
 */
static int32_t
xran_prepare_section1_hdr(struct xran_cp_radioapp_section1_header *s1hdr,
                struct xran_cp_gen_params *params)
{
    s1hdr->udComp.udIqWidth         = params->hdr.iqWidth;
    s1hdr->udComp.udCompMeth        = params->hdr.compMeth;
    s1hdr->reserved                 = 0;

    return (XRAN_STATUS_SUCCESS);
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
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if the number of symbol is invalid
 */
static int32_t
xran_prepare_section3(struct xran_cp_radioapp_section3 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info->numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info->numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    /*section->hdr.u1.common.sectionId      = params->info->id;
    section->hdr.u1.common.rb             = params->info->rb;
    section->hdr.u1.common.symInc         = params->info->symInc;
    section->hdr.u1.common.startPrbc      = params->info->startPrbc;
    section->hdr.u1.common.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info->numPrbc);

    section->hdr.u.s3.reMask    = params->info->reMask;
    section->hdr.u.s3.numSymbol = params->info->numSymbol;
    section->hdr.u.s3.beamId    = params->info->beamId;
    section->hdr.u.s3.ef        = params->info->ef;*/

    section->hdr.u.first_4byte   = (params->info->reMask << xran_cp_radioapp_sec_hdr_sc_ReMask)
                                 | (params->info->numSymbol << xran_cp_radioapp_sec_hdr_sc_NumSym)
                                 | (params->info->ef << xran_cp_radioapp_sec_hdr_sc_Ef)
                                 | (params->info->beamId << xran_cp_radioapp_sec_hdr_sc_BeamID);
    section->hdr.u1.second_4byte = (params->info->id << xran_cp_radioapp_sec_hdr_c_SecId)
                                 | (params->info->rb << xran_cp_radioapp_sec_hdr_c_RB)
                                 | (params->info->symInc << xran_cp_radioapp_sec_hdr_c_SymInc)
                                 | (params->info->startPrbc << xran_cp_radioapp_sec_hdr_c_StartPrbc)
                                 | ((XRAN_CONVERT_NUMPRBC(params->info->numPrbc)) << xran_cp_radioapp_sec_hdr_c_NumPrbc);

    section->freqOffset         = rte_cpu_to_be_32(params->info->freqOffset)>>8;
    section->reserved           = 0;

    /* for network byte order (header, 8 bytes) */
    *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Fill the section header of type 3 in C-Plane packet
 *
 * @param s3hdr
 *  A pointer to the section header in the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  XRAN_STATUS_SUCCESS always
 */
static int32_t
xran_prepare_section3_hdr(struct xran_cp_radioapp_section3_header *s3hdr,
                struct xran_cp_gen_params *params)
{
    s3hdr->timeOffset               = rte_cpu_to_be_16(params->hdr.timeOffset);
    s3hdr->frameStructure.fftSize   = params->hdr.fftSize;
    s3hdr->frameStructure.uScs      = params->hdr.scs;
    s3hdr->cpLength                 = rte_cpu_to_be_16(params->hdr.cpLength);
    s3hdr->udComp.udIqWidth         = params->hdr.iqWidth;
    s3hdr->udComp.udCompMeth        = params->hdr.compMeth;

    return (XRAN_STATUS_SUCCESS);
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
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if section type is not 1 or 3, or handler is NULL
 *  XRAN_STATUS_RESOURCE if failed to allocate the space to packet buffer
 */
int32_t
xran_append_control_section(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params,uint16_t start_sect_id)
{
    int32_t i, ret;
  uint32_t totalen;
  void *section;
    int32_t section_size;
    int32_t (*xran_prepare_section_func)(void *section, void *params);

    totalen = 0;
    switch(params->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: /* Unused RB or Symbols in DL or UL, not supportted */
            section_size                = sizeof(struct xran_cp_radioapp_section0);
            xran_prepare_section_func   = (int32_t (*)(void *, void *))xran_prepare_section0;
            break;

        case XRAN_CP_SECTIONTYPE_1: /* Most DL/UL Radio Channels */
            section_size                = sizeof(struct xran_cp_radioapp_section1);
            xran_prepare_section_func   = (int32_t (*)(void *, void *))xran_prepare_section1;
            break;

        case XRAN_CP_SECTIONTYPE_3: /* PRACH and Mixed-numerology Channels */
            section_size                = sizeof(struct xran_cp_radioapp_section3);
            xran_prepare_section_func   = (int32_t (*)(void *, void *))xran_prepare_section3;
            break;

        case XRAN_CP_SECTIONTYPE_5: /* UE scheduling information, not supported */
        case XRAN_CP_SECTIONTYPE_6: /* Channel Information, not supported */
        case XRAN_CP_SECTIONTYPE_7: /* LAA, not supported */
        default:
            section_size                = 0;
            xran_prepare_section_func   = NULL;
            print_err("Section Type %d is not supported!", params->sectionType);
            return (XRAN_STATUS_INVALID_PARAM);
        }

    if(unlikely(xran_prepare_section_func == NULL)) {
       print_err("Section Type %d is not supported!", params->sectionType);
       return (XRAN_STATUS_INVALID_PARAM);
    }

    for(i=start_sect_id; i < (start_sect_id + params->numSections); i++) {
        section = rte_pktmbuf_append(mbuf, section_size);
        if(section == NULL) {
            print_err("Fail to allocate the space for section[%d]!", i);
            return (XRAN_STATUS_RESOURCE);
        }
        print_dbg("%s %d ef %d\n", __FUNCTION__, i, params->sections[i].info->ef);
        ret = xran_prepare_section_func((void *)section,
                            (void *)&params->sections[i]);
        if(ret < 0){
            print_err("%s %d\n", __FUNCTION__, ret);
            return (ret);
        }
        totalen += section_size;

        if(params->sections[i].info->ef) {
            print_dbg("sections[%d].info.ef %d exDataSize %d  type %d\n", i, params->sections[i].info->ef,
                params->sections[i].exDataSize, params->sections[i].exData[0].type);
            ret = xran_append_section_extensions(mbuf, &params->sections[i]);
            if(ret < 0)
                return (ret);
            totalen += ret;
       }
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
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PARM if direction, slot index or symbold index is invalid
 */
static inline int32_t
xran_prepare_radioapp_common_header(struct xran_cp_radioapp_common_header *apphdr,
                struct xran_cp_gen_params *params)
{

#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->dir != XRAN_DIR_DL && params->dir != XRAN_DIR_UL)) {
        print_err("Invalid direction!");
        return (XRAN_STATUS_INVALID_PARAM);
        }
    if(unlikely(params->hdr.slotId > XRAN_SLOTID_MAX)) {
        print_err("Invalid Slot ID!");
        return (XRAN_STATUS_INVALID_PARAM);
        }
    if(unlikely(params->hdr.startSymId > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid Symbol ID!");
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    /*apphdr->field.all_bits = XRAN_PAYLOAD_VER << 28;
    apphdr->field.dataDirection   = params->dir;
    //apphdr->field.payloadVer      = XRAN_PAYLOAD_VER;
    apphdr->field.filterIndex     = params->hdr.filterIdx;
    apphdr->field.frameId         = params->hdr.frameId;
    apphdr->field.subframeId      = params->hdr.subframeId;
    apphdr->field.slotId          = xran_slotid_convert(params->hdr.slotId, 0);
    apphdr->field.startSymbolId   = params->hdr.startSymId;*/

    apphdr->field.all_bits   = (params->dir << xran_cp_radioapp_cmn_hdr_bitwidth_DataDir)
                             | (XRAN_PAYLOAD_VER << xran_cp_radioapp_cmn_hdr_bitwidth_PayLoadVer)
                             | (params->hdr.filterIdx << xran_cp_radioapp_cmn_hdr_bitwidth_FilterIdex)
                             | (params->hdr.frameId << xran_cp_radioapp_cmn_hdr_bitwidth_FrameId)
                             | (params->hdr.subframeId << xran_cp_radioapp_cmn_hdr_bitwidth_SubFrameId)
                             | (xran_slotid_convert(params->hdr.slotId, 0) << xran_cp_radioapp_cmn_hdr_bitwidth_SlotId)
                             | (params->hdr.startSymId << xran_cp_radioapp_cmn_hdr_bitwidth_StartSymId);

    apphdr->numOfSections   = params->numSections;
    apphdr->sectionType     = params->sectionType;

    /* radio app header has common parts of 4bytes for all section types */
    //*((uint32_t *)apphdr) = rte_cpu_to_be_32(*((uint32_t *)apphdr));
    *((uint32_t *)apphdr) = rte_cpu_to_be_32(apphdr->field.all_bits);
    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief add a radio application header in a C-Plane packet
 *
 * @param mbuf
 *  A pointer to the packet buffer
 * @param params
 *  A porinter to the information to generate a C-Plane packet
 * @return
 *  The length of added section (>0) on success
 *  XRAN_STATUS_INVALID_PARM if section type is invalid, or handler is NULL
 *  XRAN_STATUS_RESOURCE if failed to allocate the space to packet buffer
 */
int32_t
xran_append_radioapp_header(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params)
{
  int32_t ret;
  uint32_t totalen;
  struct xran_cp_radioapp_common_header *apphdr;
  int32_t (*xran_prepare_radioapp_section_hdr_func)(void *hdr, void *params);


#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->sectionType >= XRAN_CP_SECTIONTYPE_MAX)) {
        print_err("Invalid Section Type - %d", params->sectionType);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    switch(params->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: /* Unused RB or Symbols in DL or UL, not supportted */
            xran_prepare_radioapp_section_hdr_func = (int32_t (*)(void *, void*))xran_prepare_section0_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section0_header);
            break;

        case XRAN_CP_SECTIONTYPE_1: /* Most DL/UL Radio Channels */
            xran_prepare_radioapp_section_hdr_func = (int32_t (*)(void *, void*))xran_prepare_section1_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section1_header);
            break;

        case XRAN_CP_SECTIONTYPE_3: /* PRACH and Mixed-numerology Channels */
            xran_prepare_radioapp_section_hdr_func = (int32_t (*)(void *, void*))xran_prepare_section3_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section3_header);
            break;

        case XRAN_CP_SECTIONTYPE_5: /* UE scheduling information, not supported */
        case XRAN_CP_SECTIONTYPE_6: /* Channel Information, not supported */
        case XRAN_CP_SECTIONTYPE_7: /* LAA, not supported */
        default:
            print_err("Section Type %d is not supported!", params->sectionType);
            xran_prepare_radioapp_section_hdr_func = NULL;
            totalen = 0;
            return (XRAN_STATUS_INVALID_PARAM);
        }

    apphdr = (struct xran_cp_radioapp_common_header *)rte_pktmbuf_append(mbuf, totalen);
    if(unlikely(apphdr == NULL)) {
        print_err("Fail to reserve the space for radio application header!");
        return (XRAN_STATUS_RESOURCE);
        }

    ret = xran_prepare_radioapp_common_header(apphdr, params);
    if(unlikely(ret < 0)) {
        return (ret);
        }

    if(likely(xran_prepare_radioapp_section_hdr_func)) {
        totalen += xran_prepare_radioapp_section_hdr_func(apphdr, params);
        }
    else {
        print_err("xran_prepare_radioapp_section_hdr_func is NULL!");
        return (XRAN_STATUS_INVALID_PARAM);
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
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_RESOURCE if failed to allocate the space to packet buffer
 *  XRAN_STATUS_INVALID_PARM if section type is invalid
 */
int32_t
xran_prepare_ctrl_pkt(struct rte_mbuf *mbuf,
                        struct xran_cp_gen_params *params,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id,
                        uint16_t start_sect_id)
{
    int32_t ret;
  uint32_t payloadlen;
  struct xran_ecpri_hdr *ecpri_hdr;

    payloadlen = xran_build_ecpri_hdr(mbuf, CC_ID, Ant_ID, seq_id, &ecpri_hdr);

    ret = xran_append_radioapp_header(mbuf, params);
    if(ret < 0) {
        print_err("%s %d\n", __FUNCTION__, ret);
        return (ret);
    }
    payloadlen += ret;

    ret = xran_append_control_section(mbuf, params,start_sect_id);
    if(ret < 0) {
        print_err("%s %d\n", __FUNCTION__, ret);
        return (ret);
    }
    payloadlen += ret;

    /* set payload length */
    ecpri_hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(payloadlen);

    return (XRAN_STATUS_SUCCESS);
}

///////////////////////////////////////
// for RU emulation
int32_t
xran_parse_section_ext1(void *ext, struct xran_sectionext1_info *extinfo)
{
    int32_t len;
    int32_t total_len;
  struct xran_cp_radioapp_section_ext1 *ext1;
  uint8_t *data;
    int32_t parm_size = 0, iq_size, iq_size_bytes;
    int32_t N;
  void *pHandle;

    pHandle = NULL;
    N = xran_get_conf_num_bfweights(pHandle);
    extinfo->bfwNumber = N;

    ext1 = (struct xran_cp_radioapp_section_ext1 *)ext;
    data = (uint8_t *)ext;

    len = 0;
    total_len = ext1->extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->bfwCompMeth    = ext1->bfwCompMeth;
    extinfo->bfwIqWidth     = (ext1->bfwIqWidth==0)?16:ext1->bfwIqWidth;

    len     += sizeof(struct xran_cp_radioapp_section_ext1);
    data    += sizeof(struct xran_cp_radioapp_section_ext1);
    extinfo->p_bfwIQ =  (int8_t*)(data);

    switch(ext1->bfwCompMeth) {
        case XRAN_BFWCOMPMETHOD_NONE:
            parm_size = 0;
            break;

        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            parm_size = 1;
            extinfo->bfwCompParam.exponent = *data & 0x0f;
            break;

        case XRAN_BFWCOMPMETHOD_BLKSCALE:
            parm_size = 1;
            extinfo->bfwCompParam.blockScaler = *data;
            break;

        case XRAN_BFWCOMPMETHOD_ULAW:
            parm_size = 1;
            extinfo->bfwCompParam.compBitWidthShift = *data;
            break;

        case XRAN_BFWCOMPMETHOD_BEAMSPACE:
            parm_size = N>>3; if(N%8) parm_size++; parm_size *= 8;
            memcpy(data, extinfo->bfwCompParam.activeBeamspaceCoeffMask, parm_size);
            break;

        default:
            print_err("Invalid BfComp method - %d", ext1->bfwCompMeth);
            parm_size = 0;
        }

    len     += parm_size;
    data    += parm_size;
    iq_size_bytes = parm_size;

    /* Get BF weights */
    iq_size = N * extinfo->bfwIqWidth * 2;  // total in bits
    parm_size = iq_size>>3;                 // total in bytes (/8)
    if(iq_size%8) parm_size++;              // round up
    iq_size_bytes += parm_size;

    //memcpy(data, extinfo->p_bfwIQ, parm_size);
    extinfo->bfwIQ_sz = iq_size_bytes;

    len += parm_size;

    parm_size = len % XRAN_SECTIONEXT_ALIGN;
    if(parm_size)
        len += (XRAN_SECTIONEXT_ALIGN - parm_size);

    if(len != total_len) {
        print_err("The size of extension 1 is not correct! [%d:%d]", len, total_len);
    }

    return (total_len);
}

int32_t
xran_parse_section_ext2(void *ext, struct xran_sectionext2_info *extinfo)
{
    int32_t len;
    int32_t total_len;
  struct xran_cp_radioapp_section_ext2 *ext2;
  uint8_t *data;
    int32_t parm_size;
  uint32_t val;
    int32_t val_size;

    ext2 = (struct xran_cp_radioapp_section_ext2 *)ext;
    data = (uint8_t *)ext;
    *(uint32_t *)ext2 = rte_be_to_cpu_32(*(uint32_t *)ext2);

    len = 0;
    total_len = ext2->extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    parm_size = sizeof(struct xran_cp_radioapp_section_ext2);

    extinfo->bfAzPtWidth    = ext2->bfAzPtWidth;
    extinfo->bfZePtWidth    = ext2->bfZePtWidth;
    extinfo->bfAz3ddWidth   = ext2->bfAz3ddWidth;
    extinfo->bfZe3ddWidth   = ext2->bfZe3ddWidth;

    if(ext2->bfaCompResv0 || ext2->bfaCompResv1)
        print_err("Incorrect reserved field - %d, %d", ext2->bfaCompResv0, ext2->bfaCompResv1);

    data    += parm_size;
    len     += parm_size;

    val_size = (extinfo->bfAzPtWidth ? extinfo->bfAzPtWidth+1 : 0)
                + (extinfo->bfZePtWidth ? extinfo->bfZePtWidth+1 : 0)
                + (extinfo->bfAz3ddWidth ? extinfo->bfAz3ddWidth+1 : 0)
                + (extinfo->bfZe3ddWidth ? extinfo->bfZe3ddWidth+ 1: 0);
    if(val_size) {
        val = rte_be_to_cpu_32(*(uint32_t *)data);
        val >>= (32 - val_size);

        if(extinfo->bfZe3ddWidth) {
            extinfo->bfZe3dd    = val & bitmask[extinfo->bfZe3ddWidth];
            val >>= (extinfo->bfZe3ddWidth + 1);
            }
        if(extinfo->bfAz3ddWidth) {
            extinfo->bfAz3dd    = val & bitmask[extinfo->bfAz3ddWidth];
            val >>= (extinfo->bfAz3ddWidth + 1);
            }
        if(extinfo->bfZePtWidth) {
            extinfo->bfZePt     = val & bitmask[extinfo->bfZePtWidth];
            val >>= (extinfo->bfZePtWidth + 1);
            }
        if(extinfo->bfAzPtWidth) {
            extinfo->bfAzPt     = val & bitmask[extinfo->bfAzPtWidth];
            val >>= (extinfo->bfAzPtWidth + 1);
            }
        }

    parm_size = val_size/8;
    if(val_size%8) parm_size += 1;

    data    += parm_size;
    len     += parm_size;

    extinfo->bfAzSI = (*data >> 3) & 0x07;
    extinfo->bfZeSI = *data & 0x07;

    data++;
    len++;

    parm_size = len % XRAN_SECTIONEXT_ALIGN;
    if(parm_size)
        len += (XRAN_SECTIONEXT_ALIGN - parm_size);

    if(len != total_len) {
        print_err("The size of extension 2 is not correct! [%d:%d]", len, total_len);
        }

    return (total_len);

}

int32_t
xran_parse_section_ext3(void *ext, struct xran_sectionext3_info *extinfo)
{
    int32_t len;
    int32_t total_len;

    total_len = 0;
    len = *((uint8_t *)ext + 1);

    switch(len) {
        case 1:     /* non-first data layer */
            {
            union xran_cp_radioapp_section_ext3_non_first *ext3_nf;

            ext3_nf = (union xran_cp_radioapp_section_ext3_non_first *)ext;
            *(uint32_t *)ext3_nf = rte_be_to_cpu_32(*(uint32_t *)ext3_nf);

            total_len = ext3_nf->all_bits.extLen * XRAN_SECTIONEXT_ALIGN;    /* from word to byte */

            extinfo->codebookIdx= ext3_nf->all_bits.codebookIndex;
            extinfo->layerId    = ext3_nf->all_bits.layerId;
            extinfo->numLayers  = ext3_nf->all_bits.numLayers;
            }
            break;

        case 3:     /* first data layer with two antenna */
        case 4:     /* first data layer with four antenna */
            {
            union xran_cp_radioapp_section_ext3_first *ext3_f;
            uint16_t *beamid;

            ext3_f = (union xran_cp_radioapp_section_ext3_first *)ext;
            *(uint64_t *)ext3_f = rte_be_to_cpu_64(*(uint64_t *)ext3_f);

            total_len = ext3_f->all_bits.extLen * XRAN_SECTIONEXT_ALIGN; /* from word to byte */

            extinfo->codebookIdx= ext3_f->all_bits.codebookIndex;
            extinfo->layerId    = ext3_f->all_bits.layerId;
            extinfo->numLayers  = ext3_f->all_bits.numLayers;
            extinfo->txScheme   = ext3_f->all_bits.txScheme;
            extinfo->crsReMask  = ext3_f->all_bits.crsReMask;
            extinfo->crsShift   = ext3_f->all_bits.crsShift;
            extinfo->crsSymNum  = ext3_f->all_bits.crsSymNum;

            /* beam IDs are stored from 10th octet */
            beamid = (uint16_t *)((uint8_t *)ext + 10);

            extinfo->beamIdAP1  = rte_be_to_cpu_16(*beamid++);
            if(len == 4) {
                extinfo->beamIdAP2  = rte_be_to_cpu_16(*beamid++);
                extinfo->beamIdAP3  = rte_be_to_cpu_16(*beamid);
                extinfo->numAntPort = 4;
                }
            else {
                extinfo->numAntPort = 2;
                }
            }
            break;

        default:
            print_err("Invalid length of extension 3 - %d", len);
        }

    return (total_len);
}

int32_t
xran_parse_section_ext4(void *ext, struct xran_sectionext4_info *extinfo)
{
    int32_t len;
  struct xran_cp_radioapp_section_ext4 *ext4;
    int32_t total_len;

    ext4 = (struct xran_cp_radioapp_section_ext4 *)ext;

    *(uint32_t *)ext4 = rte_be_to_cpu_32(*(uint32_t *)ext4);

    len = 0;
    total_len = ext4->extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->modCompScaler  = ext4->modCompScaler;
    extinfo->csf            = ext4->csf;

    len += sizeof(struct xran_cp_radioapp_section_ext4);
    if(len != total_len) {
        print_err("The size of extension 4 is not correct! [%d:%d]", len, total_len);
        }

    return (total_len);
}

int32_t
xran_parse_section_ext5(void *ext,
                struct xran_sectionext5_info *extinfo)
{
  struct xran_cp_radioapp_section_ext_hdr *ext_hdr;
  struct xran_cp_radioapp_section_ext5 ext5;
    int32_t parm_size;
    int32_t total_len;
  uint8_t *data;
  uint16_t i;

    ext_hdr = (struct xran_cp_radioapp_section_ext_hdr *)ext;
    *(uint16_t *)ext_hdr = rte_be_to_cpu_16(*(uint16_t *)ext_hdr);

    total_len = ext_hdr->extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    /* one set has 3.5 bytes, so enforcing double to do integer calculation */
    parm_size = ((total_len-sizeof(struct xran_cp_radioapp_section_ext_hdr))*2) / 7;

    if(parm_size > XRAN_MAX_MODCOMP_ADDPARMS) {
        print_err("Exceeds maximum number of parameters - %d", parm_size);
        parm_size = XRAN_MAX_MODCOMP_ADDPARMS;
    }

    data = (uint8_t *)(ext_hdr + 1);

    i = 0;
    while(i < parm_size) {
        // For odd number set, more data can be copied
        *((uint64_t *)&ext5) = rte_be_to_cpu_64(*((uint64_t *)data));

        extinfo->mc[i].mcScaleOffset    = ext5.mcScaleOffset1;
        extinfo->mc[i].csf              = ext5.csf1;
        extinfo->mc[i].mcScaleReMask    = ext5.mcScaleReMask1;
        i++;

        extinfo->mc[i].mcScaleOffset    = ext5.mcScaleOffset2;
        extinfo->mc[i].csf              = ext5.csf2;
        extinfo->mc[i].mcScaleReMask    = ext5.mcScaleReMask2;
        i++;

        data += sizeof(struct xran_cp_radioapp_section_ext5);
        }

    /* check the values of last set
     * due to alignment, it cannot be identified by the length that 3 or 4, 11 or 12 and etc
     * don't check mcScaleOffset might not be zero (some part is out of zero-padding) */
    i--;
    if(i < XRAN_MAX_MODCOMP_ADDPARMS) {
        if(extinfo->mc[i].csf == 0 && extinfo->mc[i].mcScaleReMask == 0)
            extinfo->num_sets = i;
        else
            extinfo->num_sets = i+1;
    }else {
        print_err("Maximum total number %d is not correct!", i);
    }

    return (total_len);
}

int32_t
xran_parse_section_ext6(void *ext,
                struct xran_sectionext6_info *extinfo)
{
    int32_t len;
    union xran_cp_radioapp_section_ext6 *ext6;
    int32_t total_len;

    ext6 = (union xran_cp_radioapp_section_ext6 *)ext;
    *(uint64_t *)ext6 = rte_be_to_cpu_64(*(uint64_t *)ext6);

    total_len = ext6->all_bits.extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->rbgSize    = ext6->all_bits.rbgSize;
    extinfo->rbgMask    = ext6->all_bits.rbgMask;
    extinfo->symbolMask = ext6->all_bits.symbolMask;

    len = sizeof(union xran_cp_radioapp_section_ext6);
    if(len != total_len) {
        print_err("The size of extension 6 is not correct! [%d:%d]", len, total_len);
        }

    return (total_len);
}

int32_t
xran_parse_section_ext9(void *ext,
                 struct xran_sectionext9_info *extinfo, struct xran_cp_recv_params *result)
{
    int32_t len = 0;
    int32_t total_len;
    int8_t dssSlot = 0;
    int8_t presumed_technology = -1;
    struct xran_cp_radioapp_section_ext9 *ext9;

    ext9 = (struct xran_cp_radioapp_section_ext9 *)ext;
    *(uint32_t *)ext9 = rte_be_to_cpu_32(*(uint32_t *)ext9);

    total_len = ext9->extLen * XRAN_SECTIONEXT_ALIGN;
    
    if(result) {
        dssSlot = result->tti % result->dssPeriod;
        presumed_technology = result->technology_arr[dssSlot];
    } else {
        print_err("\nTechnology verification parameters not received");
        // return (-1);
    }

    if(presumed_technology != ext9->technology) {
        print_err("\nWrong technology recieved! [%d,%d]", presumed_technology, ext9->technology);
        // return (-1);
    }

    extinfo->technology = ext9->technology;
    extinfo->reserved = ext9->reserved;

    len += sizeof(struct xran_cp_radioapp_section_ext9);
    if(len != total_len) {
        print_err("\nThe size of extension 9 is not correct! [%d:%d]", len, total_len);
    }

    return (total_len);
}


int32_t
xran_parse_section_ext10(void *ext,
                struct xran_sectionext10_info *extinfo)
{
    int32_t len, padding;
    int32_t i;
    union xran_cp_radioapp_section_ext10 *ext10;
    int32_t total_len;
    uint16_t *ptr;

    ext10 = (union xran_cp_radioapp_section_ext10 *)ext;

    total_len = ext10->all_bits.extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->numPortc   = ext10->all_bits.numPortc;
    extinfo->beamGrpType= ext10->all_bits.beamGroupType;

    len = sizeof(union xran_cp_radioapp_section_ext10);
    if(ext10->all_bits.beamGroupType == XRAN_BEAMGT_VECTORLIST) {
        len += extinfo->numPortc * 2 - 1;
        padding = len % XRAN_SECTIONEXT_ALIGN;
        if(padding) {
            padding = XRAN_SECTIONEXT_ALIGN - padding;
            len += padding;
            }

        ptr = (uint16_t *)&ext10->all_bits.reserved;
        for(i=0; i < extinfo->numPortc; i++)
            extinfo->beamID[i] = rte_be_to_cpu_16(ptr[i]);
        }

    if(len != total_len) {
        print_err("The size of extension 10 is not correct! [%d:%d]", len, total_len);
        }

    return (total_len);
}

int32_t
xran_parse_section_ext11(void *ext,
                         struct xran_sectionext11_recv_info *extinfo)
{
    int32_t len;
    int32_t total_len;
    union xran_cp_radioapp_section_ext11 *ext11;
    uint8_t *data;
    int32_t parm_size, iq_size;
    int32_t N;
    void *pHandle;

    pHandle = NULL;
    N = xran_get_conf_num_bfweights(pHandle);

    ext11 = (union xran_cp_radioapp_section_ext11 *)ext;
    data = (uint8_t *)ext;

    *(uint32_t *)ext11 = rte_cpu_to_be_32(*(uint32_t*)ext11);
    total_len = ext11->all_bits.extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->RAD            = ext11->all_bits.RAD;
    extinfo->disableBFWs    = ext11->all_bits.disableBFWs;
    extinfo->numBundPrb     = ext11->all_bits.numBundPrb;
    extinfo->bfwCompMeth    = ext11->all_bits.bfwCompMeth;
    extinfo->bfwIqWidth     = (ext11->all_bits.bfwIqWidth==0)?16:ext11->all_bits.bfwIqWidth;

    len     = sizeof(union xran_cp_radioapp_section_ext11);
    data    += sizeof(union xran_cp_radioapp_section_ext11);

    extinfo->numSetBFWs = 0;
    while((len+4) < total_len) {    /* adding 4 is to consider zero pads */
        /* Get bfwCompParam */
        switch(ext11->all_bits.bfwCompMeth) {
            case XRAN_BFWCOMPMETHOD_NONE:
                parm_size = 0;
                break;

            case XRAN_BFWCOMPMETHOD_BLKFLOAT:
                parm_size = 1;
                extinfo->bundInfo[extinfo->numSetBFWs].bfwCompParam.exponent = *data & 0x0f;
                break;
#if 0   /* Not supported */
            case XRAN_BFWCOMPMETHOD_BLKSCALE:
                parm_size = 1;
                extinfo->bundInfo[extinfo->numSetBFWs].bfwCompParam.blockScaler = *data;
                break;

            case XRAN_BFWCOMPMETHOD_ULAW:
                parm_size = 1;
                extinfo->bundInfo[extinfo->numSetBFWs].bfwCompParam.compBitWidthShift = *data;
                break;

            case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                parm_size = N>>3; if(N%8) parm_size++; parm_size *= 8;
                memcpy(data, extinfo->bundInfo[extinfo->numSetBFWs].bfwCompParam.activeBeamspaceCoeffMask, parm_size);
                break;
#endif
            default:
                print_err("Invalid BfComp method - %d", ext11->all_bits.bfwCompMeth);
                parm_size = 0;
            }
        len     += parm_size;
        data    += parm_size;

        /* Get beam ID */
        extinfo->bundInfo[extinfo->numSetBFWs].beamId = rte_be_to_cpu_16(*((int16_t *)data));
        len     += sizeof(int16_t);
        data    += sizeof(int16_t);

        /* Get BF weights */
        iq_size = N * extinfo->bfwIqWidth * 2;  // total in bits
        parm_size = iq_size>>3;                 // total in bytes (/8)
        if(iq_size%8) parm_size++;              // round up

        if(extinfo->bundInfo[extinfo->numSetBFWs].pBFWs) {
            memcpy(extinfo->bundInfo[extinfo->numSetBFWs].pBFWs, data, parm_size);
            }
        extinfo->bundInfo[extinfo->numSetBFWs].BFWSize  = parm_size;

        len     += parm_size;
        data    += parm_size;
        extinfo->numSetBFWs++;
        }

    parm_size = len % XRAN_SECTIONEXT_ALIGN;
    if(parm_size)
        len += (XRAN_SECTIONEXT_ALIGN - parm_size);

    if(len != total_len) {
        //print_err("The size of extension 11 is not correct! [%d:%d]", len, total_len);
        }

    return (total_len);
}

int32_t
xran_parse_section_extension(struct rte_mbuf *mbuf,
                             void *ext, struct xran_cp_recv_params *result,
                             int32_t section_idx)
{
    struct xran_section_recv_info *section = &result->sections[section_idx];
    int32_t total_len, len, numext;
  uint8_t *ptr;
    int32_t flag_last;
    int32_t ext_type;
    int32_t i;

    total_len = 0;
    ptr = (uint8_t *)ext;

    numext = 0;

    flag_last = 1;
    i = 0;
    while(flag_last) {
        /* check ef */
        flag_last = (*ptr & 0x80);

        ext_type = *ptr & 0x7f;
        section->exts[numext].type = ext_type;
        switch(ext_type) {
            case XRAN_CP_SECTIONEXTCMD_1:
                result->ext1count++;
                len = xran_parse_section_ext1(ptr, &section->exts[numext].u.ext1);
                break;
            case XRAN_CP_SECTIONEXTCMD_2:
                len = xran_parse_section_ext2(ptr, &section->exts[numext].u.ext2);
                break;
            case XRAN_CP_SECTIONEXTCMD_3:
                len = xran_parse_section_ext3(ptr, &section->exts[numext].u.ext3);
                break;
            case XRAN_CP_SECTIONEXTCMD_4:
                len = xran_parse_section_ext4(ptr, &section->exts[numext].u.ext4);
                break;
            case XRAN_CP_SECTIONEXTCMD_5:
                len = xran_parse_section_ext5(ptr, &section->exts[numext].u.ext5);
                break;
            case XRAN_CP_SECTIONEXTCMD_6:
                len = xran_parse_section_ext6(ptr, &section->exts[numext].u.ext6);
                break;
            case XRAN_CP_SECTIONEXTCMD_9:
                len = xran_parse_section_ext9(ptr, &section->exts[numext].u.ext9, result);
                break;
            case XRAN_CP_SECTIONEXTCMD_10:
                len = xran_parse_section_ext10(ptr, &section->exts[numext].u.ext10);
                break;
            case XRAN_CP_SECTIONEXTCMD_11:
                len = xran_parse_section_ext11(ptr, &section->exts[numext].u.ext11);
                break;

            default:
                print_err("Extension %d is not supported!", ext_type);
                len = 0;
            }

        section->exts[numext].size = len;
        ptr += len; total_len += len;

        i++;
        if(++numext < XRAN_MAX_NUM_EXTENSIONS) continue;

        /* exceeds maximum number of extensions */
        break;
        }

    section->numExts = numext;

    return (total_len);
}

/**
 * @brief Parse a C-Plane packet (for RU emulation)
 *  Transport layer fragmentation is not supported.
 *
 * @ingroup xran_cp_pkt
 *
 * @param mbuf
 *  The pointer of the packet buffer to be parsed
 * @param params
 *  The pointer of structure to store the information of parsed packet
 * @param eaxc
 *  The pointer of sturcture to store the decomposed information of ecpriRtcid/ecpriPcid
 * @return
 *  XRAN_STATUS_SUCCESS on success
 *  XRAN_STATUS_INVALID_PACKET if failed to parse the packet
 */
int32_t
xran_parse_cp_pkt(struct rte_mbuf *mbuf,
                    struct xran_cp_recv_params *result,
                    struct xran_recv_packet_info *pkt_info, void* handle, uint32_t *mb_free)
{
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_cp_radioapp_common_header *apphdr;
    struct xran_common_counters* pCnt = NULL;
    struct xran_prb_map *pRbMap = NULL;
    struct xran_prb_map *pRbMap_desc = NULL;
    struct xran_prb_elm * prbMapElm = NULL;
    struct rte_mbuf *mb = NULL;
    int32_t i, j, ret, extlen;
    int tti = 0,interval = 0;
    uint8_t idx = 0, ctx_id = 0;
    struct xran_device_ctx * p_dev_ctx = NULL;
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    if(unlikely(p_xran_dev_ctx == NULL)){
        print_err("p_xran_dev_ctx is NULL\n");
        return XRAN_STATUS_INVALID_PARAM;
    }
    p_dev_ctx = xran_dev_get_ctx();
    ret = xran_parse_ecpri_hdr(mbuf, &ecpri_hdr, pkt_info);
    struct xran_eaxc_info eaxc = pkt_info->eaxc;
    struct xran_section_info *info = NULL;
    if(ret < 0 && ecpri_hdr == NULL)
        return (XRAN_STATUS_INVALID_PACKET);

    /* Process radio header. */
    apphdr = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_ecpri_hdr));
    if(unlikely(apphdr == NULL)) {
        print_err("Invalid packet - radio app header!");
        return (XRAN_STATUS_INVALID_PACKET);
        }

    *((uint32_t *)apphdr) = rte_be_to_cpu_32(*((uint32_t *)apphdr));

    if(apphdr->field.payloadVer != XRAN_PAYLOAD_VER) {
        print_err("Invalid Payload version - %d", apphdr->field.payloadVer);
        ret = XRAN_STATUS_INVALID_PACKET;
        }

    result->dir             = apphdr->field.dataDirection;
    result->hdr.filterIdx   = apphdr->field.filterIndex;
    result->hdr.frameId     = apphdr->field.frameId;
    result->hdr.subframeId  = apphdr->field.subframeId;
    result->hdr.slotId      = apphdr->field.slotId;
    result->hdr.startSymId  = apphdr->field.startSymbolId;
    result->sectionType     = apphdr->sectionType;
    result->numSections     = apphdr->numOfSections;
    result->ext1count       = 0;

    interval = p_xran_dev_ctx->interval_us_local;
    tti = apphdr->field.frameId * SLOTS_PER_SYSTEMFRAME(interval) + apphdr->field.subframeId * SLOTNUM_PER_SUBFRAME(interval) + apphdr->field.slotId;
    result->tti = tti;
    ctx_id      = tti % XRAN_MAX_SECTIONDB_CTX;
#if 0
    printf("[CP%5d] eAxC[%d:%d:%02d:%02d] %s seq[%03d-%03d-%d] sec[%d-%d] frame[%3d-%2d-%2d] sym%02d\n",
        pkt_info->payload_len,
        pkt_info->eaxc.cuPortId, pkt_info->eaxc.bandSectorId,
        pkt_info->eaxc.ccId, pkt_info->eaxc.ruPortId,
        result->dir?"DL":"UL",
        pkt_info->seq_id, pkt_info->subseq_id, pkt_info->ebit,
        result->sectionType, result->numSections,
        result->hdr.frameId, result->hdr.subframeId, result->hdr.slotId,
        result->hdr.startSymId
        );
#endif

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
                    print_err("Invalid packet: section type0 - radio app hedaer!");
                    return (XRAN_STATUS_INVALID_PACKET);
                    }
                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.u1.common.sectionId;
                    result->sections[i].info.rb         = section->hdr.u1.common.rb;
                    result->sections[i].info.symInc     = section->hdr.u1.common.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.u1.common.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.u1.common.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s0.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s0.reMask;
                    //section->hdr.u.s0.reserved;   /* should be zero */

                    section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section0));
                    if(section == NULL) {
                        print_err("Invalid packet: section type0 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = XRAN_STATUS_INVALID_PACKET;
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
                if(unlikely(section == NULL)) {
                    print_err("Invalid packet: section type1 - radio app hedaer!");
                    return (XRAN_STATUS_INVALID_PACKET);
                    }

                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.u1.common.sectionId;
                    result->sections[i].info.rb         = section->hdr.u1.common.rb;
                    result->sections[i].info.symInc     = section->hdr.u1.common.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.u1.common.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.u1.common.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s1.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s1.reMask;
                    result->sections[i].info.beamId     = section->hdr.u.s1.beamId;
                    result->sections[i].info.ef         = section->hdr.u.s1.ef;

                    section = (void *)rte_pktmbuf_adj(mbuf,
                                    sizeof(struct xran_cp_radioapp_section1));
                    if(unlikely(section == NULL)) {
                        print_err("Invalid packet: section type1 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = XRAN_STATUS_INVALID_PACKET;
                        break;
                        }
                    if (eaxc.ruPortId < p_xran_dev_ctx->srs_cfg.eAxC_offset)
                    {
                        struct xran_flat_buffer *pBuffer = NULL;
                        if(result->dir == 1)
                            pBuffer = p_xran_dev_ctx->sFHCpRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][eaxc.ruPortId].sBufferList.pBuffers;
                        else if(result->dir == 0)
                            pBuffer = p_xran_dev_ctx->sFHCpTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][eaxc.ruPortId].sBufferList.pBuffers;
                        if(pBuffer)
                            pRbMap = (struct xran_prb_map *)pBuffer->pData;
                        if(p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][eaxc.ruPortId].sBufferList.pBuffers)
                            pRbMap_desc = (struct xran_prb_map *) p_xran_dev_ctx->sFrontHaulTxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][eaxc.ruPortId].sBufferList.pBuffers->pData;

                        if(i == 0){
                            if((pRbMap_desc != NULL) && (pRbMap_desc->nPrbElm <= p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId])){
                                p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId]=0;
                                xran_cp_reset_section_info(handle, result->dir, eaxc.ccId, eaxc.ruPortId, ctx_id);
                            }
                            idx = p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId]++;  

                            if(p_dev_ctx){
                                result->numSetBFW = p_dev_ctx->numSetBFWs_arr[idx];
                                if(likely(pRbMap!=NULL)){
                                    prbMapElm = &pRbMap->prbMap[idx];
                                    mb = prbMapElm->bf_weight.p_ext_start;
                                    if(mb){
                                        rte_pktmbuf_free(mb);
                                    }
                                    prbMapElm->bf_weight.p_ext_start = mbuf;
                                    prbMapElm->bf_weight.p_ext_section = (void *)section;
                                    *mb_free = MBUF_KEEP;
                                }
                            }
                        }
                        info = xran_cp_get_section_info_ptr(handle, result->dir, eaxc.ccId, eaxc.ruPortId, ctx_id);
                        if(likely(info != NULL))
                        {
                            info->prbElemBegin = (i == 0 ) ?  1 : 0;
                            info->prbElemEnd   = (i == (result->numSections -1)) ?  1 : 0;
                            info->ef           = result->sections[i].info.ef;
                            info->startPrbc    = result->sections[i].info.startPrbc;
                            info->numPrbc      = result->sections[i].info.numPrbc;
                            info->type         = result->sections[i].info.type;
                            info->startSymId   = result->hdr.startSymId;
                            info->iqWidth      = result->hdr.iqWidth;
                            info->compMeth     = result->hdr.compMeth;
                            info->id           = result->sections[i].info.id;
                            info->rb           = XRAN_RBIND_EVERY;
                            info->numSymbol    = result->sections[i].info.numSymbol;
                            info->reMask       = 0xfff;
                            info->beamId       = result->sections[i].info.beamId;
                            info->symInc       = XRAN_SYMBOLNUMBER_NOTINC;

                            int loc_sym=0;
                            if(likely(pRbMap_desc != NULL)){
                                prbMapElm = &pRbMap_desc->prbMap[idx];
                                for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++)
                                {
                                    struct xran_section_desc *p_sec_desc =  &prbMapElm->sec_desc[loc_sym][0];

                                    if(likely(p_sec_desc!=NULL))
                                    {
                                        info->sec_desc[loc_sym].iq_buffer_offset = p_sec_desc->iq_buffer_offset;
                                        info->sec_desc[loc_sym].iq_buffer_len    = p_sec_desc->iq_buffer_len;
                                        
                                        p_sec_desc->section_id   = info->id;
                                    }
                                    else
                                    {
                                        print_err("section desc is NULL\n");
                                    }
                                } /* for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++) */
                            }
                        }

                    if(result->sections[i].info.ef) {
                            result->dssPeriod = p_xran_dev_ctx->dssPeriod;
                            for( j=0; j< p_xran_dev_ctx->dssPeriod; j++) {
                                result->technology_arr[j] = p_xran_dev_ctx->technology[j];
                            }
                            extlen = xran_parse_section_extension(mbuf, (void *)section, result, i);
                        if(extlen > 0) {
                            section = (void *)rte_pktmbuf_adj(mbuf, extlen);
                                if(unlikely(section == NULL)) {
                                print_err("Invalid packet: section type1 - section extension [%d]!", i);
                                ret = XRAN_STATUS_INVALID_PACKET;
                                break;
                                }
                            }
                        }
                    else extlen = 0;
                    }
                    else if((eaxc.ruPortId >= p_xran_dev_ctx->srs_cfg.eAxC_offset) && p_xran_dev_ctx->fh_cfg.srsEnable){
                        int32_t ant_id = ((eaxc.ruPortId - p_xran_dev_ctx->srs_cfg.eAxC_offset) & 0x3F); /*Klocwork fix*/
                        if(p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][ant_id].sBufferList.pBuffers){
                            pRbMap_desc = (struct xran_prb_map *) p_xran_dev_ctx->sFHSrsRxPrbMapBbuIoBufCtrl[tti % XRAN_N_FE_BUF_LEN][eaxc.ccId][ant_id].sBufferList.pBuffers->pData;
                        }
                        if(i == 0){
                            if((pRbMap_desc != NULL) && (pRbMap_desc->nPrbElm <= p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId])){
                                p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId]=0;
                                xran_cp_reset_section_info(handle, result->dir, eaxc.ccId, eaxc.ruPortId, ctx_id);
                            }
                            idx = p_xran_dev_ctx->sectiondb_elm[ctx_id][result->dir][eaxc.ccId][eaxc.ruPortId]++;  
                        }
                        info = xran_cp_get_section_info_ptr(handle, result->dir, eaxc.ccId, eaxc.ruPortId, ctx_id);
                        if(likely(info != NULL))
                        {
                            info->prbElemBegin = (i == 0 ) ?  1 : 0;
                            info->prbElemEnd   = (i == (result->numSections -1)) ?  1 : 0;
                            info->ef           = result->sections[i].info.ef;
                            info->type         = result->sections[i].info.type;
                            info->startSymId   = result->hdr.startSymId;
                            info->iqWidth      = result->hdr.iqWidth;
                            info->compMeth     = result->hdr.compMeth;
                            info->id           = result->sections[i].info.id;
                            info->rb           = XRAN_RBIND_EVERY;
                            info->numSymbol    = result->sections[i].info.numSymbol;
                            info->reMask       = 0xfff;
                            info->beamId       = result->sections[i].info.beamId;
                            info->symInc       = XRAN_SYMBOLNUMBER_NOTINC;
                            int loc_sym=0;
                            if(likely(pRbMap_desc != NULL)){
                                prbMapElm = &pRbMap_desc->prbMap[idx];
                                info->startPrbc    = prbMapElm->nRBStart;
                                info->numPrbc      = prbMapElm->nRBSize;

                                struct xran_section_desc *p_sec_desc = NULL;
                                for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++)
                                {
                                    p_sec_desc =  &prbMapElm->sec_desc[loc_sym][0];

                                    if(likely(p_sec_desc!=NULL))
                                    {
                                        info->sec_desc[loc_sym].iq_buffer_offset = p_sec_desc->iq_buffer_offset;
                                        info->sec_desc[loc_sym].iq_buffer_len    = p_sec_desc->iq_buffer_len;                                    
                                        p_sec_desc->section_id   = info->id;
                                    }
                                    else
                                    {
                                        print_err("section desc is NULL\n");
                                    }
                                } /* for(loc_sym = 0; loc_sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; loc_sym++) */
                            }
                        }
                        /*Assuming SRS CP will not have extension, removed the ef flag check and extension processing*/
                    }    
                }  
                pCnt = &p_xran_dev_ctx->fh_counters;
                /* SRS should not have extension */
                if(pCnt && (result->sections[0].info.ef) && (result->sections[0].exts[0].type == 1) && (result->numSections != result->numSetBFW) && (result->ext1count != result->numSetBFW)){
                    print_err("extension 1 is not Valid! [%d:%d:%d]", result->numSections, result->numSetBFW, result->ext1count);
                    pCnt->rx_invalid_ext1_packets++;
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
                    print_err("Invalid packet: section type3 - radio app hedaer!");
                    return (XRAN_STATUS_INVALID_PACKET);
                    }

                for(i=0; i<result->numSections; i++) {
                    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));

                    result->sections[i].info.type       = apphdr->sectionType;
                    result->sections[i].info.id         = section->hdr.u1.common.sectionId;
                    result->sections[i].info.rb         = section->hdr.u1.common.rb;
                    result->sections[i].info.symInc     = section->hdr.u1.common.symInc;
                    result->sections[i].info.startPrbc  = section->hdr.u1.common.startPrbc;
                    result->sections[i].info.numPrbc    = section->hdr.u1.common.numPrbc,
                    result->sections[i].info.numSymbol  = section->hdr.u.s3.numSymbol;
                    result->sections[i].info.reMask     = section->hdr.u.s3.reMask;
                    result->sections[i].info.beamId     = section->hdr.u.s3.beamId;
                    result->sections[i].info.ef         = section->hdr.u.s3.ef;
                    result->sections[i].info.freqOffset = ((int32_t)rte_be_to_cpu_32(section->freqOffset))>>8;

                    if(section->reserved) {
                        print_err("Invalid packet: section type3 - section[%d] reserved[%d]", i, section->reserved);
                        ret = XRAN_STATUS_INVALID_PACKET;
                        }

                    section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section3));
                    if(section == NULL) {
                        print_err("Invalid packet: section type3 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = XRAN_STATUS_INVALID_PACKET;
                        break;
                        }

                    if(result->sections[i].info.ef) {
                        // parse section extension
                        extlen = xran_parse_section_extension(mbuf, (void *)section, result, i);
                        if(extlen > 0) {
                            section = (void *)rte_pktmbuf_adj(mbuf, extlen);
                            if(section == NULL) {
                                print_err("Invalid packet: section type3 - section extension [%d]!", i);
                                ret = XRAN_STATUS_INVALID_PACKET;
                                break;
                                }
                            }
                        }
                    else extlen = 0;
                    }
            }
            break;

        case XRAN_CP_SECTIONTYPE_5: // UE scheduling information, not supported
        case XRAN_CP_SECTIONTYPE_6: // Channel Information, not supported
        case XRAN_CP_SECTIONTYPE_7: // LAA, not supported
        default:
            ret = XRAN_STATUS_INVALID_PARAM;
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
        printf("  || %3d:%04X| rb=%d symInc=%d numSym=%d startPrbc=%02d numPrbc=%d reMask=%03X beamId=%04X freqOffset=%d ef=%d\n",
            i, result->sections[i].info.id,
            result->sections[i].info.rb,
            result->sections[i].info.symInc, result->sections[i].info.numSymbol,
            result->sections[i].info.startPrbc, result->sections[i].info.numPrbc,
            result->sections[i].info.reMask,
            result->sections[i].info.beamId,
            result->sections[i].info.freqOffset,
            result->sections[i].info.ef);

        if(result->sections[i].info.ef) {
            for(int32_t j=0; j<result->sections[i].exDataSize; j++) {
                printf("      || %2d : type=%d len=%d\n",
                        j, result->sections[i].exData[j].type, result->sections[i].exData[j].len);
                switch(result->sections[i].exData[j].type) {
                    case XRAN_CP_SECTIONEXTCMD_1:
                        {
                        struct xran_sectionext1_info *ext1;
                        ext1 = result->sections[i].exData[j].data;
                        printf("      ||    bfwNumber=%d bfwIqWidth=%d bfwCompMeth=%d\n",
                                ext1->bfwNumber, ext1->bfwIqWidth, ext1->bfwCompMeth);
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_2:
                        {
                        struct xran_sectionext2_info *ext2;
                        ext2 = result->sections[i].exData[j].data;
                        printf("      ||    AzPt=%02x(%d) ZePt=%02x(%d) Az3dd=%02x(%d) Ze3dd=%02x(%d) AzSI=%02x ZeSI=%02x\n",
                                ext2->bfAzPt, ext2->bfAzPtWidth,
                                ext2->bfZePt, ext2->bfZePtWidth,
                                ext2->bfAz3dd, ext2->bfAz3ddWidth,
                                ext2->bfZe3dd, ext2->bfZe3ddWidth,
                                ext2->bfAzSI, ext2->bfZeSI);
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_4:
                        {
                        struct xran_sectionext4_info *ext4;
                        ext4 = result->sections[i].exData[j].data;
                        printf("      ||    csf=%d modCompScaler=%d\n",
                                ext4->csf, ext4->modCompScaler);
                        }
                        break;
                    case XRAN_CP_SECTIONEXTCMD_5:
                        {
                        struct xran_sectionext5_info *ext5;
                        ext5 = result->sections[i].exData[j].data;
                        printf("      ||    num_sets=%d\n", ext5->num_sets);
                        for(int32_t k=0; k<ext5->num_sets; k++) {
                            printf("          || %d - csf=%d mcScaleReMask=%04x mcScaleOffset=%04x\n",
                                k, ext5->mc[k].csf,
                                ext5->mc[k].mcScaleReMask, ext5->mc[k].mcScaleOffset);
                            }
                        }
                        break;

                    case XRAN_CP_SECTIONEXTCMD_0:
                    case XRAN_CP_SECTIONEXTCMD_3:
                    default:
                        printf("Invalid section extension type!\n");
                    }
                }
            }
        }
#endif

    return(ret);
}
