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
 * @brief This file is test PHY wls lib main process
 * @file phy_main.c
 * @ingroup group_testphywls
 * @author Intel Corporation
 **/


#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_launch.h>

#include "wls_lib.h"

#define SUCCESS 0
#define FAILURE 1
#define WLS_TEST_DEV_NAME "wls"
#define WLS_TEST_MSG_ID   1
#define WLS_TEST_MSG_SIZE 100
#define WLS_MAC_MEMORY_SIZE 0x3EA80000
#define WLS_PHY_MEMORY_SIZE 0x18000000
#define NUM_PHY_MSGS  16

typedef void* WLS_HANDLE;
void *g_shmem;
uint64_t g_shmem_size;

WLS_HANDLE  g_fapi_wls, g_phy_wls;

uint8_t    phy_dpdk_init(void);
uint8_t    phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize);
uint64_t   phy_fapi_recv();
uint8_t    phy_fapi_send();

int main()
{
    int64_t ret;
    uint64_t p_msg;

    // DPDK init
    ret = phy_dpdk_init();
    if (ret)
    {
        printf("\n[PHY] DPDK Init - Failed\n");
        return FAILURE;
    }
    printf("\n[PHY] DPDK Init - Done\n");

    // WLS init
    ret = phy_wls_init(WLS_TEST_DEV_NAME, WLS_MAC_MEMORY_SIZE, WLS_PHY_MEMORY_SIZE);
    if(ret)
    {
        printf("\n[PHY] WLS Init - Failed\n");
        return FAILURE;
    }
    printf("\n[PHY] WLS Init - Done\n");

    // Receive from MAC WLS
    p_msg = phy_fapi_recv();
    if (!p_msg)
    {
        printf("\n[PHY] Receive from FAPI - Failed\n");
        return FAILURE;
    }
    printf("\n[PHY] Receive from FAPI - Done\n");

    // Sent to PHY WLS
    ret = phy_fapi_send();
    if (ret)
    {
        printf("\n[PHY] Send to FAPI - Failed\n");
        return FAILURE;
    }
    printf("\n[PHY] Send to FAPI - Done\n");

    printf("\n[PHY] Exiting...\n");

    return SUCCESS;
}

uint8_t phy_dpdk_init(void)
{
    char whitelist[32];
    uint8_t i;

    char *argv[] = {"phy_app", "--proc-type=primary",
        "--file-prefix", "wls", whitelist};
    
    int argc = RTE_DIM(argv);

    /* initialize EAL first */
    sprintf(whitelist, "-a%s",  "0000:00:06.0");
    printf("[PHY] Calling rte_eal_init: ");

    for (i = 0; i < RTE_DIM(argv); i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

    if (rte_eal_init(argc, argv) < 0)
        rte_panic("Cannot init EAL\n");

    return SUCCESS;
}

uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize)
{
    g_phy_wls = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
    if(NULL == g_phy_wls)
    {
        return FAILURE;
    }
    g_shmem_size = nWlsMacMemSize+nWlsPhyMemSize;

    g_shmem = WLS_Alloc(g_phy_wls, g_shmem_size);
    if (NULL == g_shmem)
    {
        printf("Unable to alloc WLS Memory\n");
        return FAILURE;
    }
    return SUCCESS;
}

uint64_t phy_fapi_recv()
{
    uint8_t  num_blks = 0;
    uint64_t p_msg;
    uint32_t msg_size;
    uint16_t msg_id;
    uint16_t flags;
	uint32_t i=0;

		
	while (1)
	{
    num_blks = WLS_Wait(g_phy_wls);
            printf("WLS_Wait returns %d blocks\n",num_blks);
    
    if (num_blks)
    {
        p_msg = WLS_Get(g_phy_wls, &msg_size, &msg_id, &flags);
        		if (p_msg)
        		{
        			printf("\n[PHY] FAPI2PHY WLS Received Block %d\n",i);
        			i++;
    }
    else
    {
        	  	     printf("\n[PHY] FAPI2PHY WLS Get Error for msg %d\n",i);
        	  	     break;
        	    }
                if (flags & WLS_TF_FIN)
                {
                    return p_msg;
                }
    		}
    		else
    		{
        		printf("\n[PHY] FAPI2PHY WLS wait returned 0 blocks exiting \n");
        		return FAILURE;
    		}
    	
    }
    return p_msg;
}

uint8_t phy_fapi_send()
{
    uint64_t pa_block = 0;
    uint8_t ret = FAILURE;
    uint32_t i;
    
    for (i=0 ; i < NUM_PHY_MSGS; i++)
    {

    pa_block = (uint64_t) WLS_DequeueBlock((void*) g_phy_wls);
    if (!pa_block)
    {
        	printf("\n[PHY] FAPI2PHY WLS Dequeue block %d error\n",i);
        return FAILURE;
    }

    	ret = WLS_Put(g_phy_wls, pa_block, WLS_TEST_MSG_SIZE, WLS_TEST_MSG_ID, (i== (NUM_PHY_MSGS-1))? WLS_TF_FIN:0);
    	printf("\n[PHY] FAPI2PHY WLS Put Msg %d \n",i);
    	if (ret)
    	{
    		printf("\n[PHY] FAPI2PHY WLS Put Msg Error %d \n",i);
      }
    }
    return ret;
}
