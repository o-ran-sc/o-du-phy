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
 * @brief XRAN layer one-way delay measurement support
 * @file xran_delay_measurement.c
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/
#define _GNU_SOURCE
#include <immintrin.h>
#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include "xran_common.h"
#include "ethdi.h"
#include "xran_pkt.h"
#include "xran_dev.h"
#include "xran_lib_mlog_tasks_id.h"
#include "xran_ecpri_owd_measurements.h"

#include "xran_printf.h"
#include "xran_mlog_lnx.h"

//#define ORAN_OWD_DEBUG_MSG_FLOW
//#define XRAN_OWD_DEBUG_MSG_FLOW
//#define XRAN_OWD_DEBUG_DELAY_INFO
//#define XRAN_OWD_DEBUG_TIME_STAMPS_INFO
//#define XRAN_OWD_DEBUG_MEAS_DB
//#define XRAN_OWD_TIMING_MODS


    // Support for 1-way eCPRI delay measurement per section 3.2.4.6 of eCPRI Specification V2.0

uint64_t xran_ptp_to_host(uint64_t compValue)
{
    return (rte_be_to_cpu_64(compValue));
}
void xran_host_to_ptp_ts(TimeStamp *ts, struct timespec *t)
{
    uint64_t seconds, nanoseconds;

    seconds = t->tv_sec;
    nanoseconds = t->tv_nsec%1000000000LL;
#ifdef XRAN_OWD_DEBUG_DELAY_CONV_FUNCTIONS
    printf("H2P_ts tv_sec %8"PRIx64" tv_nsec %8"PRIx64" seconds %8"PRIx64" ns %8"PRIx64" \n",t->tv_sec,t->tv_nsec,seconds,nanoseconds);
#endif

    ts->secs_msb = rte_cpu_to_be_16((rte_be16_t)((seconds >> 32) & 0xFFFF));
    ts->secs_lsb = rte_cpu_to_be_32((rte_be32_t)(seconds & 0xFFFFFFFF));
    ts->ns       = rte_cpu_to_be_32((rte_be32_t)nanoseconds);
#ifdef XRAN_OWD_DEBUG_DELAY_CONV_FUNCTIONS
    printf("Net order s_msb %4"PRIx16" s_lsb %8"PRIx32" ns %8"PRIx32" \n", ts->secs_msb, ts->secs_lsb,ts->ns );
#endif
}

uint64_t xran_ptp_ts_to_ns(TimeStamp *t)
{
        uint64_t seconds, nanoseconds;
        uint64_t ret_value;
        // Convert to host order
        t->secs_msb=rte_be_to_cpu_16(t->secs_msb);
        t->secs_lsb=rte_be_to_cpu_32(t->secs_lsb);
        seconds = ((uint64_t)t->secs_msb << 32) | ((uint64_t)t->secs_lsb );
        nanoseconds = rte_be_to_cpu_32((uint64_t)t->ns);
        ret_value = seconds * NS_PER_SEC + nanoseconds;
#ifdef XRAN_OWD_DEBUG_DELAY_CONV_FUNCTIONS
        printf("PTP ts to ns sec_msb %4"PRIx16" secs_lsb %4"PRIx32" ns %4"PRIx32"  seconds %8"PRIx64" nanosec %8"PRIx64" ret_value %8"PRIx64"\n",t->secs_msb,t->secs_lsb,t->ns,seconds, nanoseconds,ret_value);
#endif
        return ret_value;

}
static inline uint64_t xran_timespec_to_ns(struct timespec *t)
{
    uint64_t ret_val;

    ret_val  = t->tv_sec * NS_PER_SEC + t->tv_nsec;
#ifdef XRAN_OWD_DEBUG_DELAY_CONV_FUNCTIONS
    printf("t->tv_sec is %08"PRIx64" tv_nsec is %08"PRIx64" ret_val is %08"PRIx64" ts_to_ns\n",t->tv_sec,t->tv_nsec,ret_val);
#endif
    return ret_val;

}

void xran_ns_to_timespec(uint64_t ns, struct timespec *t)
{
    t->tv_sec = ns/NS_PER_SEC;
    t->tv_nsec = ns % NS_PER_SEC;

}

void xran_initialize_and_verify_owd_pl_length(void* handle)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    
    if ((p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_PlLength == 0)||(p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_PlLength < MIN_OWDM_PL_LENGTH))
    {
         // Use default length value
         p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_PlLength = MIN_OWDM_PL_LENGTH;
    }
    else if ( p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_PlLength > MAX_OWDM_PL_LENGTH)
    {
         p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id].owdm_PlLength  = MAX_OWDM_PL_LENGTH;
    }
    
}

void xran_adjust_timing_parameters(void* Handle)
{
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)Handle;
#ifdef XRAN_OWD_TIMING_MODS
    printf("delayAvg is %d and DELAY_THRESHOLD is %d \n", p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][0].delayAvg, DELAY_THRESHOLD);
#endif
    if (p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][0].delayAvg < DELAY_THRESHOLD )
        {
            /* Modify the timing parameters */
            if (p_xran_dev_ctx->fh_cfg.T1a_max_up >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.T1a_max_up -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.T2a_max_up >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.T2a_max_up -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.Ta3_min >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.Ta3_min -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.T1a_max_cp_dl >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.T1a_max_cp_dl -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.T1a_min_up >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.T1a_min_up -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.T1a_max_up >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.T1a_max_up -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.Ta4_min >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.Ta4_min -= ADJUSTMENT;
            if (p_xran_dev_ctx->fh_cfg.Ta4_max >= ADJUSTMENT)
                p_xran_dev_ctx->fh_cfg.Ta4_max -= ADJUSTMENT;
#ifdef XRAN_OWD_TIMING_MODS
            printf("Mod T1a_max_up is %d\n",p_xran_dev_ctx->fh_cfg.T1a_max_up);
            printf("Mod T2a_max_up is %d\n",p_xran_dev_ctx->fh_cfg.T2a_max_up);
            printf("Mod Ta3_min is %d\n",p_xran_dev_ctx->fh_cfg.Ta3_min);
            printf("Mod T1a_max_cp_dl is %d\n",p_xran_dev_ctx->fh_cfg.T1a_max_cp_dl);
            printf("Mod T1a_min_up is %d\n",p_xran_dev_ctx->fh_cfg.T1a_min_up);
            printf("Mod T1a_max_up is %d\n",p_xran_dev_ctx->fh_cfg.T1a_max_up);
            printf("Mod Ta4_min is %d\n",p_xran_dev_ctx->fh_cfg.Ta4_min);
            printf("Mod Ta4_max is %d\n",p_xran_dev_ctx->fh_cfg.Ta4_max);
#endif
        }
   
}



void xran_compute_and_report_delay_estimate (struct xran_ecpri_del_meas_port *portData, uint16_t totalSamples, uint16_t id )
{
    uint16_t i;
    uint64_t *samples= portData->delaySamples;


    for (i=2 ; i < MX_NUM_SAMPLES; i++) //Ignore first 2 samples
    {
        portData->delayAvg += samples[i];

    }

    // Average the delay by the number of samples
    if ((totalSamples != 0)&&(totalSamples > 2))
    {
        portData->delayAvg /= (totalSamples-2);
    }
    // Report Average with printf
    flockfile(stdout);
    printf("OWD for port %i is %lu [ns] id %d \n", portData->portid, portData->delayAvg, id);
    funlockfile(stdout);

}

int xran_get_delay_measurements_results (void* handle,  uint16_t port_id, uint8_t id, uint64_t* pdelay_avg)
{
    int ret_value = FAIL;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx*)handle;
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[id][port_id];
    // Check is the one way delay measurement completed successfully
    if (powdp->msState == XRAN_OWDM_DONE)
    {
        *pdelay_avg = powdp->delayAvg;
        ret_value = OK;
    }
    return (ret_value);
}


void xran_build_owd_meas_ecpri_hdr(char* mbuf,    struct xran_ecpri_del_meas_cmn* eowdcmn)
{
    union xran_ecpri_cmn_hdr *tmp= (union xran_ecpri_cmn_hdr*)mbuf;
    /* Fill common header */
    tmp->bits.ecpri_ver           = XRAN_ECPRI_VER;
    tmp->bits.ecpri_resv          = 0;     // should be zero
    tmp->bits.ecpri_concat        = 0;
    tmp->bits.ecpri_mesg_type     = ECPRI_DELAY_MEASUREMENT;
    tmp->bits.ecpri_payl_size	 = 10 + eowdcmn->owdm_PlLength;
    tmp->bits.ecpri_payl_size    = rte_cpu_to_be_16(tmp->bits.ecpri_payl_size);
}

void xran_add_at_and_measId_info_to_header(void* pbuf, uint8_t actionType, uint8_t MeasurementID)
{
    struct xran_ecpri_delay_meas_pl* tmp = (struct xran_ecpri_delay_meas_pl*)pbuf;
    // Fill ActionType and MeasurementId
    tmp->ActionType = actionType;
    tmp->MeasurementID = MeasurementID;
}

void xran_initialize_ecpri_del_meas_port(struct xran_ecpri_del_meas_cmn* pCmn, struct xran_ecpri_del_meas_port* pPort, uint16_t full)
{

    uint16_t i=0;
    // Initialize port parameters during the first pass
    pPort->currentMeasID++;
    pPort->runMeas = 1;
    pPort->txDone = 0;

    if (full)
    {
        pPort->numMeas = 0;
        pPort->portid = pCmn->measVf;
        pPort->delayAvg = 0;
        pPort->delta = 0;
        pPort->t1 = 0;
        pPort->t2 = 0;
        pPort->tr = 0;
#ifdef XRAN_OWD_DEBUG_MEAS_DB
        printf("Clearing t1 and delta\n");
#endif

        for (i=0; i < MX_NUM_SAMPLES; i++)
        {
            pPort->delaySamples[i] = 0;
        }
    }
    // Set msState based on measMethod and whether the FHI is initiator or recipient

    if (pCmn->initiator_en)
    {
        switch (pCmn->measMethod)
        {
            case XRAN_REQUEST:
                pPort->msState = XRAN_OWDM_WAITRESP;
                break;
            case XRAN_REM_REQ:
                pPort->msState = XRAN_OWDM_WAITREQ;
                break;
            case XRAN_REQ_WFUP:
                pPort->msState = XRAN_OWDM_WAITRESP;
                break;
            case XRAN_REM_REQ_WFUP:
                pPort->msState = XRAN_OWDM_WAITREQWFUP;
                break;
            default:
                pPort->msState = XRAN_OWDM_WAITRESP;
                break;
        }
    }
    else
    {
        switch (pCmn->measMethod)
        {
            case XRAN_REQUEST:
                pPort->msState = XRAN_OWDM_WAITREQ;
                break;
            case XRAN_REM_REQ:
                pPort->msState = XRAN_OWDM_WAITREMREQ;
                break;
            case XRAN_REQ_WFUP:
                pPort->msState = XRAN_OWDM_WAITREQWFUP;
                break;
            case XRAN_REM_REQ_WFUP:
                pPort->msState = XRAN_OWDM_WAITREMREQWFUP;
                break;
            default:
                pPort->msState = XRAN_OWDM_WAITREQ;
                break;
       }
    }
}

int32_t xran_ecpri_port_update_required (struct xran_io_cfg * cfg, uint16_t port_id)
{
    int32_t ret_value = 0;
    int32_t* port = &cfg->port[0];

    if (cfg != NULL)
    {

        struct xran_ecpri_del_meas_port* eowdp = &cfg->eowd_port[cfg->id][port_id];
        struct xran_ecpri_del_meas_cmn*  eowdc = &cfg->eowd_cmn[cfg->id];


        // Check if the current port has completed all the measurements to move to the next port
        if (eowdp->numMeas == eowdc->numberOfSamples)
        {
            // Mark state as done and move to the next port
            if (port_id < cfg->num_vfs)
            {
                port_id++;
                if (port[port_id] == 0xFF)
                {
                    // Done with all ports disable further execution
                    eowdc->owdm_enable = 0;
                }
                else
                {
                    eowdc->measVf++;
                    eowdp= &cfg->eowd_port[cfg->id][port_id];
                    // Initialize the next port
#ifdef XRAN_OWD_DEBUG_MEAS_DB
                    printf("Init call_1 port %d\n", port_id);
#endif
                    xran_initialize_ecpri_del_meas_port(eowdc, eowdp,1);
                }
                ret_value = 1;  // Wait for the next pass through the loop to go to the next port
            }
            else
            {
                // Disable the measurements
                eowdc->owdm_enable = 0;
                ret_value = 1;
            }
        }
        else
        {
            // Continue running on the same port
            ret_value = 0;
//              xran_initialize_ecpri_del_meas_port(eowdc, eowdp,0); //Now this logic is driven by the receiver
        }
    }
    else
    {
        errx(1, "Exit 1 epur with cfg null");
    }
    return ret_value;
}


/**
 * @brief ecpri 2.0 one-way delay measurement transmitter control
 *
 * @ingroup group_source_xran
 *
 * @param port_id
 *  port_id to be used
 * @param handle
 *  Pointer to an xran_device_ctx (cast)
 *
 * @return
 *  OK on success
 *  FAIL if failed to process the packet

 */
int xran_ecpri_one_way_delay_measurement_transmitter(uint16_t port_id, void* handle)
{
    // The ecpri one way delay measurement transmitter handles the transmission
    // of the owd measurement packets on each of the vfs present in the system in a sequential order
    // so the owd_meas_method is provided from the configuration file and it can be one of 4 possible
    // methods: REQUEST, REM_REQ, REQ_WFUP or REM_REQ_WFUP
    // In the current implementation the measurement is performed on one vf until completion of the number
    // of measurements defined from the configuration file.
    // A variable in the xran_ecpri_del_meas_cmn keeps track of the current vf that is using the transmitter and
    // when the current vf completes all the measurements it moves to the next vf until all of the vfs complete
    // the measurements
    // In the current implementation the measurements start after the xran_if_current_state has reached the
    // XRAN_RUNNING state (i.e. after having executed the xran_start())
    // The measurements run only once for the current release.
    int ret_value = FAIL;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];

    if (powdc->measState == OWDMTX_INIT)
    {
        // Perform the initialization for the very first call to the transmitter for a given port
        powdc->measVf = port_id;
        powdc->measState = OWDMTX_ACTIVE;
        // Check whether PL length was passed in config file and if it is within bounds
        if ((powdc->owdm_PlLength == 0)|| ( powdc->owdm_PlLength < MIN_OWDM_PL_LENGTH ))
        {
            // Use default length value
            powdc->owdm_PlLength = MIN_OWDM_PL_LENGTH;
        }
        else if ( powdc->owdm_PlLength > MAX_OWDM_PL_LENGTH)
        {
            powdc->owdm_PlLength = MAX_OWDM_PL_LENGTH;
        }
#ifdef XRAN_OWD_DEBUG_MEAS_DB
        printf("Clear call 2 port_id %d\n", port_id);
#endif
        xran_initialize_ecpri_del_meas_port(powdc, powdp,1);
    }

    // Initiator State Machine , recipient state machine driven from process_delay_meas()
//    printf("owdm tx w state %d runMeas %d inen %d\n", powdp->msState,powdp->runMeas,powdc->initiator_en);

    if ((powdp->runMeas != 0 )&&(powdc->initiator_en != 0)) // Current port still running measurements
    {
        switch (powdp->msState)
        {
            case XRAN_OWDM_WAITRESP:
                // Check the measmethod to define the action
                if (powdc->measMethod == XRAN_REQUEST)
                {
                    if (!powdp->txDone)
                    {
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
                        printf("owdm ecpri tx req gen\n");
#endif
                        if (xran_generate_delay_meas(port_id, handle, (uint8_t)ECPRI_REQUEST, powdc->measId) == 0 )
                        {
                            errx(1, "Exit 1 owdm tx port_id %d measId %d", port_id, powdc->measId);
                        }
                        powdp->txDone =1;
                    }
                }
                else
                {
                    // The only else corresponds to XRAN_REQ_WFUP
                    if (!powdp->txDone)
                    {
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
                        printf("owdm ecpri tx req w fup gen\n");
#endif
                        if (xran_generate_delay_meas(port_id, handle, (uint8_t)ECPRI_REQUEST_W_FUP , powdc->measId) == 0 )
                        {
                            errx(1, "Exit 2 owdm tx port_id %d measId %d", port_id, powdc->measId );
                        }
                        powdp->txDone=0;            // Needs fup
                    }
                }
                break;
            case XRAN_OWDM_WAITREQ:
                if (!powdp->txDone)
                {
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
                    printf("owdm ecpri tx rem req gen\n");
#endif
                    if (xran_generate_delay_meas(port_id, handle, (uint8_t)ECPRI_REMOTE_REQ , powdc->measId) == 0 )
                    {
                        errx(1, "Exit 3 owdm tx port_id %d measId %d", port_id, powdc->measId );
                    }
                    powdp->txDone=1;
                }
                break;
            case XRAN_OWDM_WAITREQWFUP:
                if (!powdp->txDone)
                {
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
                    printf("owdm ecpri tx rem req w fup gen\n");
#endif
                    if (xran_generate_delay_meas(port_id, handle, (uint8_t)ECPRI_REMOTE_REQ_W_FUP , powdc->measId) == 0 )
                    {
                        errx(1, "Exit 4 owdm tx port_id %d measId %d", port_id, powdc->measId );
                    }
                    powdp->txDone=1;
                }
                break;
            case XRAN_OWDM_GENFUP:
                if (!powdp->txDone)
                {
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
                    printf("owdm ecpri follow up gen\n");
#endif
                    if (xran_generate_delay_meas(port_id, handle, (uint8_t)ECPRI_FOLLOW_UP , powdc->measId) == 0 )
                    {
                        errx(1, "Exit 4 owdm tx port_id %d measId %d", port_id, powdc->measId );
                    }
                    powdp->txDone=1;
                }
                break;
            case XRAN_OWDM_WAITFUP:
            case XRAN_OWDM_DONE:
            case XRAN_OWDM_IDLE:
                // Transmitter doesn't have to do anything in these states
                break;
            default:
                errx(1, "Exit 5 owdm tx port_id %d measId %d id %d state %d", port_id, powdc->measId, p_xran_dev_ctx->fh_init.io_cfg.id, powdp->msState );

        }
    }
    ret_value = OK;
    return ret_value;

}

/**
 * @brief Generate a Delay Measurement packet
 *  Transport layer fragmentation is not supported.
 *
 * @ingroup group_source_xran
 *
 * @param port_id
 *  port_id to be used
 * @param handle
 *  Pointer to an xran_device_ctx (cast)
 * @param actionType
 * actionType to be used in the owd measurement packet
 * @param MeasurementID
 * MeasurementID to be populated in the owd measurement packet
 * @return
 *  OK on success
 *  FAIL if failed to process the packet

 */
int xran_generate_delay_meas(uint16_t port_id, void* handle, uint8_t actionType, uint8_t MeasurementID )
{
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_delay_meas_pkt *ecpri_delmeas_pkt;
    int pkt_len;
    struct rte_mbuf *mbuf,*pkt;
    char* pChar;
    struct xran_ecpri_delay_meas_pl * pdm= NULL;
    uint64_t tcv1,tr2m,trm;
    struct timespec tr2, tr;
    struct xran_io_cfg* cfg = &p_xran_dev_ctx->fh_init.io_cfg;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    int32_t *port = &cfg->port[port_id];
    int ret_value = FAIL;
    struct rte_ether_addr addr;
    uint16_t ethertype = ETHER_TYPE_ECPRI;

//    printf("in xran_generate_delay_meas for action_type %d\n", actionType);

    pkt_len = sizeof(struct xran_ecpri_del_meas_pkt);
    // Allocate a buffer from the pool
    mbuf =xran_ethdi_mbuf_alloc();
    if (mbuf == NULL)
    {
        MLogPrint(NULL);
        errx(1,"exit 1 owdm gen");
    }
    pChar = rte_pktmbuf_append(mbuf, pkt_len);
    if (pChar == NULL)
    {
        MLogPrint(NULL);
        errx(1,"exit 2 owdm gen");
    }
    pChar = rte_pktmbuf_prepend(mbuf, sizeof(struct rte_ether_hdr));
    if (pChar == NULL)
    {
        MLogPrint(NULL);
        errx(1,"exit 3 owdm gen");
    }

    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();

    struct rte_ether_hdr *h = (struct rte_ether_hdr *)rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
    PANIC_ON(h == NULL, "mbuf prepend of ether_hdr failed");

    /* Fill in the ethernet header. */
    rte_eth_macaddr_get(port_id, &h->s_addr);          /* set source addr */

    if (p_xran_dev_ctx->fh_init.io_cfg.id)
    {
//        rte_ether_addr_copy( (struct rte_ether_addr *)p_xran_dev_ctx->fh_init.p_o_du_addr[port_id],&h->d_addr);
        h->d_addr = ctx->entities[port_id][ID_O_DU];   /* set dst addr */
    }
    else
    {
        h->d_addr = ctx->entities[port_id][ID_O_RU];   /* set dst addr */
//        rte_ether_addr_copy( (struct rte_ether_addr *)p_xran_dev_ctx->fh_init.p_o_ru_addr[port_id],&h->d_addr);
    }

    h->ether_type = rte_cpu_to_be_16(ethertype);       /* ethertype too */
    mbuf->port = ctx->io_cfg.port[port_id];


    // Prepare the ecpri header info
    // Advance pointer to the begining of the ecpri common header
    pChar = pChar + sizeof (struct rte_ether_hdr);
    xran_build_owd_meas_ecpri_hdr(pChar, powdc );
    // Advance pointer to the begining of the xran_ecpri_delay_meas_pl
    pChar  = pChar + sizeof (union xran_ecpri_cmn_hdr);
    xran_add_at_and_measId_info_to_header(pChar, actionType, MeasurementID);

    pdm  = (struct xran_ecpri_delay_meas_pl *)rte_pktmbuf_mtod_offset(mbuf, struct xran_ecpri_delay_meas_pl *, sizeof(struct rte_ether_hdr) + sizeof(union xran_ecpri_cmn_hdr));
    switch (actionType)
    {
        // For owd meas originator there are a subset of actionTypes used see ecpri 2.0 Figures 25 and 26 for the details
        case ECPRI_REQUEST:
            // Record t1, prepare Request Message and determine tcv1 and include both time stamps in the packet
            // 1) Record the current timestamp when the preparation of the message started i.e. t1
            if (clock_gettime(CLOCK_REALTIME, &tr ))     // t1
            {
                return ret_value;
            }
            trm = xran_timespec_to_ns(&tr);
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
            printf("trm at gen is %8"PRIx64" \n", trm);
#endif
            // 2) Prepare the delay measurement request packet
            pdm->ActionType = ECPRI_REQUEST;
            // 3) Record the current timestamp at the moment that the delay measurement packet is ready to be transmitted tr2 i.e.t1+tcv1 and write it
            //    to the Delay Measurement request packet PL field
            if (clock_gettime(CLOCK_REALTIME, &tr2 ))     // ts
            {
                return ret_value;
            }
            // 4) Convert host to ptp time stamp format for tr and write to the outgoing packet
            xran_host_to_ptp_ts(&pdm->ts, &tr);
            // 5) Convert from Timestamp tr2 to ns before computing the compensation value
            tr2m = xran_timespec_to_ns(&tr2);
            // 6) Compute tcv1 as tr2m-trm
            tcv1 = tr2m - trm;
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
            printf("tcv1 is %08"PRIx64"\n",tcv1);
#endif

            // 7) write tcv1 to the CompensationValue field of the delay measurement request packet
            pdm->CompensationValue = rte_cpu_to_be_64(tcv1);
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
            printf("compensation value after net order %8"PRIx64" \n", pdm->CompensationValue);
#endif
            // 8) Store t1 and tcv1 to be used later once we get the response message
            powdp->currentMeasID = pdm->MeasurementID;
            powdp->t1 = trm;
            powdp->delta = tcv1;
            powdp->msState =   XRAN_OWDM_WAITRESP;
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
            printf("At req gen t1 %8"PRIx64" and delta %8"PRIx64"  port %d  \n",powdp->t1,powdp->delta,port_id);
#endif
            break;

        case ECPRI_REMOTE_REQ:
            // Prepare and send Remote Request Message with zero timestamp and correction values
            tr.tv_sec = 0;
            tr.tv_nsec = 0;
            tcv1 = 0;
            // Convert host to ptp time stamp format for tr and write to the outgoing packet
            xran_host_to_ptp_ts(&pdm->ts, &tr);
            // write zero to the CompensationValue field of the delay measurement remote request packet
            pdm->CompensationValue = rte_cpu_to_be_64(tcv1);
            // 1) Prepare the delay measurement request packet
            pdm->ActionType = ECPRI_REMOTE_REQ;
            // 2) Store MeasurementID and msState to be checked once the Request Message is received
            powdp->currentMeasID = pdm->MeasurementID;
            powdp->msState =   XRAN_OWDM_WAITREQ;

            break;

        case ECPRI_REQUEST_W_FUP:
            // Record t1, prepare Request with follow up Message and determine tcv1, send zero timestamp and correction value  in the packet
            // 1) Record the current timestamp when the message preparation started i.e. t1
            if (clock_gettime(CLOCK_REALTIME, &tr ))     // t1
            {
                return ret_value;
            }
            trm = xran_timespec_to_ns(&tr);
            // 2) Prepare the delay measurement remote request with follow up packet
            pdm->ActionType = ECPRI_REQUEST_W_FUP;
            // 3) Record the current timestamp at the moment that the delay measurement packet is ready to be transmitted tr2 i.e.t1+tcv1
            if (clock_gettime(CLOCK_REALTIME, &tr2 ))     // ts
            {
                return ret_value;
            }
            // 4) Convert from Timestamp tr2 to ns before computing the compensation value
            tr2m = xran_timespec_to_ns(&tr2);
            // 5) Compute tcv1 as tr2m-trm
            tcv1 = tr2m - trm;
            // Prepare and send Remote Request Message with zero timestamp and correction values
            tr.tv_sec = 0;
            tr.tv_nsec = 0;
            powdp->delta = tcv1; // Save tcv1 while waiting for the Response
            tcv1 = 0;
            // Convert host to ptp time stamp format for tr and write to the outgoing packet
            xran_host_to_ptp_ts(&pdm->ts, &tr);
            // write zero to the CompensationValue field of the delay measurement remote request packet
            pdm->CompensationValue = rte_cpu_to_be_64(tcv1);
            // 6) Store MeasurementID and msState to be checked once the Request Message is received
            powdp->currentMeasID = pdm->MeasurementID;
            powdp->t1 = trm;
            powdp->msState =   XRAN_OWDM_GENFUP;

            break;

        case ECPRI_FOLLOW_UP:
            // Use the t1 and tcv1 values recorded in the ECPRI_REQUEST_W_FUP packet generation step and send these values in the follow up packet
            // 1) Prepare the delay measurement follow up packet
            pdm->ActionType = ECPRI_FOLLOW_UP;
            // 2) Convert t1 from host to ptp format
            xran_ns_to_timespec(powdp->t1, &tr);
            // 3) Convert host to ptp time stamp format for tr and write to the outgoing packet
            xran_host_to_ptp_ts(&pdm->ts, &tr);
            // 4) write tcv1 to the CompensationValue field of the delay measurement request packet
            pdm->CompensationValue = rte_cpu_to_be_64(powdp->delta);
            powdp->currentMeasID = pdm->MeasurementID;
            powdp->msState =   XRAN_OWDM_WAITRESP;
            break;

        case ECPRI_REMOTE_REQ_W_FUP:
            // Prepare the Remote Request with follow up Message, send zero timestamp and correction value  in the packet
            tr.tv_sec = 0;
            tr.tv_nsec = 0;
            tcv1 = 0;
            // Convert host to ptp time stamp format for tr and write to the outgoing packet
            xran_host_to_ptp_ts(&pdm->ts, &tr);
            // write zero to the CompensationValue field of the delay measurement remote request packet
            pdm->CompensationValue = rte_cpu_to_be_64(tcv1);
            // 1) Prepare the delay measurement request packet
            pdm->ActionType = ECPRI_REMOTE_REQ_W_FUP;
            // 2) Store MeasurementID and msState to be checked once the Request Message is received
            powdp->currentMeasID = pdm->MeasurementID;
            powdp->msState =   XRAN_OWDM_WAITREQWFUP;

            break;

        default:
            errx(1,"exit 4 owdm gen");
            break;
    }

 //   printf("xran_gen_del_4n");

    // Retrieve Ethernet Header for the port and copy to the packet
    rte_eth_macaddr_get(port_id, &addr);
#ifdef XRAN_OWD_DEBUG_PKTS
    printf("id is %d\n", p_xran_dev_ctx->fh_init.io_cfg.id);
    printf("Port %u SRC MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
        " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
        (unsigned)port_id,
        addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
        addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);
#endif

    if (p_xran_dev_ctx->fh_init.io_cfg.id)
    {
#ifdef XRAN_OWD_DEBUG_PKTS
        int8_t *pa = &p_xran_dev_ctx->fh_init.p_o_du_addr[0];
        printf("DST_MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pa[0],pa[1],pa[2],pa[3],pa[4],pa[5]);
#endif
        rte_ether_addr_copy((struct rte_ether_addr *)&p_xran_dev_ctx->fh_init.p_o_du_addr[0], (struct rte_ether_addr *)&h->d_addr.addr_bytes[0]);

    }
    else
    {
#ifdef XRAN_OWD_DEBUG_PKTS
        int8_t *pb = &p_xran_dev_ctx->fh_init.p_o_ru_addr[0];
        printf("DST_MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pb[0],pb[1],pb[2],pb[3],pb[4],pb[5]);
#endif
        rte_ether_addr_copy((struct rte_ether_addr *)&p_xran_dev_ctx->fh_init.p_o_ru_addr[0], (struct rte_ether_addr *)&h->d_addr.addr_bytes[0]);

    }
#ifdef XRAN_OWD_DEBUG_PKTS
    uint8_t *pc = &h->s_addr.addr_bytes[0];
    printf(" Src MAC from packet: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pc[0],pc[1],pc[2],pc[3],pc[4],pc[5]);
    uint8_t *pd = &h->d_addr.addr_bytes[0];
    printf(" Dst MAC from packet: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pd[0],pd[1],pd[2],pd[3],pd[4],pd[5]);
#endif
    // Copy dest address from above
    // Send out the packet
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &mbuf, 1);
// Try using the normal scheme of passing through the ring
//    ret_value = xran_enqueue_mbuf(mbuf, ctx->tx_ring[port_id]);
#ifdef XRAN_OWD_DEBUG_PKTS
    printf("owdt rte_eth_tx_burst returns %d for port %d\n", ret_value,port_id);
#endif
    return ret_value;
}


/**
 * @brief Process a Delay Measurement Request packet
 *
 * @ingroup group_source_xran
 *
 * @param mbuf
 *  The pointer of the packet buffer to be processed
 * @param handle
 *  Pointer to an xran_device_ctx (cast)
 * @param xran_ecpri_delay_meas_pl
 * Pointer to an eCPRI delay measurement PL
 * @return
 *  OK on success
 *  FAIL if failed to process the packet
 */
int xran_process_delmeas_request(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = FAIL;

    struct xran_ecpri_delay_meas_pl *txDelayHdr;
    TimeStamp pt1;
    struct rte_mbuf* pkt1;
    char* pchar;
    uint64_t tcv1, tcv2,t2m,trm, td12, t1m;
    struct xran_ecpri_del_meas_pkt *pdm= NULL;
    union xran_ecpri_cmn_hdr *cmn;
    struct timespec tr, t2;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr addr;
    struct xran_ethdi_ctx *ctx = xran_ethdi_get_ctx();
//101620
    struct xran_io_cfg* cfg = &p_xran_dev_ctx->fh_init.io_cfg;
//    struct xran_io_cfg *cfg = &ctx->io_cfg;
    int32_t *port = &cfg->port[port_id];

#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("RX ecpri  Measure Request \n");
#endif
    // Since we are processing the receipt of a delay measurement request packet the following actions
    // need to be taken (Per eCPRI V2.0 Figure 25)
    // 1) Record the current timestamp when the message was received i.e. tr
    if (clock_gettime(CLOCK_REALTIME, &tr ))     // tr
    {
        errx(1, "Exit 1 owd rx f1 port_id %d", port_id);
        return ret_value;
    }

    trm = xran_timespec_to_ns(&tr);
    // 2) Copy MeasurementID to the Delay Measurement Response packet
    //     but first prepend ethernet header since the info is still in the buffer
//    pchar = rte_pktmbuf_prepend(pkt, (uint16_t)(sizeof(struct rte_ether_hdr)+ sizeof(union xran_ecpri_cmn_hdr ))); // Pointer to new data start address 10/20/20 Now not removing ecpri_cmn in process_delay_meas
    pchar = rte_pktmbuf_prepend(pkt, (uint16_t)sizeof(struct rte_ether_hdr));
    pkt1 = rte_pktmbuf_copy(pkt, _eth_mbuf_pool, 0, UINT32_MAX);
    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt*, sizeof(struct rte_ether_hdr));
    // 3) Get time stamp T1 from the Timestamp field i.e. t1
    pt1  = pdm->deMeasPl.ts;
    // 3a) Convert to ns in the host format
    t1m = xran_ptp_ts_to_ns(&pt1);
    // 4) Get the compensation value from the packet i.e. tcv1
    tcv1 = rte_be_to_cpu_64(pdm->deMeasPl.CompensationValue);
    // 5) Prepare the delay measurement response packet
    pdm->deMeasPl.ActionType = ECPRI_RESPONSE;
    // 6) Record the current timestamp at the moment that the delay measurement packet is ready to be transmitted i.e.t2 and write it
    //    to the Delay Measurement response packet PL field
    if (clock_gettime(CLOCK_REALTIME, &t2 ))     // t2
    {
        errx(1,"Exit 2 owd rx f1 port_id %d", port_id);
        return ret_value;
    }
    // 7) Convert host to ptp time stamp format for t2 and write to the outgoing packet
    xran_host_to_ptp_ts(&pdm->deMeasPl.ts, &t2);
    // 8) Convert from Timestamp t2 to ns before computing the compensation value
    t2m = xran_timespec_to_ns(&t2);
    // 9) Compute tcv2 as t2-tr
    tcv2 = t2m - trm;
    // 10) write cv2 to the CompensationValue field of the delay measurement response packet
    pdm->deMeasPl.CompensationValue = rte_cpu_to_be_64(tcv2);
    // 11) Fill the ethernet header properly by swapping src and dest addressed from the copied frame
    eth_hdr = rte_pktmbuf_mtod(pkt1, struct rte_ether_hdr *);
    /* Swap dest and src mac addresses. */
    rte_ether_addr_copy(&eth_hdr->d_addr, &addr);
    rte_ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
    rte_ether_addr_copy(&addr, &eth_hdr->s_addr);
    // Still need to check ol_flags state and update if necessary
    // Compute the delay td12 and save
    // Still need to define the DB to save the info and run averages
    td12 = t2m - tcv2 - (t1m + tcv1);
    // 12) Send the response right away
    struct rte_ether_hdr *h = (struct rte_ether_hdr *)rte_pktmbuf_mtod(pkt1, struct rte_ether_hdr*);
#ifdef XRAN_OWD_DEBUG_PKTS
    uint8_t *pc = &h->s_addr.addr_bytes[0];
    printf(" Src MAC from packet: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pc[0],pc[1],pc[2],pc[3],pc[4],pc[5]);
    uint8_t *pd = &h->d_addr.addr_bytes[0];
    printf(" Dst MAC from packet: %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", pd[0],pd[1],pd[2],pd[3],pd[4],pd[5]);
//    printf("EtherType: %04"PRIx16" \n",&h->ether_type);
#endif
    pdm  = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt *, sizeof(struct rte_ether_hdr) );
    pdm->cmnhdr.bits.ecpri_payl_size	 = 10 + powdc->owdm_PlLength; // 10 correponds to the xran_ecpri_delay_meas_pl minus the dummy_bytes field which now allows the user to select the length for this field to be sent
    pdm->cmnhdr.bits.ecpri_payl_size    = rte_cpu_to_be_16(pdm->cmnhdr.bits.ecpri_payl_size);
    pdm->cmnhdr.bits.ecpri_mesg_type = ECPRI_DELAY_MEASUREMENT;
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
    printf ("pdm has:%02"PRIx8" %04"PRIx16" %02"PRIx8" %02"PRIx8" \n", pdm->cmnhdr.bits.ecpri_mesg_type, pdm->cmnhdr.bits.ecpri_payl_size, pdm->cmnhdr.bits.ecpri_ver,pdm->deMeasPl.MeasurementID);
#endif

    // Copy dest address from above
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &pkt1, 1);  // Need to check for the proper method of getting the port and mac address
#ifdef  XRAN_OWD_DEBUG_MSG_FLOW
    printf ("in dly ms req sending response rte_eth_tx_burst returns %d for port %d\n",ret_value, *port);
#endif
    // 13) Update measurements DB and check if completed
    powdp->delaySamples[powdp->numMeas]= td12 ;
#ifdef XRAN_OWD_DEBUG_DELAY_INFO
    printf("Computed delay is %08"PRIx64" MeasNum %d portId %d id is %d \n",powdp->delaySamples[powdp->numMeas],powdp->numMeas, port_id, p_xran_dev_ctx->fh_init.io_cfg.id);
#endif

    powdp->numMeas++;

    if (powdp->numMeas == powdc->numberOfSamples)
    {
        xran_compute_and_report_delay_estimate(powdp, powdc->numberOfSamples, p_xran_dev_ctx->fh_init.io_cfg.id);
        powdp->msState = XRAN_OWDM_DONE;
        xran_if_current_state = XRAN_RUNNING;
    }
    else
    {

//        powdp->msState = XRAN_OWDM_IDLE;
        if (powdc->initiator_en)
        {
            // Reinitialize txDone for next pass
            powdp->txDone = 0;
#ifdef XRAN_OWD_DEBUG_MEAS_DB
            printf("Clear call 3 port id %d \n", port_id);
#endif
            xran_initialize_ecpri_del_meas_port(powdc, powdp,0);
        }
    }
    return 1;

}

int xran_process_delmeas_request_w_fup(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = FAIL;
    struct xran_ecpri_delay_meas_pl* txDelayHdr;
    TimeStamp pt2;
    struct rte_mbuf* pkt1;
    uint64_t trm;
    struct xran_ecpri_del_meas_pkt* pdm= ptr;
    struct timespec tr;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg *cfg = &ctx->io_cfg;
    int32_t* port = &cfg->port[port_id];

    // Since we are processing the receipt of a delay measurement request with follow up packet the following actions
    // need to be taken (Per eCPRI V2.0 Figure 26)
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("RX ecpri  Measure Request with fup\n");
#endif

    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod(pkt, struct xran_ecpri_del_meas_pkt*);
    // Record tr and save to memory with the associated measurement Id and Port
    // 1) Record the current timestamp when the message was received i.e. tr
    if (clock_gettime(CLOCK_REALTIME, &tr ))     // tr
    {
        errx(1, "Exit 1 owd rx f2 port_id %d",port_id);
        return ret_value;
    }
    trm = xran_timespec_to_ns(&tr);
    // Save trm so when the Follow Up packet is received we can compute tcv2 as t2-trm
    powdp->tr = trm;
    // Save the measurement Id
    powdp->currentMeasID = pdm->deMeasPl.MeasurementID;
    // Change the state to waiting for follow up
    powdp->msState = XRAN_OWDM_WAITFUP;

    return ret_value;

}

int xran_process_delmeas_response(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = 1;
    struct xran_ecpri_delay_meas_pl* txDelayHdr;
    TimeStamp pt2;
    struct rte_mbuf* pkt1;
    uint64_t tcv1, tcv2,t2m,trm, td12;
    struct xran_ecpri_del_meas_pkt* pdm;
    struct timespec tr, t2;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg *cfg = &ctx->io_cfg;
    struct xran_io_cfg* cfg1 = &p_xran_dev_ctx->fh_init.io_cfg;
    int32_t* port = &cfg->port[port_id];


    // Since we are processing the receipt of a delay measurement response packet the following actions
    // need to be taken (Per eCPRI V2.0 Figure 25)
    // Need to know if a Remote Request was processed against this measurement ID if so then the receipt of the response
    // is used to compute the one-way delay as td= (t2-tcv2) - (t1+tcv1) with t2, tcv2 contained in the packet and
    // t1 and tcv1 stored from the previous Remote Request packet processing task
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("RX ecpri  Measure Response \n");
#endif

    pdm = (struct xran_ecpri_del_meas_pkt*)(struct xran_ecpri_del_meas_pkt *)rte_pktmbuf_mtod(pkt,  struct xran_ecpri_del_meas_pkt *);
    // Save the measurement Id
    powdp->currentMeasID = pdm->deMeasPl.MeasurementID;

    // 1) Get time stamp T2 from the Timestamp field i.e. t2
    pt2  = pdm->deMeasPl.ts;

    // 2a) Convert to ns in the host format
    t2m = xran_ptp_ts_to_ns(&pt2);
    // 3) Get the compensation value from the packet i.e. tcv2
    tcv2 = rte_be_to_cpu_64(pdm->deMeasPl.CompensationValue);
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
    printf ("tcv2 at Gen is %08"PRIx64" \n",tcv2);
#endif
    // Compute the delay using the stored t1 and tcv1 used in the request message
    // td= (t2-tcv2) - (t1+tcv1) where t1 and tcv1 have been stored previously for the same measurement ID
#ifdef XRAN_OWD_DEBUG_TIME_STAMPS_INFO
    printf("Delay comp at orig has t2m %08"PRIx64"  tcv2 %08"PRIx64" t1 %08"PRIx64" delta %08"PRIx64" port_id %d \n", t2m,tcv2,powdp->t1 ,powdp->delta,port_id);
#endif
    powdp->delaySamples[powdp->numMeas]= (t2m-tcv2) -(powdp->t1 + powdp->delta);
#ifdef XRAN_OWD_DEBUG_DELAY_INFO
        printf("Computed delay is %08"PRIx64" MeasNum %d portId %d id is %d \n",powdp->delaySamples[powdp->numMeas],powdp->numMeas, port_id,p_xran_dev_ctx->fh_init.io_cfg.id );
#endif

    powdp->numMeas++;



    if (powdp->numMeas == powdc->numberOfSamples)
    {
        xran_compute_and_report_delay_estimate(powdp, powdc->numberOfSamples,p_xran_dev_ctx->fh_init.io_cfg.id);
        powdp->msState = XRAN_OWDM_DONE;
        xran_if_current_state= XRAN_RUNNING;
    }
    else
    {

//        powdp->msState = XRAN_OWDM_IDLE;
        if (powdc->initiator_en)
        {
            // Reinitialize txDone for next pass
            powdp->txDone = 0;
#ifdef XRAN_OWD_DEBUG_MEAS_DB
            printf("Clear call_4 port_id %d \n", port_id);
#endif
            xran_initialize_ecpri_del_meas_port(powdc, powdp,0);
#ifdef XRAN_OWD_DEBUG_MEAS_DB
            printf("Reseting done \n");
#endif

        }

    }
    // Needs work and change ret_value to OK
    return ret_value;
}

int xran_process_delmeas_rem_request(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = FAIL;
    struct xran_ecpri_delay_meas_pl* txDelayHdr;
    struct rte_mbuf* pkt1;
    uint64_t tcv1,tr2m,trm;
    struct xran_ecpri_del_meas_pkt* pdm;
    char* pchar;
    struct timespec tr2, tr;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr addr;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg *cfg = &ctx->io_cfg;
    int32_t* port = &cfg->port[port_id];

    // Since we are processing the receipt of a delay measurement remote request packet the following actions
    // need to be taken (Per eCPRI V2.0 Figure 25)
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("RX ecpri  Measure Remote Request \n");
#endif

    // 1) Record the current timestamp when the message was received i.e. t1
    if (clock_gettime(CLOCK_REALTIME, &tr ))     // t1
    {
        errx(1,"Exit 1 owd rx f4 port_id %d", port_id);
        return ret_value;
    }
    trm = xran_timespec_to_ns(&tr);
    // 2) Copy MeasurementID to the Delay Measurement Request packet
    //     but first prepend ethernet header since the info is still in the buffer
    pchar = rte_pktmbuf_prepend(pkt, (uint16_t)sizeof(struct rte_ether_hdr));
    pkt1 = rte_pktmbuf_copy(pkt, _eth_mbuf_pool, 0, UINT32_MAX);
    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt*, sizeof(struct rte_ether_hdr));

    // 3) Prepare the delay measurement request packet
    pdm->deMeasPl.ActionType = ECPRI_REQUEST;
    // 4) Record the current timestamp at the moment that the delay measurement packet is ready to be transmitted tr2 i.e.t1+tcv1 and write it
    //    to the Delay Measurement request packet PL field
    if (clock_gettime(CLOCK_REALTIME, &tr2 ))     // tr2
    {
        errx(1,"Exit 2 owd rx f4 port_id %d", port_id);
        return ret_value;
    }
    // 5) Convert host to ptp time stamp format for tr2 and write to the outgoing packet
    xran_host_to_ptp_ts(&pdm->deMeasPl.ts, &tr);
    // 6) Convert from Timestamp tr2 to ns before computing the compensation value
    tr2m = xran_timespec_to_ns(&tr2);
    // 7) Compute tcv1 as tr2m-trm
    tcv1 = tr2m - trm;
    // 8) write tcv1 to the CompensationValue field of the delay measurement request packet
    pdm->deMeasPl.CompensationValue = rte_cpu_to_be_64(tcv1);
    // 9) Fill the ethernet header properly by swapping src and dest addressed from the copied frame
    eth_hdr = rte_pktmbuf_mtod(pkt1, struct rte_ether_hdr *);
    /* Swap dest and src mac addresses. */
    rte_ether_addr_copy(&eth_hdr->d_addr, &addr);
    rte_ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
    rte_ether_addr_copy(&addr, &eth_hdr->s_addr);
    // 10) Send the response right away
    pdm  = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt *, sizeof(struct rte_ether_hdr) );
    pdm->cmnhdr.bits.ecpri_payl_size	 = 10 + powdc->owdm_PlLength; // 10 correponds to the xran_ecpri_delay_meas_pl minus the dummy_bytes field which now allows the user to select the length for this field to be sent
    pdm->cmnhdr.bits.ecpri_payl_size    = rte_cpu_to_be_16(pdm->cmnhdr.bits.ecpri_payl_size);
    pdm->cmnhdr.bits.ecpri_mesg_type = ECPRI_DELAY_MEASUREMENT;
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("Ecpri  Measure Sending Request Msg \n");
#endif
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &pkt1, 1);  // Need to check for the proper method of getting the port and mac address
    // Still need to check ol_flags state and update if necessary
    // Save the computed delays and the measurementId
    powdp->t1 = trm;
    powdp->delta = tcv1;
    powdp->currentMeasID = pdm->deMeasPl.MeasurementID;
    powdp->msState = XRAN_OWDM_WAITRESP;
    return ret_value;


}
int xran_process_delmeas_rem_request_w_fup(struct rte_mbuf* pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = FAIL;
    struct xran_ecpri_delay_meas_pl* txDelayHdr;
    TimeStamp pt2;
    struct rte_mbuf* pkt1;
    struct rte_mbuf* pkt2;
    uint64_t tcv1,tsm,t1;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr addr;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct xran_ecpri_del_meas_pkt* pdm;
    struct timespec tr, ts;
    char* pchar;


    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg *cfg = &ctx->io_cfg;
    int32_t* port = &cfg->port[port_id];
    tsm = 0;

    // Since we are processing the receipt of a delay measurement remote request with follow up packet the following
    // actions need to be taken (Per eCPRI V2.0 Figure 26)
    // record t1 for the packet arrival time and then prepare Request with follow up packet which uses 0 for timsetamp
    // and for correctionvalue.
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("RX ecpri  Measure Remote Request w Fup \n");
#endif
    // 1) Record the current timestamp when the message was received i.e. t1
    if (clock_gettime(CLOCK_REALTIME, &tr ))     // t1
    {
        errx(1,"Exit 1 owd rx f5 port_id %d", port_id);
        return ret_value;
    }
    t1 = xran_timespec_to_ns(&tr);
    // 2) Copy MeasurementID to the Delay Measurement Request packet
    //     but first prepend ethernet header since the info is still in the buffer
    pchar = rte_pktmbuf_prepend(pkt, (uint16_t)sizeof(struct rte_ether_hdr));
    pkt1 = rte_pktmbuf_copy(pkt, _eth_mbuf_pool, 0, UINT32_MAX);

    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt*, sizeof(struct rte_ether_hdr));


    // 3) Prepare the delay measurement request w fup packet
    pdm->deMeasPl.ActionType = ECPRI_REQUEST_W_FUP;
    // 4) Zero the ts and CompensationValue entries in the packet
    ts.tv_sec=0;
    ts.tv_nsec =0;
    // 5) Convert host to ptp time stamp format for t2 and write to the outgoing packet
    xran_host_to_ptp_ts(&pdm->deMeasPl.ts, &ts);
    // 6) write zero to the CompensationValue field of the delay measurement response packet
    pdm->deMeasPl.CompensationValue = rte_cpu_to_be_64(tsm);
    // 7) Fill the ethernet header properly by swapping src and dest addressed from the copied frame
    eth_hdr = rte_pktmbuf_mtod(pkt1, struct rte_ether_hdr *);
    /* Swap dest and src mac addresses. */
    rte_ether_addr_copy(&eth_hdr->d_addr, &addr);
    rte_ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
    rte_ether_addr_copy(&addr, &eth_hdr->s_addr);
    // 8) Duplicate packet to be used for the follow up packet
    pkt2 = rte_pktmbuf_copy(pkt1, _eth_mbuf_pool, 0, UINT32_MAX);
    // 9) Record the current timestamp when the request with follow up is being sent
    if (clock_gettime(CLOCK_REALTIME, &ts ))     // ts
    {
        errx(1,"Exit 2 owd rx f5 port_id %d", port_id);
        return ret_value;
    }
    // 10) Send the request with follow up
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("ecpri Measure sending Request with Fup \n");
#endif
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &pkt1, 1);  // Need to check for the proper method of getting the port and mac address

    // After the Request with follow up packet has been sent, prepare follow up packet with t1 and tcv1, where
    // tcv1 = ts - t1 and writing it to the outgoing packet
    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt2, struct xran_ecpri_del_meas_pkt*, sizeof(struct rte_ether_hdr));
    // 11) Prepare the delay measurement request with follow up packet
    pdm->deMeasPl.ActionType = ECPRI_FOLLOW_UP;
    // 12) Convert host to ptp time stamp format for t1 and write to the outgoing packet
    xran_host_to_ptp_ts(&pdm->deMeasPl.ts, &tr);
    // 13) Convert from Timestamp t2 to ns before computing the compensation value
    tsm = xran_timespec_to_ns(&ts);
    // 14) Compute tcv1 as tsm-t1
    tcv1 = tsm - t1;
    // 15) write cv1 to the CompensationValue field of the delay measurement response packet
    pdm->deMeasPl.CompensationValue = rte_cpu_to_be_64(tcv1);

    // 16) Send the follow up message
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("ecpri Measure sending Follow Up \n");
#endif
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &pkt2, 1);  // Need to check for the proper method of getting the port and mac address

    // Save trm since it will be used to compute tcv2 based on the arrival of the Follow Up packet
    powdp->currentMeasID = pdm->deMeasPl.MeasurementID;
    powdp->t1 = t1;
    powdp->delta = tcv1;
    powdp->msState =   XRAN_OWDM_WAITRESP;

    return ret_value;

}

int xran_process_delmeas_follow_up(struct rte_mbuf *pkt, void* handle, struct xran_ecpri_del_meas_pkt* ptr, uint16_t port_id)
{
    int ret_value = FAIL;
    struct xran_ecpri_delay_meas_pl *txDelayHdr;
    struct rte_mbuf *pkt1;
    char* pChar= NULL;
    uint64_t tcv1,tr2m, tcv2, t1;
    struct xran_ecpri_del_meas_pkt *pdm;
    struct timespec tr2, tr;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr addr;
    TimeStamp pt1;
    struct xran_device_ctx* p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_cmn* powdc  = &p_xran_dev_ctx->fh_init.io_cfg.eowd_cmn[p_xran_dev_ctx->fh_init.io_cfg.id];
    struct xran_ecpri_del_meas_port* powdp = &p_xran_dev_ctx->fh_init.io_cfg.eowd_port[p_xran_dev_ctx->fh_init.io_cfg.id][port_id];
    struct xran_ethdi_ctx *const ctx = xran_ethdi_get_ctx();
    struct xran_io_cfg *cfg = &ctx->io_cfg;
    int32_t *port = &cfg->port[0];
    // Since we are processing the receipt of a delay measurement follow up packet the following actions
    // need to be taken (Per eCPRI V2.0 Figure 26)
#ifdef XRAN_OWD_DEBUG_MSG_FLOW
    printf("ecpri Measure received Followup \n");
#endif

    // 1) Record the current timestamp when the message was received i.e. tr2
    if (clock_gettime(CLOCK_REALTIME, &tr2 ))     // tr2
    {
        errx(1,"Exit 1 owd rx f6 port_id %d", port_id);
        return ret_value;
    }
    tr2m = xran_timespec_to_ns(&tr2);


    // 2) Copy MeasurementID to the Delay Measurement Response packet
    //     but first prepend ethernet header since the info is still in the buffer
    pChar = rte_pktmbuf_prepend(pkt, (uint16_t)sizeof(struct rte_ether_hdr));
    pkt1 = rte_pktmbuf_copy(pkt, _eth_mbuf_pool, 0, UINT32_MAX);
    pdm = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt*, sizeof(struct rte_ether_hdr));

    // 3) Get time stamp T1 from the Timestamp field i.e. t1
    pt1  = pdm->deMeasPl.ts;
    // 4) Convert to ns in the host format
    t1 = xran_ptp_ts_to_ns(&pt1);
    // 5) Get the compensation value from the packet i.e. tcv1
    tcv1 = rte_be_to_cpu_64(pdm->deMeasPl.CompensationValue);

    // 6) Prepare the delay measurement response packet
    pdm->deMeasPl.ActionType = ECPRI_RESPONSE;

    // 7) Convert host to ptp time stamp format for tr2 and write to the outgoing packet
    xran_host_to_ptp_ts(&pdm->deMeasPl.ts, &tr2);
    // 8) Convert from Timestamp tr2 to ns before computing the compensation value
    tr2m = xran_timespec_to_ns(&tr2);
    // 9) Compute tcv2 as tr2m-trm
    tcv2 = tr2m - powdp->tr;
    // 0) write tcv2 to the CompensationValue field of the delay measurement request packet
    pdm->deMeasPl.CompensationValue = rte_cpu_to_be_64(tcv2);
    // 9) Fill the ethernet header properly by swapping src and dest addressed from the copied frame
    eth_hdr = rte_pktmbuf_mtod(pkt1, struct rte_ether_hdr *);
    /* Swap dest and src mac addresses. */
    rte_ether_addr_copy(&eth_hdr->d_addr, &addr);
    rte_ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
    rte_ether_addr_copy(&addr, &eth_hdr->s_addr);
    pdm  = (struct xran_ecpri_del_meas_pkt*)rte_pktmbuf_mtod_offset(pkt1, struct xran_ecpri_del_meas_pkt *, sizeof(struct rte_ether_hdr) );
    pdm->cmnhdr.bits.ecpri_payl_size	 = 10 + powdc->owdm_PlLength; // 10 correponds to the xran_ecpri_delay_meas_pl minus the dummy_bytes field which now allows the user to select the length for this field to be sent
    pdm->cmnhdr.bits.ecpri_payl_size    = rte_cpu_to_be_16(pdm->cmnhdr.bits.ecpri_payl_size);
    pdm->cmnhdr.bits.ecpri_mesg_type = ECPRI_DELAY_MEASUREMENT;

    // 10) Send the response right away
    ret_value = rte_eth_tx_burst((uint16_t)*port, 0, &pkt1, 1);  // Need to check for the proper method of getting the port and mac address

    // Compute the delay using the stored t1 and tcv1 used in the request message
    // td= (t2-tcv2) - (t1+tcv1) where t1 and tcv1 have been stored previously for the same measurement ID
    powdp->delaySamples[powdp->numMeas]= (tr2m-tcv2) -(t1 + tcv1);
#ifdef XRAN_OWD_DEBUG_DELAY_INFO
    printf("Computed delay is %08"PRIx64" MeasNum %d portId %d id %d \n",powdp->delaySamples[powdp->numMeas],powdp->numMeas,port_id,p_xran_dev_ctx->fh_init.io_cfg.id);
#endif
    powdp->numMeas++;

    if (powdp->numMeas == powdc->numberOfSamples)
    {
        xran_compute_and_report_delay_estimate(powdp, powdc->numberOfSamples, p_xran_dev_ctx->fh_init.io_cfg.id);
        powdp->msState = XRAN_OWDM_DONE;
        xran_if_current_state = XRAN_RUNNING;
    }
    else
    {
;
//        powdp->msState = XRAN_OWDM_IDLE;
        if (powdc->initiator_en)
        {
            // Reinitialize txDone for next pass
            powdp->txDone = 0;
#ifdef XRAN_OWD_DEBUG_MEAS_DB
            printf("Clear Call_5 port_id %d \n", port_id);
#endif
            xran_initialize_ecpri_del_meas_port(powdc, powdp,0);
        }
    }

    return ret_value;

}


/**
 * @brief Parse a Delay Measurement packet
 *  Transport layer fragmentation is not supported.
 *
 * @ingroup group_source_xran
 *
 * @param mbuf
 *  The pointer of the packet buffer to be parsed
 * @param handle
 *  Pointer to an xran_device_ctx (cast)
 * @return
 *  OK on success
 *  FAIL if failed to process the packet
 */
int process_delay_meas(struct rte_mbuf *pkt,  void* handle, uint16_t port_id)
{
    struct xran_device_ctx * p_xran_dev_ctx = (struct xran_device_ctx *)handle;
    struct xran_ecpri_del_meas_pkt *ecpri_delmeas_pkt;
    union  xran_ecpri_cmn_hdr * ecpricmn;
    int ret_value = FAIL;
#ifdef XRAN_OWD_DEBUG_PKTS
    printf("pdm Device is %d\n", p_xran_dev_ctx->fh_init.io_cfg.id);
#endif
        /* Process eCPRI cmn header. */

 //   (void *)rte_pktmbuf_adj(pkt, sizeof(*ecpricmn));
    ecpri_delmeas_pkt = (struct xran_ecpri_del_meas_pkt *)rte_pktmbuf_mtod(pkt,  struct xran_ecpri_del_meas_pkt *);
    // The processing of the delay measurement here corresponds to eCPRI sections 3.2.4.6.2 and 3.42.6.3

    switch(ecpri_delmeas_pkt->deMeasPl.ActionType) {
        case ECPRI_REQUEST:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc rx  Dly Meas Req\n");
#endif
            ret_value = xran_process_delmeas_request(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
            break;
        case ECPRI_REQUEST_W_FUP:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas rx Req w Fup\n");
#endif
            ret_value = xran_process_delmeas_request_w_fup(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
            break;
        case ECPRI_RESPONSE:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas rx Resp\n");
#endif
            ret_value = xran_process_delmeas_response(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
            break;
        case ECPRI_REMOTE_REQ:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas rx Rem Req\n");
#endif
            ret_value = xran_process_delmeas_rem_request(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
           break;
        case ECPRI_REMOTE_REQ_W_FUP:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas Rem rx Req w Fup\n");
#endif
            ret_value = xran_process_delmeas_rem_request_w_fup(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
           break;
        case ECPRI_FOLLOW_UP:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas rx Fup\n");
#endif
            ret_value = xran_process_delmeas_follow_up(pkt, p_xran_dev_ctx, ecpri_delmeas_pkt, port_id);
           break;
        default:
#ifdef ORAN_OWD_DEBUG_MSG_FLOW
            printf("Proc Dly Meas default\n");
#endif
           break;
    }
    return ret_value;

}
