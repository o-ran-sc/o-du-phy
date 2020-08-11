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
 * @brief This file provides the definition of Control Plane Messages
 *      for XRAN Front Haul layer as defined in XRAN-FH.CUS.0-v02.01.
 *
 * @file xran_pkt_cp.h
 * @ingroup group_lte_source_xran
 * @author Intel Corporation
 *
 **/

#ifndef _XRAN_PKT_CP_H_
#define _XRAN_PKT_CP_H_

#ifdef __cplusplus
extern "C" {
#endif


/**********************************************************************
 * Common structures for C/U-plane
 **********************************************************************/
/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      user data compression header defined in 5.4.4.10 / 6.3.3.13
 */
struct xran_radioapp_udComp_header {
    uint8_t     udCompMeth:4;           /**< Compression method, XRAN_COMPMETHOD_xxxx */
    uint8_t     udIqWidth:4;            /**< IQ bit width, 1 ~ 16 */
    } __attribute__((__packed__));


/**********************************************************************
 * Definition of C-Plane Protocol 5.4
 **********************************************************************/
/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Common Radio Application Header for C-Plane
 */
struct xran_cp_radioapp_common_header {     /* 6bytes, first 4bytes need the conversion for byte order */
    uint32_t    startSymbolId:6;        /**< 5.4.4.7 start symbol identifier */
    uint32_t    slotId:6;               /**< 5.4.4.6 slot identifier */
    uint32_t    subframeId:4;           /**< 5.4.4.5 subframe identifier */
    uint32_t    frameId:8;              /**< 5.4.4.4 frame identifier */
    uint32_t    filterIndex:4;          /**< 5.4.4.3 filter index, XRAN_FILTERINDEX_xxxx */
    uint32_t    payloadVer:3;           /**< 5.4.4.2 payload version, should be 1 */
    uint32_t    dataDirection:1;        /**< 5.4.4.1 data direction (gNB Tx/Rx) */
    uint8_t     numOfSections;          /**< 5.4.4.8 number of sections */
    uint8_t     sectionType;            /**< 5.4.4.9 section type */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      frame structure defined in 5.4.4.13
 */
struct xran_cp_radioapp_frameStructure {
    uint8_t     uScs:4;                 /**< sub-carrier spacing, XRAN_SCS_xxx */
    uint8_t     fftSize:4;              /**< FFT size,  XRAN_FFTSIZE_xxx */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section headers definition for C-Plane.
 *      Section type 6 and 7 are not present since those have different fields.
 */
struct xran_cp_radioapp_section_header {    /* 8bytes, need the conversion for byte order */
    union {
        struct {
            uint32_t    reserved:16;
            uint32_t    numSymbol:4;    /**< 5.4.5.7 number of symbols */
            uint32_t    reMask:12;      /**< 5.4.5.5 resource element mask */
            } s0;
        struct {
            uint32_t     beamId:15;     /**< 5.4.5.9 beam identifier */
            uint32_t     ef:1;          /**< 5.4.5.8 extension flag */
            uint32_t     numSymbol:4;   /**< 5.4.5.7 number of symbols */
            uint32_t     reMask:12;     /**< 5.4.5.5 resource element mask */
            } s1;
        struct {
            uint32_t    beamId:15;      /**< 5.4.5.9 beam identifier */
            uint32_t    ef:1;           /**< 5.4.5.8 extension flag */
            uint32_t    numSymbol:4;    /**< 5.4.5.7 number of symbols */
            uint32_t    reMask:12;      /**< 5.4.5.5 resource element mask */
            } s3;
        struct {
            uint32_t    ueId:15;        /**< 5.4.5.10 UE identifier */
            uint32_t    ef:1;           /**< 5.4.5.8 extension flag */
            uint32_t    numSymbol:4;    /**< 5.4.5.7 number of symbols */
            uint32_t    reMask:12;      /**< 5.4.5.5 resource element mask */
            } s5;
        } u;

    uint32_t    numPrbc:8;              /**< 5.4.5.6 number of contiguous PRBs per control section  0000 0000b = all PRBs */
    uint32_t    startPrbc:10;           /**< 5.4.5.4 starting PRB of control section */
    uint32_t    symInc:1;               /**< 5.4.5.3 symbol number increment command XRAN_SYMBOLNUMBER_xxxx */
    uint32_t    rb:1;                   /**< 5.4.5.2 resource block indicator, XRAN_RBIND_xxx */
    uint32_t    sectionId:12;           /**< 5.4.5.1 section identifier */
    } __attribute__((__packed__));


struct xran_cp_radioapp_section_ext_hdr {
    /* 12 bytes, need to convert byte order for two parts respectively
     *  - 2 and 8 bytes, reserved1 would be OK if it is zero
     */
    uint16_t     extLen:8;          /**< 5.4.6.3 extension length, in 32bits words */
    uint16_t     extType:7;         /**< 5.4.6.1 extension type */
    uint16_t     ef:1;              /**< 5.4.6.2 extension flag */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Beamforming Weights Extension Type(ExtType 1) defined in 5.4.7.1
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext1 {
    /* variable length, need to be careful to convert byte order
     *   - does not need to convert first 3 bytes */
    uint8_t     extType:7;          /**< 5.4.6.1 extension type */
    uint8_t     ef:1;               /**< 5.4.6.2 extension flag */
    uint8_t     extLen;             /**< 5.4.6.3 extension length, in 32bits words */
    /* bfwCompHdr */
    uint8_t     bfwCompMeth:4;      /**< 5.4.7.1.1 Beamforming weight Compression method */
    uint8_t     bfwIqWidth:4;       /**< 5.4.7.1.1 Beamforming weight IQ bit width */

    /*
     *
     *
     * bfwCompParam
     * (bfwI,  bfwQ)+
     *    ......
     * zero padding for 4-byte alignment
     */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Beamforming Attributes Extension Type(ExtType 2) defined in 5.4.7.2
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext2 {
    /* variable length, need to be careful to convert byte order
     *   - first 4 bytes can be converted at once
     */
    uint32_t    bfZe3ddWidth:3;     /**< 5.4.7.2.1 beamforming zenith beamwidth parameter bitwidth, Table 5-21 */
    uint32_t    bfAz3ddWidth:3;     /**< 5.4.7.2.1 beamforming azimuth beamwidth parameter bitwidth, Table 5-20 */
    uint32_t    bfaCompResv1:2;
    uint32_t    bfZePtWidth:3;      /**< 5.4.7.2.1 beamforming zenith pointing parameter bitwidth, Table 5-19 */
    uint32_t    bfAzPtWidth:3;      /**< 5.4.7.2.1 beamforming azimuth pointing parameter bitwidth, Table 5-18 */
    uint32_t    bfaCompResv0:2;
    uint32_t    extLen:8;           /**< 5.4.6.3 extension length, in 32bits words */
    uint32_t    extType:7;          /**< 5.4.6.1 extension type */
    uint32_t    ef:1;               /**< 5.4.6.2 extension flag */

    /*
     * would be better to use bit manipulation directly to add these parameters
     *
     * bfAzPt: var by bfAzPtWidth
     * bfZePt: var by bfZePtWidth
     * bfAz3dd: var by bfAz3ddWidth
     * bfZe3dd: var by bfZe3ddWidth
     * bfAzSI:5 (including zero-padding for unused bits)
     * bfZeSI:3
     * padding for 4-byte alignment
     *
     */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      DL Precoding Extension Type(ExtType 3) for first data layer.
 *      Defined in 5.4.7.3 Table 5-22.
 *      Only be used for LTE TM2-4 and not for other LTE TMs nor NR.
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext3_first {
    /* 16 bytes, need to convert byte order for two parts - 8/8 bytes */
    uint64_t    reserved1:8;
    uint64_t    crsSymNum:4;        /**< 5.4.7.3.6 CRS symbol number indication */
    uint64_t    reserved0:3;
    uint64_t    crsShift:1;         /**< 5.4.7.3.7 CRS shift used for DL transmission */
    uint64_t    crsReMask:12;       /**< 5.4.7.3.5 CRS resource element mask */
    uint64_t    txScheme:4;         /**< 5.4.7.3.3 transmission scheme */
    uint64_t    numLayers:4;        /**< 5.4.7.3.4 number of layers used for DL transmission */
    uint64_t    layerId:4;          /**< 5.4.7.3.2 Layer ID for DL transmission */
    uint64_t    codebookIndex:8;    /**< 5.4.7.3.1 precoder codebook used for transmission */
    uint64_t    extLen:8;           /**< 5.4.6.3 extension length, in 32bits words */
    uint64_t    extType:7;          /**< 5.4.6.1 extension type */
    uint64_t    ef:1;               /**< 5.4.6.2 extension flag */

    uint64_t    beamIdAP1:16;       /**< 5.4.7.3.8 beam id to be used for antenna port 1 */
    uint64_t    beamIdAP2:16;       /**< 5.4.7.3.9 beam id to be used for antenna port 2 */
    uint64_t    beamIdAP3:16;       /**< 5.4.7.3.10 beam id to be used for antenna port 3 */
    uint64_t    reserved2:16;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      DL Precoding Extension Type(ExtType 3) for non-first data layer.
 *      Defined in 5.4.7.3 Table 5-23.
 *      Only be used for LTE TM2-4 and not for other LTE TMs nor NR.
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext3_non_first {
    /* 4 bytes, need to convert byte order at once */
    uint32_t    numLayers:4;        /**< 5.4.7.3.4 number of layers used for DL transmission */
    uint32_t    layerId:4;          /**< 5.4.7.3.2 Layer ID for DL transmission */
    uint32_t    codebookIndex:8;    /**< 5.4.7.3.1 precoder codebook used for transmission */

    uint32_t    extLen:8;           /**< 5.4.6.3 extension length, in 32bits words */
    uint32_t    extType:7;          /**< 5.4.6.1 extension type */
    uint32_t    ef:1;               /**< 5.4.6.2 extension flag */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Modulation Compression Parameter Extension Type(ExtType 4), 5.4.7.4
 *      Only applies to section type 1 and 3.
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext4 {
    /* 4 bytes, need to convert byte order at once */
    uint32_t    modCompScaler:15;   /**< 5.4.7.4.2 modulation compression scaler value */
    uint32_t    csf:1;              /**< 5.4.7.4.1 constellation shift flag */

    uint32_t    extLen:8;           /**< 5.4.6.3 extension length, in 32bits words */
    uint32_t    extType:7;          /**< 5.4.6.1 extension type */
    uint32_t    ef:1;               /**< 5.4.6.2 extension flag */
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Modulation Compression Additional Parameter Extension Type(ExtType 5) for one scaler value.
 *      Defined in 5.4.7.5 Table 5-26 and Table 5-27.
 *      Only applies to section type 1 3, and 5.
 *      The structure is reordered for byte order conversion.
 */
struct xran_cp_radioapp_section_ext5 {
    uint32_t    reserved0:8;
    uint32_t    mcScaleOffset2:15;  /**< 5.4.7.5.3 scaling value for modulation compression */
    uint32_t    csf2:1;             /**< 5.4.7.5.2 constellation shift flag */
    uint32_t    mcScaleReMask2:12;  /**< 5.4.7.5.1 modulation compression power scale RE mask */
    uint32_t    mcScaleOffset1:15;  /**< 5.4.7.5.3 scaling value for modulation compression */
    uint32_t    csf1:1;             /**< 5.4.7.5.2 constellation shift flag */
    uint32_t    mcScaleReMask1:12;  /**< 5.4.7.5.1 modulation compression power scale RE mask */
    } __attribute__((__packed__));


/**********************************************************
 * Scheduling and Beam-forming Commands 5.4.2
 **********************************************************/
/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 0
 */
struct xran_cp_radioapp_section0_header {   // 12bytes (6+2+1+2+1)
    struct xran_cp_radioapp_common_header cmnhdr;
    uint16_t    timeOffset;             /**< 5.4.4.12 time offset */

    struct xran_cp_radioapp_frameStructure  frameStructure;
    uint16_t    cpLength;               /**< 5.4.4.14 cyclic prefix length */
    uint8_t     reserved;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section definition for type 0: Unused RB or Symbols in DL or UL (Table 5-2)
 *      Not supported in this release
 */
struct xran_cp_radioapp_section0 {          // 8bytes (4+4)
    struct xran_cp_radioapp_section_header hdr;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 1
 */
struct xran_cp_radioapp_section1_header {   // 8bytes (6+1+1)
    struct xran_cp_radioapp_common_header cmnhdr;
    struct xran_radioapp_udComp_header udComp;
    uint8_t     reserved;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section definition for type 1: Most DL/UL Radio Channels (Table 5-3)
 */
struct xran_cp_radioapp_section1 {          // 8bytes (4+4)
    struct xran_cp_radioapp_section_header hdr;

    // section extensions               // 5.4.6 & 5.4.7
    //  .........
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 3
 */
struct xran_cp_radioapp_section3_header {   // 12bytes (6+2+1+2+1)
    struct xran_cp_radioapp_common_header cmnhdr;
    uint16_t    timeOffset;             /**< 5.4.4.12 time offset */

    struct xran_cp_radioapp_frameStructure  frameStructure;
    uint16_t    cpLength;               /**< 5.4.4.14 cyclic prefix length */
    struct xran_radioapp_udComp_header udComp;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section definition for type 3: PRACH and Mixed-numerology Channels (Table 5-4)
 */
struct xran_cp_radioapp_section3 {          // 12bytes (4+4+4)
    struct xran_cp_radioapp_section_header hdr;
    uint32_t    freqOffset:24;          /**< 5.4.5.11 frequency offset */
    uint32_t    reserved:8;

    // section extensions               // 5.4.6 & 5.4.7
    //  .........
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 5
 */
struct xran_cp_radioapp_section5_header {   // 8bytes (6+1+1)
    struct xran_cp_radioapp_common_header cmnhdr;
    struct xran_radioapp_udComp_header udComp;
    uint8_t     reserved;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section definition for type 5: UE scheduling information (Table 5-5)
 *      Not supported in this release
 */
struct xran_cp_radioapp_section5 {
    struct xran_cp_radioapp_section_header hdr;

    // section extensions               // 5.4.6 & 5.4.7
    //  .........
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 6
 */
struct xran_cp_radioapp_section6_header {   // 8bytes (6+1+1)
    struct xran_cp_radioapp_common_header cmnhdr;
    uint8_t     numberOfUEs;            /**< 5.4.4.11 number of UEs */
    uint8_t     reserved;
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section definition for type 6: Channel Information (Table 5-6)
 *      Not supported in this release
 */
struct xran_cp_radioapp_section6 {
    uint32_t    regularizationFactor:16;/**< 5.4.5.12 regularization Factor */
    uint32_t    ueId:15;                /**< 5.4.5.10 UE identifier */
    uint32_t    ef:1;                   /**< 5.4.5.8 extension flag */
    uint8_t     startPrbch:2;           /**< 5.4.5.4 starting PRB of control section */
    uint8_t     symInc:1;               /**< 5.4.5.3 symbol number increment command XRAN_SYMBOLNUMBER_xxxx */
    uint8_t     rb:1;                   /**< 5.4.5.2 resource block indicator, XRAN_RBIND_xxx */
    uint8_t     reserved:4;
    uint8_t     startPrbcl:8;           /**< 5.4.5.4 starting PRB of control section */
    uint8_t     numPrbc:8;              /**< 5.4.5.6 number of contiguous PRBs per control section */

    // ciIQsamples start from here      // 5.4.5.13 channel information I and Q values
    //  .........
    //
    // section extensions               // 5.4.6 & 5.4.7
    //  .........
    } __attribute__((__packed__));

/**
 * @ingroup xran_cp_pkt
 *
 * @description
 *      Section header definition for type 7: LAA
 *      Not supported in this release
 */
struct xran_cp_radioapp_section7_header {
    struct xran_cp_radioapp_common_header cmnhdr;
    uint16_t    reserved;
    uint8_t     laaMsgLen:4;            /**< 5.4.5.15 LAA message length */
    uint8_t     laaMsgType:4;           /**< 5.4.5.14 LAA message type */

    // Payload start from here          // 5.4.5.16 ~ 5.4.5.32
    } __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif  /* _XRAN_PKT_CP_H_ */
