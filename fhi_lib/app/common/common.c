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

#ifndef _XRAN_APP_COMMON_
#define _XRAN_APP_COMMON_

#include <assert.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#include "../common/common.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "xran_cp_api.h"
#include "xran_up_api.h"
#include "../src/xran_printf.h"


#define MBUFS_CNT 256

extern enum app_state state;

uint8_t numCCPorts = 1;
/* Number of antennas supported by front-end */

uint8_t num_eAxc = 4;
/* Number of CPRI ports supported by front-end */

int16_t *p_tx_play_buffer[MAX_ANT_CARRIER_SUPPORTED];
int32_t tx_play_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
int32_t tx_play_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

int16_t *p_rx_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
int32_t rx_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
int32_t rx_log_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

int16_t *p_prach_log_buffer[MAX_ANT_CARRIER_SUPPORTED];
int32_t prach_log_buffer_size[MAX_ANT_CARRIER_SUPPORTED];
int32_t prach_log_buffer_position[MAX_ANT_CARRIER_SUPPORTED];

int16_t *p_tx_buffer[MAX_ANT_CARRIER_SUPPORTED];
int32_t tx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

int16_t *p_rx_buffer[MAX_ANT_CARRIER_SUPPORTED];
int32_t rx_buffer_size[MAX_ANT_CARRIER_SUPPORTED];

void sys_save_buf_to_file(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Storing %s to file %s: ", bufname, filename);
            file = fopen(filename, "wb");
            if (file == NULL)
            {
                print_err("can't open file %s!!!", filename);
            }
            else
            {
                uint32_t             num;
                num = fwrite(pBuffer, buffers_num, size, file);
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) bytes num (%d)", (uint64_t)pBuffer, size, num);
            }
            printf(" \n");
        }
        else
        {
            print_err(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        print_err(" the %s is free: size = %d bytes!!!", bufname, size);
    }
}

int sys_load_file_to_buff(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    unsigned int  file_size = 0;
    int  num= 0;

    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Loading file %s to  %s: ", filename, bufname);
            file = fopen(filename, "rb");


            if (file == NULL)
            {
                print_err("can't open file %s!!!", filename);
                exit(-1);
            }
            else
            {
                fseek(file, 0, SEEK_END);
                file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                if ((file_size > size) || (file_size == 0))
                    file_size = size;

                printf("Reading IQ samples from file: File Size: %d [Buffer Size: %d]\n", file_size, size);

                num = fread(pBuffer, buffers_num, size, file);
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) bytes num (%d)", (uint64_t)pBuffer, file_size, num);
            }
            printf(" \n");

        }
        else
        {
            print_err(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        print_err(" the %s is free: size = %d bytes!!!", bufname, size);
    }
    return num;
}


void sys_save_buf_to_file_txt(char *filename, char *bufname, unsigned char *pBuffer, unsigned int size, unsigned int buffers_num)
{
    unsigned int i;
    int ret = 0;
    if (pBuffer == NULL)
        return;

    if (size)
    {
        if (filename && bufname)
        {
            FILE           *file;
            printf("Storing %s to file %s: ", bufname, filename);
            file = fopen(filename, "w");
            if (file == NULL)
            {
                print_err("can't open file %s!!!", filename);
                exit(-1);
            }
            else
            {
                uint32_t num = 0;

                signed short *ptr = (signed short*)pBuffer;
                for (i = 0; i < (size/((unsigned int)sizeof(signed short) /** 2 * 2 * 2*/)); i = i + 2)
                {
                    ret = fprintf(file,"%d %d\n", ptr[i], ptr[i + 1]);
                    if (ret < 0)
                    {
                        printf("fprintf %d\n", ret);
                        fclose(file);
                        break;
                    }
                    num++;
                }
                fflush(file);
                fclose(file);
                printf("from addr (0x%lx) size (%d) IQ num (%d)", (uint64_t)pBuffer, size, num);
            }
            printf(" \n");
        }
        else
        {
            print_err(" the file name, buffer name are not set!!!");
        }
    }
    else
    {
        print_err(" the %s is free: size = %d bytes!!!", bufname, size);
    }
}


#endif /* _XRAN_APP_COMMON_ */
