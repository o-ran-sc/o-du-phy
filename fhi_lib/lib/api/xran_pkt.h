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
 * @brief Definitions and support functions to process XRAN packet
 * @file xran_pkt.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

/* XRAN-FH.CUS.0-v02.01.03 xRAN Front haul Working Group Control, User and Synchronization Plane Specification */

/*
 * Layer common to data and control packets
 */

#ifndef _XRAN_PKT_H_
#define _XRAN_PKT_H_

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_byteorder.h>


/**
 *****************************************************************************
 * @file xran_pkt.h
 *
 * @defgroup xran_common_pkt XRAN Packet definitions and functions
 * @ingroup xran
 *
 * @description
 *      Definitions and support functions to process XRAN packet
 *****************************************************************************/

#define ECPRI_MAX_PAYLOAD_SIZE 65535 /**< Max packet size taken in this implementation */

/* XRAN spec: For this encapsulation, either the eCPRI Ethertype or the IEEE 1914.3 Ethertype shall be use */
#define XRAN_ETHER_TYPE 0xAEFE /**< defined by eCPRI Specification V1.1 */

#define XRAN_ECPRI_VER      0x0001      /**< eCPRI protocol revision 3.1.3.1.1 */
#define XRAN_PAYLOAD_VER    0x0001      /**< Payload version 5.4.4.2 */

#define VLAN_ID  0 /**< Default Tag protocol identifier (TPID)*/
#define VLAN_PCP 7 /**< U-Plane and C-Plane only see Table 3 5 : Quality of service classes */

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       eCPRI message types
 *       as per eCPRI spec 3.2.4. Message Types
 *****************************************************************************/
enum ecpri_msg_type
{
     ECPRI_IQ_DATA           = 0x00, /**< U-plane: IQ data */
     ECPRI_BIT_SEQUENCE      = 0x01, /* msg type is not supported */
     ECPRI_RT_CONTROL_DATA   = 0x02, /**< C-plane: Control */

     /* Below msg types are not supported */
     ECPRI_GEN_DATA_TRANSFER = 0x03,
     ECPRI_REMOTE_MEM_ACCESS = 0x04,
     ECPRI_DELAY_MEASUREMENT = 0x05,
     ECPRI_REMOTE_RESET      = 0x06,
     ECPRI_EVENT_INDICATION  = 0x07,
     ECPRI_MSG_TYPE_MAX
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       see 3.1.3.1.7 ecpriSeqid (message identifier)
 *****************************************************************************/
struct ecpri_seq_id
{
    uint8_t seq_id:8; /**< Sequence ID */
    uint8_t sub_seq_id:7; /**< Subsequence ID */
    uint8_t e_bit:1;  /**< E bit */
} __rte_packed;


/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds common eCPRI header as per
 *       Table 3 1 : eCPRI Transport Header Field Definitions
 *****************************************************************************/
struct xran_ecpri_hdr
{
    uint8_t ecpri_concat:1; /**< This parameter indicates when eCPRI concatenation is in use
                            (allowing multiple eCPRI messages in a single Ethernet payload).
                            NOTE: This parameter is part of the eCPRI common header. */
    uint8_t ecpri_resv:3; /**< This parameter is reserved for eCPRI future use.
                               NOTE: This parameter is part of the eCPRI common header. */
    uint8_t ecpri_ver:4; /**< This parameter indicates the eCPRI protocol version.
                              NOTE: This parameter is part of the eCPRI common header. */
    uint8_t ecpri_mesg_type:8; /**< This parameter indicates the type of service conveyed by
                               the message type. NOTE: This parameter is part of the eCPRI
                               common header. NOTE: In this version of the specification,
                               only values "0000 0000b" and "0000 0010b" and "0000 0101b" are used. */
    rte_be16_t ecpri_payl_size:16; /**< This parameter is the size in bytes of the payload part
                                    of the corresponding eCPRI message. It does not include any padding bytes
                                    following the eCPRI message. The maximum supported payload size is 216-1,
                                    but the actual size may be further limited by the maximum payload size of
                                    the underlying transport network. NOTE: This parameter is part of the
                                    eCPRI common header. */

    rte_be16_t ecpri_xtc_id;  /**< 3.1.3.1.6 This parameter is a component_eAxC identifier (c_eAxC ID) and
                            identifies the specific data flow associated with each C-Plane (ecpriRtcid) or
                            U-Plane (ecpriPcid) message.  It is the analog of CPRI's "AxC" (antenna-carrier)
                            value so is designated here as "eAxC" ("e" for "extended" to accommodate multiple
                            bands and multiple component carriers).  In addition, the "eAxC" is divided into
                            "component eAxC" parts (c_eAxC) because multiple lls-CU processors may contribute
                            to a single eAxC and must be identified for correct data routing. */

    struct ecpri_seq_id ecpri_seq_id; /**< This parameter provides unique message identification and ordering on
        two different levels. The first octet of this parameter is the Sequence ID, which is used to identify ordering of
        messages within an eAxC message stream.  The Sequence ID field increments and wraps independently for each U-Plane
        eAxC DL, U-Plane eAxC UL, C-Plane eAxC DL, and C-Plane eAxC UL, even if they share the same eAxC ID.
        The Sequence ID is used to verify that all messages are received and also to reorder messages that are received out of order.
        The second octet of this parameter is the Subsequence ID. The Subsequence ID is used to verify ordering and implement
        reordering when radio-transport-level (eCPRI or IEEE-1914.3) fragmentation occurs.
        Radio-transport (eCPRI or IEEE-1914.3) fragmentation is a method of splitting U-plane messages containing one or
        more sections whose length exceeds the maximum packet or message length of the underlying protocol.
        The Subsequence ID field consists of a 7 bit Subsequence counter and a single bit field, called E-bit.
        The Subsequence number increments starting from zero for each fragment of a U-plane message.  The E bit
        is used to indicate the last message of the radio-transport level fragments.  It is always set to zero
        except for the last message of the U-plane fragment. In the case of C-plane messages radio-transport
        fragmentation is not allowed, therefore the Subsequence ID shall be set to zero, and the E bit set to one.
        See Section 3.1.4 for a description of the fragmentation process.
        NOTE: As an alternative to radio-transport-level fragmentation, application fragmentation can be implemented.
        In this case the application can take the responsibility to ensure all transport messages are not too long
        (fit within the necessary transport payload size).  When this "application layer fragmentation" is used,
        the subsequence identifier shall always be set to "0", and the E-bit set to "1" (See Section 3.1.4). */

} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds complete xran packet header
 *       3.1.1	Ethernet Encapsulation
 *****************************************************************************/
struct xran_pkt_hdr
{
    struct ether_hdr eth_hdr; /**< Ethernet Header */
    struct vlan_hdr vlan_hdr; /**< VLAN Header */
    struct xran_ecpri_hdr ecpri_hdr; /**< eCPRI Transport Header */
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *      Enum used to set xRAN packet data direction (gNB Tx/Rx 5.4.4.1)
 *      uplink or downlink
 *****************************************************************************/
enum xran_pkt_dir
{
     XRAN_DIR_UL  = 0, /**< UL direction */
     XRAN_DIR_DL  = 1, /**< DL direction */
     XRAN_DIR_MAX
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds components of radio application header
 *       5.4.4 Coding of Information Elements - Application Layer, Common
 *       for U-plane as per 6.3.2	DL/UL Data
 *****************************************************************************/
struct radio_app_common_hdr
{
   /* Octet 9 */
   uint8_t filter_id:4; /**< This parameter defines an index to the channel filter to be
                              used between IQ data and air interface, both in DL and UL.
                              For most physical channels filterIndex =0000b is used which
                              indexes the standard channel filter, e.g. 100MHz channel filter
                              for 100MHz nominal carrier bandwidth. (see 5.4.4.3 for more) */
   uint8_t payl_ver:3; /**< This parameter defines the payload protocol version valid
                            for the following IEs in the application layer. In this version of
                            the specification payloadVersion=001b shall be used. */
   uint8_t data_direction:1; /**< This parameter indicates the gNB data direction. */

   /* Octet 10 */
   uint8_t frame_id:8;    /**< This parameter is a counter for 10 ms frames (wrapping period 2.56 seconds) */

   /* Octet 11 */
   /* Octet 12 */
   union {
       uint16_t value;
       struct {
           uint16_t symb_id:6; /**< This parameter identifies the first symbol number within slot,
                                          to which the information of this message is applies. */
           uint16_t slot_id:6; /**< This parameter is the slot number within a 1ms sub-frame. All slots in
                                   one sub-frame are counted by this parameter, slotId running from 0 to Nslot-1.
                                   In this version of the specification the maximum Nslot=16, All
                                   other values of the 6 bits are reserved for future use. */
           uint16_t subframe_id:4; /**< This parameter is a counter for 1 ms sub-frames within 10ms frame. */
       };
   }sf_slot_sym;

} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *      This parameter defines the compression method and IQ bit width for the
 *      user data in the data section.  This field is absent from U-Plane messages
 *      when the static IQ format and compression method is configured via the M-Plane.
 *      In this way a single compression method and IQ bit width is provided
 *     (per UL and DL, per LTE and NR) without adding more overhead to U-Plane messages.
 *****************************************************************************/
struct compression_hdr
{
    uint8_t ud_iq_width:4; /**< Bit width of each I and each Q
                                16 for udIqWidth=0, otherwise equals udIqWidth e.g. udIqWidth = 0000b means I and Q are each 16 bits wide;
                                e.g. udIQWidth = 0001b means I and Q are each 1 bit wide;
                                e.g. udIqWidth = 1111b means I and Q are each 15 bits wide
                                */
    uint8_t ud_comp_meth:4;
    /**< udCompMeth|  compression method         |udIqWidth meaning
    ---------------+-----------------------------+--------------------------------------------
    0000b          | no compression              |bitwidth of each uncompressed I and Q value
    0001b          | block floating point        |bitwidth of each I and Q mantissa value
    0010b          | block scaling               |bitwidth of each I and Q scaled value
    0011b          | mu-law                      |bitwidth of each compressed I and Q value
    0100b          | modulation compression      |bitwidth of each compressed I and Q value
    0100b - 1111b  | reserved for future methods |depends on the specific compression method
    */
} __rte_packed;

#endif
