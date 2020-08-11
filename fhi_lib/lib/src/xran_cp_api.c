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
#include "xran_printf.h"
#include "xran_compression.h"


/**
 * This structure to store the section information of C-Plane
 * in order to generate and parse corresponding U-Plane */
struct xran_sectioninfo_db {
    uint32_t    cur_index;  /**< Current index to store for this eAXC */
    struct xran_section_info list[XRAN_MAX_NUM_SECTIONS]; /**< The array of section information */
    };

static struct xran_sectioninfo_db sectiondb[XRAN_MAX_SECTIONDB_CTX][XRAN_DIR_MAX][XRAN_COMPONENT_CARRIERS_MAX][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR];

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
int xran_cp_init_sectiondb(void *pHandle)
{
  int ctx, dir, cc, ant;

    for(ctx=0; ctx < XRAN_MAX_SECTIONDB_CTX; ctx++)
        for(dir=0; dir < XRAN_DIR_MAX; dir++)
            for(cc=0; cc < XRAN_COMPONENT_CARRIERS_MAX; cc++)
                for(ant=0; ant < XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR; ant++)
                    sectiondb[ctx][dir][cc][ant].cur_index = 0;

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
int xran_cp_free_sectiondb(void *pHandle)
{
    return (XRAN_STATUS_SUCCESS);
}

static inline struct xran_sectioninfo_db *xran_get_section_db(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  struct xran_sectioninfo_db *ptr;


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

    ptr = &sectiondb[ctx_id][dir][cc_id][ruport_id];

    return(ptr);
}

static inline struct xran_section_info *xran_get_section_info(struct xran_sectioninfo_db *ptr, uint16_t index)
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
int xran_cp_add_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id,
        struct xran_section_info *info)
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

    rte_memcpy(list, info, sizeof(struct xran_section_info));

    ptr->cur_index++;

    return (XRAN_STATUS_SUCCESS);
}

int xran_cp_add_multisection_info(void *pHandle,
        uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id,
        struct xran_cp_gen_params *gen_info)
{
  int i;
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

    for(i=0; i<num_sections; i++) {
        rte_memcpy(&list[i], &gen_info->sections[i].info, sizeof(struct xran_section_info));
        ptr->cur_index++;
        }

    return (XRAN_STATUS_SUCCESS);
}

/**
 * @brief Find a section information of C-Plane from dabase
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
struct xran_section_info *xran_cp_find_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t ctx_id, uint16_t section_id)
{
  int index, num_index;
  struct xran_sectioninfo_db *ptr;


    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL))
        return (NULL);

    if(ptr->cur_index > XRAN_MAX_NUM_SECTIONS)
        num_index = XRAN_MAX_NUM_SECTIONS;
    else
        num_index = ptr->cur_index;

    for(index=0; index < num_index; index++) {
        if(ptr->list[index].id == section_id) {
            return (xran_get_section_info(ptr, index));
            }
        }

    print_dbg("No section ID in the list - %d", section_id);
    return (NULL);
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
struct xran_section_info *xran_cp_iterate_section_info(void *pHandle,
        uint8_t dir, uint8_t cc_id, uint8_t ruport_id,
        uint8_t ctx_id, uint32_t *next)
{
  int index;
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
int32_t xran_cp_getsize_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  int index;
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
int xran_cp_reset_section_info(void *pHandle, uint8_t dir, uint8_t cc_id, uint8_t ruport_id, uint8_t ctx_id)
{
  struct xran_sectioninfo_db *ptr;

    ptr = xran_get_section_db(pHandle, dir, cc_id, ruport_id, ctx_id);
    if(unlikely(ptr == NULL)) {
        return (XRAN_STATUS_INVALID_PARAM);
        }

    ptr->cur_index = 0;

    return (XRAN_STATUS_SUCCESS);
}


int xran_dump_sectiondb(void)
{
    // TODO:
    return (0);
}

int32_t xran_cp_populate_section_ext_1(int8_t  *p_ext1_dst,    /**< destination buffer */
                                       uint16_t  ext1_dst_len, /**< dest buffer size */
                                       int16_t  *p_bfw_iq_src, /**< source buffer of IQs */
                                       uint16_t  rbNum,        /* number RBs to ext1 chain */
                                       uint16_t  bfwNumPerRb,  /* number of bf weights per RB (i.e. antenna elements) */
                                       uint8_t   bfwiqWidth,   /* bit size of IQs */
                                       uint8_t   bfwCompMeth)  /* compression method */
{
    struct xran_cp_radioapp_section_ext1 *p_ext1;

    uint8_t *p_bfw_content = NULL;
    int32_t parm_size   = 0;
    int32_t bfw_iq_bits = 0;
    int32_t total_len   = 0;
    int32_t comp_len    = 0;
    uint8_t ext_flag    = XRAN_EF_F_ANOTHER_ONE;
    int16_t idxRb       = 0;
    int16_t cur_ext_len = 0;
    int8_t  *p_ext1_dst_cur = NULL;

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

    /* create extType=1 section for each RB */
    for (idxRb = 0; idxRb < rbNum; idxRb++) {
        print_dbg("%s RB %d\n", __FUNCTION__, idxRb);

        if(total_len >= ext1_dst_len){
            print_err("p_ext1_dst overflow\n");
            return -1;
        }

        cur_ext_len = 0; /** populate one extType=1 section with BFW for 1 RB */
        parm_size = sizeof(struct xran_cp_radioapp_section_ext1);
        p_ext1 = (struct xran_cp_radioapp_section_ext1 *)p_ext1_dst_cur;
        if(p_ext1 == NULL) {
            print_err("p_ext1 is null!\n");
            return (XRAN_STATUS_INVALID_PARAM);
        }

        cur_ext_len += parm_size;

        if(idxRb+1 == rbNum)
            ext_flag = XRAN_EF_F_LAST;

        p_ext1->extType       = XRAN_CP_SECTIONEXTCMD_1;
        p_ext1->ef            = ext_flag;
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

                parm_size = 1; /* exponent as part of bfwCompParam 1 octet */
                break;
            case XRAN_BFWCOMPMETHOD_BLKSCALE:
                rte_panic("XRAN_BFWCOMPMETHOD_BLKSCALE");
                break;

            case XRAN_BFWCOMPMETHOD_ULAW:
                rte_panic("XRAN_BFWCOMPMETHOD_BLKSCALE");
                break;

            case XRAN_BFWCOMPMETHOD_BEAMSPACE:
                rte_panic("XRAN_BFWCOMPMETHOD_BLKSCALE");
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
        if (p_ext1->bfwIqWidth == 0 || p_ext1->bfwIqWidth == 16){
            rte_memcpy(p_bfw_content, p_bfw_iq_src, parm_size);
        } else {
            bfp_com_rsp.data_out = (int8_t*)p_bfw_content;
            if(xranlib_compress_avx512_bfw(&bfp_com_req, &bfp_com_rsp) == 0){
                comp_len = bfp_com_rsp.len;
                print_dbg("comp_len %d parm_size %d\n", comp_len, parm_size);
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
            p_bfw_content = (uint8_t *)(p_bfw_content + parm_size);
            rte_memcpy(p_bfw_content, zeropad, parm_size);
            cur_ext_len += parm_size;
            print_dbg("zeropad %d cur_ext_len %d\n", parm_size, cur_ext_len);
        }

        if(cur_ext_len % XRAN_SECTIONEXT_ALIGN)
            rte_panic("ext1 should be aligned on 4-bytes boundary");

        p_ext1->extLen = cur_ext_len / XRAN_SECTIONEXT_ALIGN;
        print_dbg("[%d] %p iq %p p_ext1->extLen %d\n",idxRb, p_ext1, p_ext1+1,  p_ext1->extLen);

        /* update for next RB */
        p_ext1_dst_cur += cur_ext_len;
        p_bfw_iq_src   = p_bfw_iq_src + bfwNumPerRb*2;

        total_len += cur_ext_len;
    }

    print_dbg("total_len %d\n", total_len);
    return (total_len);
}


// Cyclic Prefix Length 5.4.4.14
//   CP_length = cpLength * Ts,  Ts = 1/30.72MHz
//    i.e cpLength = CP_length / Ts ?
#define CPLEN_TS           (30720000)
inline uint16_t xran_get_cplength(int CP_length)
{
    return (CP_length);
}

// Frequency offset 5.4.5.11
//   frequency_offset = freqOffset * SCS * 0.5
//    i.e freqOffset = (frequency_offset *2 )/ SCS ?
inline int32_t xran_get_freqoffset(int32_t freqOffset, int32_t scs)
{
    return (freqOffset);
}

static int xran_append_sectionext_1(struct rte_mbuf *mbuf,
                struct xran_sectionext1_info *params, int last_flag)
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


static int xran_prepare_sectionext_1(struct rte_mbuf *mbuf,
                struct xran_sectionext1_info *params, int last_flag)
{
    struct xran_cp_radioapp_section_ext1 *ext1;
    uint8_t *data;
    int parm_size, iq_size;
    int total_len;

    total_len = 0;

    print_dbg("%s %d\n", __FUNCTION__, last_flag);

    parm_size = sizeof(struct xran_cp_radioapp_section_ext1);
    ext1 = (struct xran_cp_radioapp_section_ext1 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext1 == NULL) {
        print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
        return (XRAN_STATUS_RESOURCE);
    }

    total_len += parm_size;

    ext1->extType       = XRAN_CP_SECTIONEXTCMD_1;
    ext1->ef            = last_flag;
    ext1->bfwCompMeth   = params->bfwCompMeth;
    ext1->bfwIqWidth    = XRAN_CONVERT_BFWIQWIDTH(params->bfwiqWidth);

    switch(params->bfwCompMeth) {
        case XRAN_BFWCOMPMETHOD_BLKFLOAT:
            parm_size = 1;
            data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
            if(data == NULL) {
                print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
                return (XRAN_STATUS_RESOURCE);
            }
            total_len += parm_size;
            *data = (params->bfwCompParam.exponent & 0x0f);
            break;

        case XRAN_BFWCOMPMETHOD_BLKSCALE:
            parm_size = 1;
            data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
            if(data == NULL) {
                print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
                return (XRAN_STATUS_RESOURCE);
            }
            total_len += parm_size;
            *data = params->bfwCompParam.blockScaler;
            break;

        case XRAN_BFWCOMPMETHOD_ULAW:
            parm_size = 1;
            data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
            if(data == NULL) {
                print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
                return (XRAN_STATUS_RESOURCE);
                }
            total_len += parm_size;
            *data = params->bfwCompParam.compBitWidthShift;
            break;

        case XRAN_BFWCOMPMETHOD_BEAMSPACE:
            parm_size = params->bfwNumber>>3;
            if(params->bfwNumber%8) parm_size++;
            parm_size *= 8;
            data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
            if(data == NULL) {
                print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
                return (XRAN_STATUS_RESOURCE);
                }
            rte_memcpy(data, params->bfwCompParam.activeBeamspaceCoeffMask, parm_size);
            total_len += parm_size;
            break;

        case XRAN_BFWCOMPMETHOD_NONE:
        default:
            parm_size = 0;
        }

    print_dbg("params->bfwNumber %d params->bfwiqWidth %d\n", params->bfwNumber, params->bfwiqWidth);

    iq_size = params->bfwNumber * params->bfwiqWidth * 2;

    parm_size = iq_size>>3;
    if(iq_size%8)
        parm_size++;

    data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
    if(data == NULL) {
        print_err("Fail to allocate the space for section extension 1 BF W iq_size: [%d]", parm_size);
        return (XRAN_STATUS_RESOURCE);
    }
    rte_memcpy(data, params->p_bfwIQ, parm_size);

    total_len += parm_size;
    parm_size = total_len % XRAN_SECTIONEXT_ALIGN;
    if(parm_size) {
        parm_size = XRAN_SECTIONEXT_ALIGN - parm_size;
        data = (uint8_t *)rte_pktmbuf_append(mbuf, parm_size);
        if(data == NULL) {
            print_err("Fail to allocate the space for section extension 1 [%d]", parm_size);
            return (XRAN_STATUS_RESOURCE);
        }
        rte_memcpy(data, zeropad, parm_size);
        total_len += parm_size;
    }

    ext1->extLen        = total_len / XRAN_SECTIONEXT_ALIGN;

    return (total_len);
}

static int xran_prepare_sectionext_2(struct rte_mbuf *mbuf,
                struct xran_sectionext2_info *params, int last_flag)
{
  struct xran_cp_radioapp_section_ext2 *ext2;
  uint8_t *data;
  int total_len;
  int parm_size;
  uint32_t val, shift_val;
  int val_size, pad_size;


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
        }
    else
        shift_val += 8;

    if(params->bfZePtWidth) {
        val = val << (params->bfZePtWidth+1);
        val += params->bfZePt & bitmask[params->bfZePtWidth];
        shift_val += 8 - (params->bfZePtWidth+1);
        }
    else
        shift_val += 8;

    if(params->bfAz3ddWidth) {
        val = val << (params->bfAz3ddWidth+1);
        val += params->bfAz3dd & bitmask[params->bfAz3ddWidth];
        shift_val += 8 - (params->bfAz3ddWidth+1);
        }
    else
        shift_val += 8;

    if(params->bfZe3ddWidth) {
        val = val << (params->bfZe3ddWidth+1);
        val += params->bfZe3dd & bitmask[params->bfZe3ddWidth];
        shift_val += 8 - (params->bfZe3ddWidth+1);
        }
    else
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

    rte_memcpy(data, &val, val_size);
    data += val_size;
    *data = ((params->bfAzSI) << 3) + (params->bfZeSI);
    data++;
    rte_memcpy(data, zeropad, pad_size);

    ext2->extLen = total_len / XRAN_SECTIONEXT_ALIGN;
    *(uint32_t *)ext2 = rte_cpu_to_be_32(*(uint32_t *)ext2);

    return (total_len);
}

static int xran_prepare_sectionext_3(struct rte_mbuf *mbuf,
                struct xran_sectionext3_info *params, int last_flag)
{
  int total_len;
  int adj;


    if(params->layerId == XRAN_LAYERID_0
        || params->layerId == XRAN_LAYERID_TXD) {   /* first data layer */

        struct xran_cp_radioapp_section_ext3_first *ext3_f;
        uint64_t *tmp;

        total_len = sizeof(struct xran_cp_radioapp_section_ext3_first);
        ext3_f = (struct xran_cp_radioapp_section_ext3_first *)rte_pktmbuf_append(mbuf, total_len);
        if(ext3_f == NULL) {
            print_err("Fail to allocate the space for section extension 3");
            return (XRAN_STATUS_RESOURCE);
            }

        ext3_f->layerId         = params->layerId;
        ext3_f->ef              = last_flag;
        ext3_f->extType         = XRAN_CP_SECTIONEXTCMD_3;
        ext3_f->crsSymNum       = params->crsSymNum;
        ext3_f->crsShift        = params->crsShift;
        ext3_f->crsReMask       = params->crsReMask;
        ext3_f->txScheme        = params->txScheme;
        ext3_f->numLayers       = params->numLayers;
        ext3_f->codebookIndex   = params->codebookIdx;

        if(params->numAntPort == 2) {
            ext3_f->beamIdAP3   = params->beamIdAP1;
            ext3_f->beamIdAP2   = 0;
            ext3_f->beamIdAP1   = 0;
            ext3_f->extLen      = 3;
            adj = 4;
            total_len -= adj;
            }
        else {
            ext3_f->beamIdAP3   = params->beamIdAP1;
            ext3_f->beamIdAP2   = params->beamIdAP2;
            ext3_f->beamIdAP1   = params->beamIdAP3;
            ext3_f->extLen      = 4;
            adj = 0;
            }
        ext3_f->reserved0       = 0;
        ext3_f->reserved1       = 0;
        ext3_f->reserved2       = 0;

        /* convert byte order */
        tmp = (uint64_t *)ext3_f;
        *tmp = rte_cpu_to_be_64(*tmp); tmp++;
        *tmp = rte_cpu_to_be_64(*tmp);

        if(adj)
            rte_pktmbuf_trim(mbuf, adj);
        }
    else {  /* non-first data layer */
        struct xran_cp_radioapp_section_ext3_non_first *ext3_nf;

        total_len = sizeof(struct xran_cp_radioapp_section_ext3_non_first);
        ext3_nf = (struct xran_cp_radioapp_section_ext3_non_first *)rte_pktmbuf_append(mbuf, total_len);
        if(ext3_nf == NULL) {
            print_err("Fail to allocate the space for section extension 3");
            return (XRAN_STATUS_RESOURCE);
            }

        ext3_nf->layerId        = params->layerId;
        ext3_nf->ef             = last_flag;
        ext3_nf->extType        = XRAN_CP_SECTIONEXTCMD_3;
        ext3_nf->numLayers      = params->numLayers;
        ext3_nf->codebookIndex  = params->codebookIdx;

        ext3_nf->extLen         = sizeof(struct xran_cp_radioapp_section_ext3_non_first)/XRAN_SECTIONEXT_ALIGN;

        *(uint32_t *)ext3_nf = rte_cpu_to_be_32(*(uint32_t *)ext3_nf);
        }

    return (total_len);
}

static int xran_prepare_sectionext_4(struct rte_mbuf *mbuf,
                struct xran_sectionext4_info *params, int last_flag)
{
  struct xran_cp_radioapp_section_ext4 *ext4;
  int parm_size;
  int total_len;
  int ret;


    total_len = 0;

    parm_size = sizeof(struct xran_cp_radioapp_section_ext4);
    ext4 = (struct xran_cp_radioapp_section_ext4 *)rte_pktmbuf_append(mbuf, parm_size);
    if(ext4 == NULL) {
        print_err("Fail to allocate the space for section extension 4");
        return(XRAN_STATUS_RESOURCE);
        }
    else {
        total_len += parm_size;

        ext4->extType       = XRAN_CP_SECTIONEXTCMD_4;
        ext4->ef            = last_flag;
        ext4->modCompScaler = params->modCompScaler;
        ext4->csf           = params->csf?1:0;
        ext4->extLen        = total_len / XRAN_SECTIONEXT_ALIGN;

        *(uint32_t *)ext4 = rte_cpu_to_be_32(*(uint32_t*)ext4);
        }

    return (total_len);
}

static int xran_prepare_sectionext_5(struct rte_mbuf *mbuf,
                struct xran_sectionext5_info *params, int last_flag)
{
  struct xran_cp_radioapp_section_ext_hdr *ext_hdr;
  struct xran_cp_radioapp_section_ext5 ext5;
  int padding;
  int total_len;
  uint8_t *data;
  int i;


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
            rte_memcpy(data, &ext5, sizeof(struct xran_cp_radioapp_section_ext5));
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
                rte_memcpy(data, &ext5, sizeof(struct xran_cp_radioapp_section_ext5)/2);
                data += sizeof(struct xran_cp_radioapp_section_ext5)/2;
                break;
                }
            }
        }

    /* zero padding */
    if(padding)
        rte_memcpy(data, zeropad, padding);

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
int xran_append_section_extensions(struct rte_mbuf *mbuf, struct xran_section_gen_info *params)
{
    int i, ret;
    uint32_t totalen;
    int last_flag;
    int ext_size;

    if(unlikely(params->exDataSize > XRAN_MAX_NUM_EXTENSIONS)) {
        print_err("Invalid total number of extensions - %d", params->exDataSize);
        return (XRAN_STATUS_INVALID_PARAM);
    }

    totalen = 0;

    ret = XRAN_STATUS_SUCCESS;

    print_dbg("params->exDataSize %d\n", params->exDataSize);
    for(i=0; i < params->exDataSize; i++) {
        if(params->exData[i].data == NULL) {
            print_err("Invalid parameter - extension data %d is NULL", i);
            ret = XRAN_STATUS_INVALID_PARAM;
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
            default:
                print_err("Extension Type %d is not supported!", params->exData[i].type);
                ret = XRAN_STATUS_INVALID_PARAM;
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
static int xran_prepare_section0(
                struct xran_cp_radioapp_section0 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    section->hdr.sectionId      = params->info.id;
    section->hdr.rb             = params->info.rb;
    section->hdr.symInc         = params->info.symInc;
    section->hdr.startPrbc      = params->info.startPrbc;
    section->hdr.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info.numPrbc);

    section->hdr.u.s0.reMask    = params->info.reMask;
    section->hdr.u.s0.numSymbol = params->info.numSymbol;
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
static int xran_prepare_section0_hdr(
                struct xran_cp_radioapp_section0_header *s0hdr,
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
static int xran_prepare_section1(
                struct xran_cp_radioapp_section1 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    section->hdr.sectionId      = params->info.id;
    section->hdr.rb             = params->info.rb;
    section->hdr.symInc         = params->info.symInc;
    section->hdr.startPrbc      = params->info.startPrbc;
    section->hdr.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info.numPrbc);

    section->hdr.u.s1.reMask    = params->info.reMask;
    section->hdr.u.s1.numSymbol = params->info.numSymbol;
    section->hdr.u.s1.beamId    = params->info.beamId;

    section->hdr.u.s1.ef        = params->info.ef;

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
static int xran_prepare_section1_hdr(
                struct xran_cp_radioapp_section1_header *s1hdr,
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
static int xran_prepare_section3(
                struct xran_cp_radioapp_section3 *section,
                struct xran_section_gen_info *params)
{
#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->info.numSymbol > XRAN_SYMBOLNUMBER_MAX)) {
        print_err("Invalid number of Symbols - %d", params->info.numSymbol);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    section->hdr.sectionId      = params->info.id;
    section->hdr.rb             = params->info.rb;
    section->hdr.symInc         = params->info.symInc;
    section->hdr.startPrbc      = params->info.startPrbc;
    section->hdr.numPrbc        = XRAN_CONVERT_NUMPRBC(params->info.numPrbc);

    section->hdr.u.s3.reMask    = params->info.reMask;
    section->hdr.u.s3.numSymbol = params->info.numSymbol;
    section->hdr.u.s3.beamId    = params->info.beamId;

    section->freqOffset         = rte_cpu_to_be_32(params->info.freqOffset)>>8;
    section->reserved           = 0;

    section->hdr.u.s3.ef        = params->info.ef;

    // for network byte order (header, 8 bytes)
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
int xran_append_control_section(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params)
{
  int i, ret, ext_flag;
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
            return (XRAN_STATUS_INVALID_PARAM);
        }

    if(unlikely(xran_prepare_section_func == NULL)) {
       print_err("Section Type %d is not supported!", params->sectionType);
       return (XRAN_STATUS_INVALID_PARAM);
    }

    for(i=0; i < params->numSections; i++) {
        section = rte_pktmbuf_append(mbuf, section_size);
        if(section == NULL) {
            print_err("Fail to allocate the space for section[%d]!", i);
            return (XRAN_STATUS_RESOURCE);
        }
        print_dbg("%s %d ef %d\n", __FUNCTION__, i, params->sections[i].info.ef);
        ret = xran_prepare_section_func((void *)section,
                            (void *)&params->sections[i]);
        if(ret < 0){
            print_err("%s %d\n", __FUNCTION__, ret);
            return (ret);
        }
        totalen += section_size;

        if(params->sections[i].info.ef) {
            print_dbg("sections[%d].info.ef %d exDataSize %d  type %d\n", i, params->sections[i].info.ef,
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
static inline int xran_prepare_radioapp_common_header(
                struct xran_cp_radioapp_common_header *apphdr,
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

    apphdr->dataDirection   = params->dir;
    apphdr->payloadVer      = XRAN_PAYLOAD_VER;
    apphdr->filterIndex     = params->hdr.filterIdx;
    apphdr->frameId         = params->hdr.frameId;
    apphdr->subframeId      = params->hdr.subframeId;
    apphdr->slotId          = xran_slotid_convert(params->hdr.slotId, 0);
    apphdr->startSymbolId   = params->hdr.startSymId;
    apphdr->numOfSections   = params->numSections;
    apphdr->sectionType     = params->sectionType;

    /* radio app header has common parts of 4bytes for all section types */
    *((uint32_t *)apphdr) = rte_cpu_to_be_32(*((uint32_t *)apphdr));

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
int xran_append_radioapp_header(struct rte_mbuf *mbuf, struct xran_cp_gen_params *params)
{
  int ret;
  uint32_t totalen;
  struct xran_cp_radioapp_common_header *apphdr;
  int (*xran_prepare_radioapp_section_hdr_func)(void *hdr, void *params);


#if (XRAN_STRICT_PARM_CHECK)
    if(unlikely(params->sectionType >= XRAN_CP_SECTIONTYPE_MAX)) {
        print_err("Invalid Section Type - %d", params->sectionType);
        return (XRAN_STATUS_INVALID_PARAM);
        }
#endif

    switch(params->sectionType) {
        case XRAN_CP_SECTIONTYPE_0: /* Unused RB or Symbols in DL or UL, not supportted */
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section0_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section0_header);
            break;

        case XRAN_CP_SECTIONTYPE_1: /* Most DL/UL Radio Channels */
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section1_hdr;
            totalen = sizeof(struct xran_cp_radioapp_section1_header);
            break;

        case XRAN_CP_SECTIONTYPE_3: /* PRACH and Mixed-numerology Channels */
            xran_prepare_radioapp_section_hdr_func = (int (*)(void *, void*))xran_prepare_section3_hdr;
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
int xran_prepare_ctrl_pkt(struct rte_mbuf *mbuf,
                        struct xran_cp_gen_params *params,
                        uint8_t CC_ID, uint8_t Ant_ID,
                        uint8_t seq_id)
{
  int ret;
  uint32_t payloadlen;
  struct xran_ecpri_hdr *ecpri_hdr;


    payloadlen = xran_build_ecpri_hdr(mbuf, CC_ID, Ant_ID, seq_id, &ecpri_hdr);

    ret = xran_append_radioapp_header(mbuf, params);
    if(ret < 0) {
        print_err("%s %d\n", __FUNCTION__, ret);
        return (ret);
    }
    payloadlen += ret;

    ret = xran_append_control_section(mbuf, params);
    if(ret < 0) {
        print_err("%s %d\n", __FUNCTION__, ret);
        return (ret);
    }
    payloadlen += ret;

    /* set payload length */
    ecpri_hdr->cmnhdr.ecpri_payl_size = rte_cpu_to_be_16(payloadlen);

    return (XRAN_STATUS_SUCCESS);
}


///////////////////////////////////////
// for RU emulation
int xran_parse_section_ext1(void *ext,
                struct xran_sectionext1_info *extinfo)
{
  int len;
  int total_len;
  struct xran_cp_radioapp_section_ext1 *ext1;
  uint8_t *data;
  int parm_size, iq_size;
  int N;
  void *pHandle;

    pHandle = NULL;
    N = xran_get_conf_num_bfweights(pHandle);
    extinfo->bfwNumber = N;

    ext1 = (struct xran_cp_radioapp_section_ext1 *)ext;
    data = (uint8_t *)ext;

    len = 0;
    total_len = ext1->extLen * XRAN_SECTIONEXT_ALIGN;   /* from word to byte */

    extinfo->bfwCompMeth    = ext1->bfwCompMeth;
    extinfo->bfwiqWidth     = (ext1->bfwIqWidth==0)?16:ext1->bfwIqWidth;

    len     += sizeof(struct xran_cp_radioapp_section_ext1);
    data    += sizeof(struct xran_cp_radioapp_section_ext1);

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
            rte_memcpy(data, extinfo->bfwCompParam.activeBeamspaceCoeffMask, parm_size);
            break;

        default:
            print_err("Invalid BfComp method - %d", ext1->bfwCompMeth);
            parm_size = 0;
        }

    len     += parm_size;
    data    += parm_size;

    /* Get BF weights */
    iq_size = N * extinfo->bfwiqWidth * 2;  // total in bits
    parm_size = iq_size>>3;                 // total in bytes (/8)
    if(iq_size%8) parm_size++;              // round up

    //rte_memcpy(data, extinfo->p_bfwIQ, parm_size);
    extinfo->p_bfwIQ =  (int16_t*)data;

    len += parm_size;

    parm_size = len % XRAN_SECTIONEXT_ALIGN;
    if(parm_size)
        len += (XRAN_SECTIONEXT_ALIGN - parm_size);

    if(len != total_len) {
        // TODO: fix this print_err("The size of extension 1 is not correct! [%d:%d]", len, total_len);
    }

    return (total_len);
}

int xran_parse_section_ext2(void *ext,
                struct xran_sectionext2_info *extinfo)
{
  int len;
  int total_len;
  struct xran_cp_radioapp_section_ext2 *ext2;
  uint8_t *data;
  int parm_size;
  uint32_t val;
  int val_size;


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

int xran_parse_section_ext3(void *ext,
                struct xran_sectionext3_info *extinfo)
{
  int len;
  int total_len;

    total_len = 0;
    len = *((uint8_t *)ext + 1);

    switch(len) {
        case 1:     /* non-first data layer */
            {
            struct xran_cp_radioapp_section_ext3_non_first *ext3_nf;

            ext3_nf = (struct xran_cp_radioapp_section_ext3_non_first *)ext;
            *(uint32_t *)ext3_nf = rte_be_to_cpu_32(*(uint32_t *)ext3_nf);

            total_len = ext3_nf->extLen * XRAN_SECTIONEXT_ALIGN;    /* from word to byte */

            extinfo->codebookIdx= ext3_nf->codebookIndex;
            extinfo->layerId    = ext3_nf->layerId;
            extinfo->numLayers  = ext3_nf->numLayers;
            }
            break;

        case 3:     /* first data layer with two antenna */
        case 4:     /* first data layer with four antenna */
            {
            struct xran_cp_radioapp_section_ext3_first *ext3_f;
            uint16_t *beamid;

            ext3_f = (struct xran_cp_radioapp_section_ext3_first *)ext;
            *(uint64_t *)ext3_f = rte_be_to_cpu_64(*(uint64_t *)ext3_f);

            total_len = ext3_f->extLen * XRAN_SECTIONEXT_ALIGN; /* from word to byte */

            extinfo->codebookIdx= ext3_f->codebookIndex;
            extinfo->layerId    = ext3_f->layerId;
            extinfo->numLayers  = ext3_f->numLayers;
            extinfo->txScheme   = ext3_f->txScheme;
            extinfo->crsReMask  = ext3_f->crsReMask;
            extinfo->crsShift   = ext3_f->crsShift;
            extinfo->crsSymNum  = ext3_f->crsSymNum;

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

int xran_parse_section_ext4(void *ext,
                struct xran_sectionext4_info *extinfo)
{
  int len;
  struct xran_cp_radioapp_section_ext4 *ext4;
  int total_len;


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

int xran_parse_section_ext5(void *ext,
                struct xran_sectionext5_info *extinfo)
{
  int len;
  struct xran_cp_radioapp_section_ext_hdr *ext_hdr;
  struct xran_cp_radioapp_section_ext5 ext5;
  int parm_size;
  int total_len;
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

    len = 0;
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

int xran_parse_section_extension(struct rte_mbuf *mbuf,
                    void *ext,
                    struct xran_section_gen_info *section)
{
  int total_len, len, numext;
  uint8_t *ptr;
  int flag_last;
  int ext_type;
  int i;

    total_len = 0;
    ptr = (uint8_t *)ext;

    numext = 0;

    flag_last = 1;
    i = 0;
    while(flag_last) {
        /* check ef */
        flag_last = (*ptr & 0x80);

        ext_type = *ptr & 0x7f;
        section->exData[numext].type = ext_type;
        switch(ext_type) {
            case XRAN_CP_SECTIONEXTCMD_1:
                section->exData[numext].data = &section->m_ext1[numext];
                len = xran_parse_section_ext1(ptr, section->exData[numext].data);
                section->exData[numext].len = len;
                break;
            case XRAN_CP_SECTIONEXTCMD_2:
                section->exData[numext].data = &section->m_ext2[numext];
                len = xran_parse_section_ext2(ptr, section->exData[numext].data);
                break;
            case XRAN_CP_SECTIONEXTCMD_3:
                section->exData[numext].data = &section->m_ext3[numext];
                len = xran_parse_section_ext3(ptr, section->exData[numext].data);
                break;
            case XRAN_CP_SECTIONEXTCMD_4:
                section->exData[numext].data = &section->m_ext4[numext];
                len = xran_parse_section_ext4(ptr, section->exData[numext].data);
                break;
            case XRAN_CP_SECTIONEXTCMD_5:
                section->exData[numext].data = &section->m_ext5[numext];
                len = xran_parse_section_ext5(ptr, section->exData[numext].data);
                break;

            default:
                print_err("Extension %d is not supported!", ext_type);
                len = 0;
            }

        section->exData[numext].len = len;
        ptr += len; total_len += len;

        i++;
        if(++numext < XRAN_MAX_NUM_EXTENSIONS) continue;

        /* exceeds maximum number of extensions */
        break;
        }

    section->exDataSize = numext;

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
int xran_parse_cp_pkt(struct rte_mbuf *mbuf,
                    struct xran_cp_gen_params *result,
                    struct xran_recv_packet_info *pkt_info)
{
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_cp_radioapp_common_header *apphdr;
  int i, ret;
  int extlen;


    ret = xran_parse_ecpri_hdr(mbuf, &ecpri_hdr, pkt_info);
    if(ret < 0 && ecpri_hdr == NULL)
        return (XRAN_STATUS_INVALID_PACKET);

    /* Process radio header. */
    apphdr = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_ecpri_hdr));
    if(apphdr == NULL) {
        print_err("Invalid packet - radio app hedaer!");
        return (XRAN_STATUS_INVALID_PACKET);
        }

    *((uint32_t *)apphdr) = rte_be_to_cpu_32(*((uint32_t *)apphdr));

    if(apphdr->payloadVer != XRAN_PAYLOAD_VER) {
        print_err("Invalid Payload version - %d", apphdr->payloadVer);
        ret = XRAN_STATUS_INVALID_PACKET;
        }

    result->dir             = apphdr->dataDirection;
    result->hdr.filterIdx   = apphdr->filterIndex;
    result->hdr.frameId     = apphdr->frameId;
    result->hdr.subframeId  = apphdr->subframeId;
    result->hdr.slotId      = apphdr->slotId;
    result->hdr.startSymId  = apphdr->startSymbolId;
    result->sectionType     = apphdr->sectionType;
    result->numSections     = apphdr->numOfSections;

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
                    print_err("Invalid packet 0 - radio app hedaer!");
                    return (XRAN_STATUS_INVALID_PACKET);
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
                if(section == NULL) {
                    print_err("Invalid packet 1 - radio app hedaer!");
                    return (XRAN_STATUS_INVALID_PACKET);
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

                    section = (void *)rte_pktmbuf_adj(mbuf,
                                    sizeof(struct xran_cp_radioapp_section1));
                    if(section == NULL) {
                        print_err("Invalid packet 1 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = XRAN_STATUS_INVALID_PACKET;
                        break;
                        }

                    if(result->sections[i].info.ef) {
                        // parse section extension
                        extlen = xran_parse_section_extension(mbuf, (void *)section, &result->sections[i]);
                        if(extlen > 0) {
                            section = (void *)rte_pktmbuf_adj(mbuf, extlen);
                            if(section == NULL) {
                                print_err("Invalid packet 1 - section extension [%d]!", i);
                                ret = XRAN_STATUS_INVALID_PACKET;
                                break;
                                }
                            }
                        }
                    else extlen = 0;
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
                    return (XRAN_STATUS_INVALID_PACKET);
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
                        ret = XRAN_STATUS_INVALID_PACKET;
                        }

                    section = (void *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section3));
                    if(section == NULL) {
                        print_err("Invalid packet 3 - number of section [%d:%d]!",
                                    result->numSections, i);
                        result->numSections = i;
                        ret = XRAN_STATUS_INVALID_PACKET;
                        break;
                        }

                    if(result->sections[i].info.ef) {
                        // parse section extension
                        extlen = xran_parse_section_extension(mbuf, (void *)section, &result->sections[i]);
                        if(extlen > 0) {
                            section = (void *)rte_pktmbuf_adj(mbuf, extlen);
                            if(section == NULL) {
                                print_err("Invalid packet 3 - section extension [%d]!", i);
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
            for(int j=0; j<result->sections[i].exDataSize; j++) {
                printf("      || %2d : type=%d len=%d\n",
                        j, result->sections[i].exData[j].type, result->sections[i].exData[j].len);
                switch(result->sections[i].exData[j].type) {
                    case XRAN_CP_SECTIONEXTCMD_1:
                        {
                        struct xran_sectionext1_info *ext1;
                        ext1 = result->sections[i].exData[j].data;
                        printf("      ||    bfwNumber=%d bfwiqWidth=%d bfwCompMeth=%d\n",
                                ext1->bfwNumber, ext1->bfwiqWidth, ext1->bfwCompMeth);
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
                        for(int k=0; k<ext5->num_sets; k++) {
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

