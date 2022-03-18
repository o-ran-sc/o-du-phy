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
#include <inttypes.h>
#include <stdio.h>


/*!
    \struct bblib_llr_demapping_5gnr_mod_compression_request
    \brief Structure defining modulation compression lib.
*/

enum xran_modulation_order {
    XRAN_BPSK   = 1, /*!< BPSK */
    XRAN_QPSK   = 2, /*!< QPSK */
    XRAN_PAM4   = 3, /*!< PAM4 */
    XRAN_QAM16  = 4, /*!< QAM16 */
    XRAN_PAM8   = 5, /*!< PAM8 */
    XRAN_QAM64  = 6, /*!< QAM64 */
    XRAN_PAM16  = 7, /*!< PAM16 */
    XRAN_QAM256 = 8  /*!< QAM256 */
};

struct xranlib_5gnr_mod_compression_request
{
    /*! Pointer to buffer of the input symbols - buffer must be 64 byte aligned. Format 16Sx. */
    int16_t * data_in;
    /*! 16bit shift value used to scale the input samples. */
    int16_t unit;
    /*! Supported modulation values are: 2 (QPSK), 4 (16QAM), 6 (64QAM), 8 (256QAM). */
    enum xran_modulation_order modulation;
    int32_t num_symbols;  /*!< Number of complex input symbols. */
    int16_t re_mask;  /*!< RE mask in one RB. */
};

/*!
    \struct bblib_llr_demapping_5gnr_mod_compression_response
    \brief Structure defining the modualtion compression output.
 */
struct xranlib_5gnr_mod_compression_response
{
    /*! Pointer to output buffer - buffer should be 64 byte aligned.bit sequence*/
    int8_t * data_out; /*!< Pointer to data after compression. */
    int32_t len_out;  /*!< Length of output data in bytes. */
};

struct xranlib_5gnr_mod_decompression_request
{
    /*! Pointer to buffer of the input symbols - buffer must be 64 byte aligned. Format 16Sx. */
    int8_t * data_in;
    /*! 16bit shift value used to scale the input samples. */
    int16_t unit;
    /*! Supported modulation values are: 2 (QPSK), 4 (16QAM), 6 (64QAM), 8 (256QAM). */
    enum xran_modulation_order modulation;
    int32_t num_symbols;  /*!< Number of complex input symbols. */
    int16_t re_mask;  /*!< RE mask in one RB. */
};

/*!
    \struct bblib_llr_demapping_5gnr_mod_compression_response
    \brief Structure defining the modualtion compression output.
 */
struct xranlib_5gnr_mod_decompression_response
{
    /*! Pointer to output buffer - buffer should be 64 byte aligned.bit sequence*/
    int16_t * data_out; /*!< Pointer to data after compression. */
    int32_t len_out;  /*!< Length of output data in bytes. */
};

//! @{
/*! \brief modulation compression procedures.
*/
int xranlib_5gnr_mod_compression(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response);
int xranlib_5gnr_mod_compression_avx512(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response);
int xranlib_5gnr_mod_compression_snc(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response);
int xranlib_5gnr_mod_compression_c(const struct xranlib_5gnr_mod_compression_request* request,
        struct xranlib_5gnr_mod_compression_response* response);
int xranlib_5gnr_mod_decompression(const struct xranlib_5gnr_mod_decompression_request* request,
    struct xranlib_5gnr_mod_decompression_response* response);

