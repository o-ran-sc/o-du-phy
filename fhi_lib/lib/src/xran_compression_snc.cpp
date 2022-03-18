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
 * @brief xRAN BFP compression/decompression U-plane implementation and interface functions
 *
 * @file xran_compression.cpp
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/
#include "xran_compression.hpp"
#include "xran_compression.h"
#include <complex>
#include <algorithm>
#include <immintrin.h>
#include <limits.h>
#include <cstring>

using namespace BlockFloatCompander;

/** callback function type for Symbol packet */
typedef void (*xran_bfp_compress_fn)(const BlockFloatCompander::ExpandedData& dataIn,
                                     BlockFloatCompander::CompressedData* dataOut);

/** callback function type for Symbol packet */
typedef void (*xran_bfp_decompress_fn)(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut);

int32_t
xranlib_compress_avxsnc(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompander::ExpandedData expandedDataInput;
    BlockFloatCompander::CompressedData compressedDataOut;
    xran_bfp_compress_fn com_fn = NULL;
    uint16_t totalRBs = request->numRBs;
    uint16_t remRBs   = totalRBs;
    int16_t len = 0;
    int16_t block_idx_bytes = 0;

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
            com_fn = BlockFloatCompander::BFPCompressUserPlaneAvxSnc;
            break;
        default:
            com_fn = BlockFloatCompander::BFPCompressRef;
            break;
    }

    expandedDataInput.iqWidth = request->iqWidth;
    expandedDataInput.numDataElements =  24;

    while (remRBs){
        expandedDataInput.dataExpanded   = &request->data_in[block_idx_bytes];
        compressedDataOut.dataCompressed = (uint8_t*)&response->data_out[len];
        if(remRBs >= 16){
            expandedDataInput.numBlocks = 16;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)16);
            block_idx_bytes += 16*expandedDataInput.numDataElements;
            remRBs -= 16;
        }else if(remRBs >= 4){
            expandedDataInput.numBlocks = 4;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)4);
            block_idx_bytes +=4*expandedDataInput.numDataElements;
            remRBs -=4;
        }else if (remRBs >= 1){
            expandedDataInput.numBlocks = 1;
            com_fn(expandedDataInput, &compressedDataOut);
            len  += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)1);
            block_idx_bytes +=1*expandedDataInput.numDataElements;
            remRBs = remRBs - 1;
        }
    }

    response->len =  ((3 * expandedDataInput.iqWidth) + 1) * totalRBs;

    return 0;
}

int32_t
xranlib_decompress_avxsnc(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response)
{
    BlockFloatCompander::CompressedData compressedDataInput;
    BlockFloatCompander::ExpandedData expandedDataOut;

    xran_bfp_decompress_fn decom_fn = NULL;
    uint16_t totalRBs = request->numRBs;
    uint16_t remRBs   = totalRBs;
    int16_t len = 0;
    int16_t block_idx_bytes = 0;

    switch (request->iqWidth) {
    case 8:
    case 9:
    case 10:
    case 12:
        decom_fn = BlockFloatCompander::BFPExpandUserPlaneAvxSnc;
        break;
    default:
        decom_fn = BlockFloatCompander::BFPExpandRef;
        break;
    }

    compressedDataInput.iqWidth         =  request->iqWidth;
    compressedDataInput.numDataElements =  24;

    while(remRBs) {
        compressedDataInput.dataCompressed = (uint8_t*)&request->data_in[block_idx_bytes];
        expandedDataOut.dataExpanded       = &response->data_out[len];
        if(remRBs >= 16){
            compressedDataInput.numBlocks = 16;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 16*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)16);
            remRBs -= 16;
        }else if(remRBs >= 4){
            compressedDataInput.numBlocks = 4;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 4*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)4);
            remRBs -=4;
        }else if (remRBs >= 1){
            compressedDataInput.numBlocks = 1;
            decom_fn(compressedDataInput, &expandedDataOut);
            len  += 1*compressedDataInput.numDataElements;
            block_idx_bytes  += ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks,(int16_t)1);
            remRBs = remRBs - 1;
        }
    }

    response->len = totalRBs * compressedDataInput.numDataElements * sizeof(int16_t);

    return 0;
}

int32_t
xranlib_compress_avxsnc_bfw(const struct xranlib_compress_request *request,
                        struct xranlib_compress_response *response)
{
    BlockFloatCompander::ExpandedData expandedDataInput;
    BlockFloatCompander::CompressedData compressedDataOut;
    xran_bfp_compress_fn com_fn = NULL;

    if (request->numRBs != 1){
        printf("Unsupported numRBs %d\n", request->numRBs);
        return -1;
    }

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
        switch (request->numDataElements) {
            case 16:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane8AvxSnc;
                break;
            case 32:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane16AvxSnc;
                break;
            case 64:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane32AvxSnc;
                break;
            case 128:
                com_fn = BlockFloatCompander::BFPCompressCtrlPlane64AvxSnc;
                break;
            case 24:
            default:
                printf("Unsupported numDataElements %d\n", request->numDataElements);
                return -1;
                break;
        }
        break;
    default:
        printf("Unsupported iqWidth %d\n", request->iqWidth);
        return -1;
        break;
    }

    expandedDataInput.iqWidth         = request->iqWidth;
    expandedDataInput.numDataElements = request->numDataElements;
    expandedDataInput.numBlocks       = 1;
    expandedDataInput.dataExpanded    = &request->data_in[0];
    compressedDataOut.dataCompressed  = (uint8_t*)&response->data_out[0];

    com_fn(expandedDataInput, &compressedDataOut);

    response->len =  (((expandedDataInput.numDataElements  * expandedDataInput.iqWidth) >> 3) + 1)
                            * request->numRBs;

    return 0;
}

int32_t
xranlib_decompress_avxsnc_bfw(const struct xranlib_decompress_request *request,
                        struct xranlib_decompress_response *response)
{
    BlockFloatCompander::CompressedData compressedDataInput;
    BlockFloatCompander::ExpandedData expandedDataOut;
    xran_bfp_decompress_fn decom_fn = NULL;

    if (request->numRBs != 1){
        printf("Unsupported numRBs %d\n", request->numRBs);
        return -1;
    }

    switch (request->iqWidth) {
        case 8:
        case 9:
        case 10:
        case 12:
        switch (request->numDataElements) {
            case 16:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane8AvxSnc;
                break;
            case 32:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane16AvxSnc;
                break;
            case 64:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane32AvxSnc;
                break;
            case 128:
                decom_fn = BlockFloatCompander::BFPExpandCtrlPlane64AvxSnc;
                break;
            case 24:
            default:
                printf("Unsupported numDataElements %d\n", request->numDataElements);
                return -1;
                break;
        }
        break;
    default:
        printf("Unsupported iqWidth %d\n", request->iqWidth);
        return -1;
        break;
    }

    compressedDataInput.iqWidth         = request->iqWidth;
    compressedDataInput.numDataElements = request->numDataElements;
    compressedDataInput.numBlocks       = 1;
    compressedDataInput.dataCompressed  = (uint8_t*)&request->data_in[0];
    expandedDataOut.dataExpanded        = &response->data_out[0];

    decom_fn(compressedDataInput, &expandedDataOut);

    response->len = request->numRBs * compressedDataInput.numDataElements * sizeof(int16_t);

    return 0;
}

