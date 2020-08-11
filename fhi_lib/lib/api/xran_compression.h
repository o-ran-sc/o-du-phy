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

/*!
    \file   xran_compression.h
    \brief  External API for compading with the use BFP algorithm.
*/

#ifndef _XRAN_COMPRESSION_H_
#define _XRAN_COMPRESSION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
    \struct xranlib_compress_request
    \brief Request structure containing pointer to data and its length.
*/
struct xranlib_compress_request {
    int16_t *data_in;   /*!< Pointer to data to compress. */
    int16_t numRBs;     /*!< numRBs  */
    int16_t numDataElements; /*!< number of elements in block process [UP: 24 i.e 12RE*2; CP: 16,32,64,128. i.e AntElm*2] */
    int16_t compMethod; /*!< Compression method */
    int16_t iqWidth;    /*!< Bit size */
    int32_t len;        /*!< Length of input buffer in bytes */
};

/*!
    \struct xranlib_compress_response
    \brief Response structure containing pointer to data and its length.
*/
struct xranlib_compress_response {
    int8_t *data_out; /*!< Pointer to data after compression. */

    int32_t len; /*!< Length of output data. */
};

/*!
    \struct xranlib_decompress_request
    \brief Request structure containing pointer to data and its length.
*/
struct xranlib_decompress_request {
    int8_t *data_in; /*!< Pointer to data to decompress. */
    int16_t numRBs;     /*!< numRBs  */
    int16_t numDataElements; /*!< number of elements in block process [UP: 24 i.e 12RE*2; CP: 16,32,64,128. i.e AntElm*2] */
    int16_t compMethod; /*!< Compression method */
    int16_t iqWidth;    /*!< Bit size */

    int32_t len; /*!< Length of input data. */
};

/*!
    \struct xranlib_decompress_response
    \brief Response structure containing pointer to data and its length.
*/
struct xranlib_decompress_response {
    int16_t *data_out; /*!< Pointer to data after decompression. */

    int32_t len; /*!< Length of output data. */
};

/*!
    \brief Report the version number for the xranlib_companding library.
    \param [in] version Pointer to a char buffer where the version string should be copied.
    \param [in] buffer_size The length of the string buffer, must be at least
               xranlib_SDK_VERSION_STRING_MAX_LEN characters.
    \return 0 if the version string was populated, otherwise -1.
*/
int16_t
xranlib_companding_version(char *version, int buffer_size);

//! @{
/*!
    \brief Compress functions - it converts a 16-bit linear PCM value to 8-bt A-law.
    \param [in]  request Structure containing the input data and data length.
    \param [out] response Structure containing the output data and data length.
    \return 0 for success, -1 for error
*/
int32_t
xranlib_compress(const struct xranlib_compress_request *request,
    struct xranlib_compress_response *response);
int32_t
xranlib_compress_sse(const struct xranlib_compress_request *request,
    struct xranlib_compress_response *response);
int32_t
xranlib_compress_avx2(const struct xranlib_compress_request *request,
    struct xranlib_compress_response *response);
int32_t
xranlib_compress_avx512(const struct xranlib_compress_request *request,
    struct xranlib_compress_response *response);
int32_t
xranlib_compress_avx512_bfw(const struct xranlib_compress_request *request,
    struct xranlib_compress_response *response);
//! @}

//! @{
/*!
    \brief Decompress function - it converts an A-law value to 16-bit linear PCM.
    \param [in] request Structure containing the input data and data length.
    \param [out] response Structure containing the output data and data length.
    \return 0 for success, -1 for error.
**/
int32_t
xranlib_decompress(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response);

int32_t
xranlib_decompress_sse(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response);
int32_t
xranlib_decompress_avx2(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response);
int32_t
xranlib_decompress_avx512(const struct xranlib_decompress_request *request,
    struct xranlib_decompress_response *response);
int32_t
xranlib_decompress_avx512_bfw(const struct xranlib_decompress_request *request,
     struct xranlib_decompress_response *response);

//! @}

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_COMPRESSION_H_ */
