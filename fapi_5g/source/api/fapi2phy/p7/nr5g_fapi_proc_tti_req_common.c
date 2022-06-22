/******************************************************************************
*
*   Copyright (c) 2021 Intel.
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

#include "nr5g_fapi_framework.h"
#include "nr5g_fapi_fapi2phy_p7_pvt_proc.h"

#define FAPI_EMPTY_RB_BITMAP_MASK (0u)
#define FAPI_EMPTY_RBG_INDEX (0u)
#define FAPI_MAX_RB_BIT_NUM (273u)

static uint16_t nr5g_fapi_rb_bitmap_mask(
    uint8_t rbg_size)
{
    switch (rbg_size) {
        case 2:
            return 0x3u;
        case 4:
            return 0xFu;
        case 8:
            return 0xFFu;
        case 16:
            return 0xFFFFu;
        default:
            return FAPI_EMPTY_RB_BITMAP_MASK;
    }
}

uint16_t nr5g_fapi_get_rb_bits_for_rbg(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint32_t nth_rbg_bit,
    uint8_t rbg_size,
    uint16_t rb_bitmap_mask)
{
    uint16_t rb_bits = 0u;
    const uint16_t rb_bits_bit_size = sizeof(rb_bits) * CHAR_BIT;
    const uint16_t nth_rb_bit = nth_rbg_bit * rbg_size;
    const uint32_t rb_byte_1 = ( nth_rb_bit / rb_bits_bit_size ) * sizeof(rb_bits);
    const uint32_t rb_byte_2 = rb_byte_1 + 1u;
    if (rb_byte_1 < FAPI_RB_BITMAP_SIZE)
    {
        rb_bits |= rb_bitmap[rb_byte_1];
    }
    if (rb_byte_2 < FAPI_RB_BITMAP_SIZE)
    {
        rb_bits |= (rb_bitmap[rb_byte_2] << CHAR_BIT);
    }
    const uint32_t local_rbg_idx = nth_rb_bit % rb_bits_bit_size;
    return (rb_bits >> local_rbg_idx) & rb_bitmap_mask;
}

static bool nr5g_fapi_has_rbg_bits_in_rb_bitmap(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    const uint16_t rbg_bit,
    const uint8_t rbg_size,
    const uint16_t rb_bitmap_mask)
{
    const uint16_t rb_bits_for_rbg = nr5g_fapi_get_rb_bits_for_rbg(
        rb_bitmap, rbg_bit, rbg_size, rb_bitmap_mask);
    if (rb_bitmap_mask == rb_bits_for_rbg)
    {
        return true;
    }
    else if (FAPI_EMPTY_RB_BITMAP_MASK != rb_bits_for_rbg)
    {
        NR5G_FAPI_LOG(ERROR_LOG, ("rb_bits don not match rb_bitmap_mask."
            " rbg_size %u rbg_bit=%u rb_bits_for_rbg=%#X rb_bitmap_mask=%#X",
            rbg_size, rbg_bit, rb_bits_for_rbg, rb_bitmap_mask));
    }
    return false;
}

/** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  rb_bitmap Pointer to FAPI DL resource block bitmap.
 *  @param[in]  bwp_start Value of bandwidth partition start.
 *  @param[in]  bwp_size Value of bandwidth partition size.
 *  @param[in]  rbg_size Value of resource block group size.
 *  @param[in]  get_rbg_index_mask Function placing bit in rbgIndex (bit order).
 *
 *  @return     Returns IAPI nRBGIndex
 *
 *  @description
 *  See TS 138 214 5.1.2.2.1/6.1.2.2.1 for more info.
 *  IAPI uses bit per Resource Block Group,
 *  FAPI uses bit per Virtual Resource Block.
 *  Therefore 1 nRBGIndex bit (IAPI), maps to nRBGSize bits (FAPI)
 *  Bitmaps mappings representation:
 *  IAPI: nRBGIndex    = RBG-0................RBG-17 (for PDSCH MSB to LSB,
 *                                                    for PUSCH LSB to MSB)
 *  FAPI  RB-0...RB-272:
 *  FAPI  rbBitmap[i]  = RB-(7+i*8)...........RB-(0+i*8) (MSB to LSB)
 *
**/
uint32_t nr5g_fapi_calc_rbg_index(
    const uint8_t rb_bitmap[FAPI_RB_BITMAP_SIZE],
    uint16_t bwp_start,
    uint16_t bwp_size,
    uint32_t(*get_rbg_index_mask)(uint32_t nth_bit))
{
    const uint8_t rbg_size = nr5g_fapi_calc_n_rbg_size(bwp_size);
    const uint16_t rb_bitmap_mask = nr5g_fapi_rb_bitmap_mask(rbg_size);
    if (FAPI_EMPTY_RB_BITMAP_MASK == rb_bitmap_mask)
    {
        NR5G_FAPI_LOG(ERROR_LOG, ("Wrong rbg_size=%u. rbg_index set to 0.",
            rbg_size));
        return FAPI_EMPTY_RBG_INDEX;
    }
    if (bwp_start >= FAPI_MAX_RB_BIT_NUM)
    {
        NR5G_FAPI_LOG(ERROR_LOG, ("Wrong bwp_start=%u. rbg_index set to 0.",
            bwp_start));
        return FAPI_EMPTY_RBG_INDEX;
    }

    const uint16_t rbg_bit_begin = bwp_start / rbg_size;
    const uint16_t rb_bit_end = fmin(FAPI_MAX_RB_BIT_NUM, bwp_start + bwp_size);
    const uint16_t rbg_bit_last = ceil((double)rb_bit_end / rbg_size) - 1u;

    const uint16_t start_offset = bwp_start % rbg_size;
    uint16_t rb_bitmap_mask_1st_rbg =
        rb_bitmap_mask & (rb_bitmap_mask << start_offset);
    const uint16_t last_rbg_size =
        (0u == rb_bit_end % rbg_size) ? rbg_size : rb_bit_end % rbg_size;
    const uint16_t end_offset = rbg_size - last_rbg_size;
    uint16_t rb_bitmap_mask_last_rbg = rb_bitmap_mask >> end_offset;
    if (rbg_bit_begin == rbg_bit_last)
    {
        const uint16_t mask = rb_bitmap_mask_1st_rbg & rb_bitmap_mask_last_rbg;
        rb_bitmap_mask_1st_rbg = mask;
        rb_bitmap_mask_last_rbg = mask;
    }

    uint32_t result = 0u;
    // fill 1st rbg
    if (nr5g_fapi_has_rbg_bits_in_rb_bitmap(
        rb_bitmap, rbg_bit_begin, rbg_size, rb_bitmap_mask_1st_rbg))
    {
        result |= get_rbg_index_mask(rbg_bit_begin);
    }
    // fill last rbg
    if (nr5g_fapi_has_rbg_bits_in_rb_bitmap(
        rb_bitmap, rbg_bit_last, rbg_size, rb_bitmap_mask_last_rbg))
    {
        result |= get_rbg_index_mask(rbg_bit_last);
    }
    // fill rest of rbgs
    uint8_t rbg_bit;
    for (rbg_bit = rbg_bit_begin + 1u; rbg_bit < rbg_bit_last; rbg_bit++)
    {
        if (nr5g_fapi_has_rbg_bits_in_rb_bitmap(
            rb_bitmap, rbg_bit, rbg_size, rb_bitmap_mask))
        {
            result |= get_rbg_index_mask(rbg_bit);
        }
    }

    return result;
}

 /** @ingroup group_source_api_p7_fapi2phy_proc
 *
 *  @param[in]  bwp_size  Variable holding the Bandwidth part size.
 *
 *  @return     Returns ::RBG Size.
 *
 *  @description
 *  This functions calculates and return RBG Size from Bandwidth part size
 *  provided.
 *
**/
uint8_t nr5g_fapi_calc_n_rbg_size(
    uint16_t bwp_size)
{
    uint8_t n_rbg_size = 0;
    if (bwp_size >= 1 && bwp_size <= 36) {
        n_rbg_size = 2;
    } else if (bwp_size >= 37 && bwp_size <= 72) {
        n_rbg_size = 4;
    } else if (bwp_size >= 73 && bwp_size <= 144) {
        n_rbg_size = 8;
    } else if (bwp_size >= 145 && bwp_size <= 275) {
        n_rbg_size = 16;
    } else {
        n_rbg_size = 0;
    }
    return n_rbg_size;
}