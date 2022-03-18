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
 * @brief This file is test MAC wls lib main process
 * @file mac_main.c
 * @ingroup group_testmacwls
 * @author Intel Corporation
 **/


#include <stdint.h>
#include <stdio.h>

#include "wls_lib.h"
#include "mac_wls.h"

#define WLS_TEST_DEV_NAME "wls"
#define WLS_TEST_MSG_ID   1
#define WLS_TEST_MSG_SIZE 100

#define N_MAC_MSGS   16

int main()
{
    p_fapi_api_queue_elem_t p_list_elem;
    unsigned int ret;
    unsigned int n= N_MAC_MSGS; 
    unsigned int i;

    // DPDK init
    ret = mac_dpdk_init();
    if (ret)
    {
        printf("\n[MAC] DPDK Init - Failed\n");
        return FAILURE;
    }
    printf("\n[MAC] DPDK Init - Done\n");

    wls_mac_init(WLS_TEST_DEV_NAME, WLS_MSG_BLOCK_SIZE);
    printf("\n[MAC] WLS Init - Done\n");

    for (i=0; i< N_MAC_MSGS; i++)
    {
    p_list_elem = wls_mac_create_elem(WLS_TEST_MSG_ID, WLS_TEST_MSG_SIZE, 1, 0);
      printf("\n[MAC] MAC Create Element %d- Done\n", i);

    if(p_list_elem)
    {
        wls_mac_send_msg_to_phy((void *)p_list_elem);
        	printf("\n[MAC] Send to FAPI %d- Done\n",i);
    	}
    }

    // Receive from FAPI WLS
    wls_mac_rx_task();

    printf("\n[MAC] Exiting...\n");

    return SUCCESS;
}

