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
 * @brief This file has utilities to parse the parameters passed in through the command
 *        line and configure the application based on these parameters
 * @file aux_cline.c
 * @ingroup xran
 * @author Intel Corporation
 **/


#include "aux_cline.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>


typedef struct _CLINE_KEY_TABLE_
{
    char *name;
    char *value;
} CLINE_KEY_TABLE;


#define CLINE_MAXKEYS               (256)
#define CLINE_MAX_STRING_LENGTH     (128)
#define CLINE_MAXKEYSIZE            (2048)

static CLINE_KEY_TABLE PhyAppKeys[CLINE_MAXKEYS];
static uint32_t pPhyCfgNumEntries = 0;



//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param   void
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  Initialize Cline Interface
 *
**/
//-------------------------------------------------------------------------------------------
int cline_init(void)
{
    uint32_t i;
    pPhyCfgNumEntries = 0;
    for (i = 0; i < CLINE_MAXKEYS; i++)
    {
       PhyAppKeys[i].name = NULL;
       PhyAppKeys[i].value = NULL;
    }

    return AUX_SUCCESS;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *name String to search for
 *  @param[out]  *value Pointer to location where number needs to be stored
 *  @param[in]   deflt Default value to put into the value field if string is not found
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion searchs for a string from the phycfg.xml file and returns the number that
 *  was associated in file
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_set_int(const char *name, int *value, int deflt)
{
    uint32_t i;

    for (i = 0; i < pPhyCfgNumEntries; i++)
    {
        if (PhyAppKeys[i].name)
        {
            if (strcasecmp(name, PhyAppKeys[i].name) == 0)
            {
                char *p1 = PhyAppKeys[i].value;
                if (strstr(p1, "0x") || strstr(p1, "0X"))
                {
                    uint64_t core;
                    if (cline_covert_hex_2_dec(p1, &core) != AUX_SUCCESS)
                    {
                        printf("cline_set_int Failed (%s)\n", p1);
                        return AUX_FAILURE;
                    }
                    *value = (int)core;
                }
                else
                {
                    *value = (int)strtol(PhyAppKeys[i].value, NULL, 0);
                    if ((*value == 0 && errno == EINVAL) || (*value == INT_MAX && errno == ERANGE))
                    {
                        *value = deflt;
                        return AUX_FAILURE;
                    }
                }
                break;
            }
        }
        else
        {
            // End of list reached
            *value = deflt;
            printf("incomming setting in XML: param \"%s\" is not found!\n", name);
            return AUX_FAILURE;
        }
    }

    return AUX_SUCCESS;
}





//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *name String to search for
 *  @param[out]  *value Pointer to location where number needs to be stored
 *  @param[in]   deflt Default value to put into the value field if string is not found
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion searchs for a string from the phycfg.xml file and returns the number that
 *  was associated in file
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_set_uint64(const char *name, uint64_t *value, uint64_t deflt)
{
    uint32_t i;

    for (i = 0; i < pPhyCfgNumEntries; i++)
    {
        if (PhyAppKeys[i].name)
        {
            if (strcasecmp(name, PhyAppKeys[i].name) == 0)
            {
                if (strstr(PhyAppKeys[i].value, "0x") || strstr(PhyAppKeys[i].value, "0X"))
                {
                    if (cline_covert_hex_2_dec(PhyAppKeys[i].value, value) != AUX_SUCCESS)
                    {
                        printf("cline_covert_hex_2_dec Failed (%s)\n", PhyAppKeys[i].value);
                        return AUX_FAILURE;
                    }
                }
                else
                {
                    *value = strtoull(PhyAppKeys[i].value, NULL, 0);
                }
                if ((*value == 0 && errno == EINVAL) || (*value == LONG_MAX && errno == ERANGE))
                {
                    *value = deflt;
                    return AUX_FAILURE;
                }
                break;
            }
        }
        else
        {
            // End of list reached
            *value = deflt;
            printf("incomming setting in XML: param \"%s\" is not found!\n", name);
            return AUX_FAILURE;
        }
    }

    return AUX_SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *pStr String to search for
 *  @param[out]  *pDst Pointer to location where number needs to be stored
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion takes a char string as input and converts it to a unit61_t
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_covert_hex_2_dec(char *pStr, uint64_t *pDst)
{
    char nibble;
    int32_t i;
    uint32_t len;
    uint64_t value = 0, mult = 1, nibble_val = 0;

    // Skip over "0x"
    pStr += 2;

    len = strlen(pStr);
    if (len > 16)
    {
        printf("String Length is invalid: %p [%d]\n", pStr, len);
    }

    for (i = len - 1; i >= 0; i--)
    {
        nibble = pStr[i];
        if ((nibble >= '0') && (nibble <= '9'))
        {
            nibble_val = nibble - '0';
        }
        else if ((nibble >= 'A') && (nibble <= 'F'))
        {
            nibble_val = nibble - 'A' + 10;
        }
        else if ((nibble >= 'a') && (nibble <= 'f'))
        {
            nibble_val = nibble - 'a' + 10;
        }
        else
        {
            printf("String is invalid: %p[%d] %c\n", pStr, i, nibble);
            return AUX_FAILURE;
        }

        value += (nibble_val * mult);

        mult = mult * 16;
    }

    *pDst = value;

    return AUX_SUCCESS;
}


//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *name String to search for
 *  @param[out]  *core Pointer to location where core id needs to be stored
 *  @param[out]  *priority Pointer to location where priority needs to be stored
 *  @param[out]  *policy Pointer to location where policy needs to be stored
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion searchs for a string from the phycfg.xml file stores all thread related info into
 *  output locations
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_set_thread_info(const char *name, uint64_t *core, int *priority, int *policy)
{
#ifndef TEST_APP
    uint32_t i;
    int sched;
    char *p1, *p2, *p3;

    for (i = 0; i < pPhyCfgNumEntries; i++)
    {
        if (PhyAppKeys[i].name)
        {
            if (strcasecmp(name, PhyAppKeys[i].name) == 0)
            {
                p1 = (char*)PhyAppKeys[i].value;
                p2 = strstr(p1, ",");
                if (p2)
                {
                    *p2 = '\0';
                    p2++;
                    p3 = strstr(p2, ",");
                    if (p3)
                    {
                        *p3 = '\0';
                        p3++;

                        if (strstr(p1, "0x") || strstr(p1, "0X"))
                        {
                            if (cline_covert_hex_2_dec(p1, core) != AUX_SUCCESS)
                            {
                                printf("cline_covert_hex_2_dec Failed (%s)\n", p1);
                                return AUX_FAILURE;
                            }
                        }
                        else
                        {
                            *core = strtoull(p1, NULL, 0);
                        }
                        *priority = strtol(p2, NULL, 0);
                        sched = strtol(p3, NULL, 0);

                        *policy = (sched ? SCHED_RR : SCHED_FIFO);

                        //print_info_log("%s %ld %d %d\n", name, *core, *priority, *policy);

                        return AUX_SUCCESS;
                    }
                    else
                    {
                        printf("p3 is null %s\n", p2);
                    }
                }
                else
                {
                    printf("p2 is null %s\n", p1);
                }

                printf("%s FAIL1\n", name);
                return AUX_FAILURE;
            }
        }
        else
        {
            // End of list reached
            printf("incomming setting in XML: param \"%s\" is not found!\n", name);

            printf("cline_set_thread_info: %s FAIL2\n", name);
            return AUX_FAILURE;
        }
    }

    printf("cline_set_thread_info: %s FAIL3\n", name);
    return AUX_FAILURE;
#else
    return AUX_SUCCESS;
#endif
}

//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *name String to search for
 *  @param[in]   maxLen Max lenth of output array
 *  @param[out]  *dataOut Pointer to the data array filled by each int element of the input string
 *  @param[out]  *outLen Filled length of the array
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion searchs for a string from the phycfg.xml file stores all int value to output array
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_set_int_array(const char *name, int maxLen, int *dataOut, int *outLen)
{
    uint32_t i;
    char *p1, *p2;
    *outLen = 0;

    for (i = 0; i < pPhyCfgNumEntries; i++)
    {
        if (PhyAppKeys[i].name)
        {
            if (strcasecmp(name, PhyAppKeys[i].name) == 0)
            {
                p1 = (char*)PhyAppKeys[i].value;
                while(*outLen < maxLen)
                {
                    p2 = strstr(p1, ",");
                    if(p2)
                    {
                        *p2 = '\0';
                        p2 ++;
                        dataOut[*outLen] = strtol(p1, NULL, 0);
                        //printf("\ngranularity %d in idx %d",dataOut[*outLen],*outLen);
                        p1 = p2;
                        *outLen += 1;
                    }
                    else
                    {
                        dataOut[*outLen] = strtol(p1, NULL, 0);
                        //printf("\ngranularity %d in idx %d",dataOut[*outLen],*outLen);
                        *outLen += 1;
                        break;
                    }
                }
                return AUX_SUCCESS;
            }
        }
    }

    //printf("cline_set_int_array: Could not find %s\n", name);

    return AUX_SUCCESS;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *name String to search for
 *  @param[out]  *value Pointer to location where string needs to be stored
 *  @param[in]   deflt Default value to put into the value field if string is not found
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This funtion searchs for a string from the phycfg.xml file and returns the string that
 *  was associated in file
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_set_str(const char *name, char *value, const char *deflt)
{
    uint32_t i;

    for (i = 0; i < CLINE_MAXKEYS; i++)
    {
        if (PhyAppKeys[i].name)
        {
            if (strcasecmp(name, PhyAppKeys[i].name) == 0)
            {
                strcpy(value, PhyAppKeys[i].value);
                break;
            }
        }
        else
        {
            // End of list reached
            strcpy(value, deflt);
            return AUX_FAILURE;
        }
    }

    return AUX_SUCCESS;
}




//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   *pString Pointer to string that needs to be parsed
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This function takes a line from phycfg.xml and parses it and if valid fields are found,
 *  stores the xml tag and the value associated with the tag into a global structure.
 *
**/
//-------------------------------------------------------------------------------------------
int cline_parse_line(char *pString)
{
    char *stringLocal, *ptr1, *ptr2, *ptr3;
//    char stringName[CLINE_MAX_STRING_LENGTH] = "", stringValue[CLINE_MAX_STRING_LENGTH] = "";
    char *stringName, *stringValue;

    stringLocal = NULL;
    stringName = NULL;
    stringValue = NULL;
    ptr1 = ptr2 = ptr3 = NULL;
    uint32_t strLen = strlen(pString);
    if (strLen)
    {
        stringLocal = (char *)malloc(strLen + 1);
        if (stringLocal == NULL)
        {
            printf("Cannot allocate stringLocal of size %d\n", (strLen + 1));
            return AUX_FAILURE;
        }
    }

    // Dont Destroy Original String
    if (stringLocal)
        strcpy(stringLocal, pString);

    if (stringLocal)
    {
        if (strlen(stringLocal) <= 2)           // Probably line feed
        {
            if (stringLocal)
                free(stringLocal);
            return AUX_SUCCESS;
        }
    }
    ptr1 = stringLocal;

    // Locate Starting
    if (ptr1)
        ptr2 = strstr(ptr1, "<");
    if (ptr2 == NULL)
    {
        if (stringLocal)
            free(stringLocal);
        printf("no begin at parameters string");
        return AUX_FAILURE;
    }

    // See if this is a comment
    if (ptr2)
        ptr3 = strstr(ptr2, "!--");
    if (ptr3 != NULL)
    {
        if (stringLocal)
            free(stringLocal);
        return AUX_SUCCESS;
    }

    // Locate Ending
    if (ptr2)
        ptr3 = strstr(ptr2, ">");
    if (ptr3 == NULL)
    {
        if (stringLocal)
            free(stringLocal);
        printf("no ending at parameters string");
        return AUX_FAILURE;
    }

    // Copy string
    if (ptr3)
        *ptr3 = '\0';
    if (ptr2)
        strLen = strlen(ptr2 + 1);
    if (strLen)
    {
        stringName = (char *)malloc(strLen + 1);
        if (stringName == NULL)
        {
            if (stringLocal)
                free(stringLocal);
            printf("Cannot allocate stringName of size %d\n", (strLen + 1));
            return AUX_FAILURE;
        }
        else
        {
            if (ptr2)
                strcpy(stringName, ptr2 + 1);
        }
    }

    ptr1 = ptr3+1;

    // Locate Starting
    if (ptr1)
        ptr2 = strstr(ptr1, "<");
    if (ptr2 != NULL)
    {
        // Locate Ending
        if (ptr2)
            ptr3 = strstr(ptr2, ">");
        if (ptr3 == NULL)
        {
            if (stringName)
                free(stringName);
            if (stringLocal)
                free(stringLocal);
            printf("no ending at parameters string");
            return AUX_FAILURE;
        }

        // Copy string
        strLen = 0;
        if (ptr2)
            *ptr2 = '\0';
        if (ptr1)
            strLen = strlen(ptr1);
        if (strLen)
        {
            stringValue = (char *)malloc(strLen + 1);
            if (stringValue == NULL)
            {
                if (stringName)
                    free(stringName);
                if (stringLocal)
                    free(stringLocal);
                printf("Cannot allocate stringValue of size %d\n", (strLen + 1));
                return AUX_FAILURE;
            }
            if (ptr1)
                strcpy(stringValue, ptr1);
        }

#ifdef WIN32
        printf("Found String: %s with Value: %s\n", stringName, stringValue);
#endif
        {
            uint32_t len = 0;

            if (stringName)
            {
                len = strlen(stringName);
                if (len)
                {
                    PhyAppKeys[pPhyCfgNumEntries].name = (char *) malloc(len+1);
                    if (PhyAppKeys[pPhyCfgNumEntries].name == NULL)
                    {
                        if (stringName)
                            free(stringName);
                        if (stringLocal)
                            free(stringLocal);
                        if (stringValue)
                            free(stringValue);
                        printf("Cannot allocate PhyAppKeys[pPhyCfgNumEntries].name of size %d\n", (strLen + 1));
                        return AUX_FAILURE;
                    }
                    if (stringName)
                        strcpy(PhyAppKeys[pPhyCfgNumEntries].name, stringName);
                }
            }

            len = 0;
            if (stringValue)
            {
                len = strlen(stringValue);
                if (len)
                {
                    PhyAppKeys[pPhyCfgNumEntries].value = (char *)malloc(len + 1);
                    if (PhyAppKeys[pPhyCfgNumEntries].value == NULL)
                    {
                        if (stringName)
                            free(stringName);
                        if (stringLocal)
                            free(stringLocal);
                        if (stringValue)
                            free(stringValue);
                        printf("Cannot allocate PhyAppKeys[pPhyCfgNumEntries].value of size %d\n", (strLen + 1));
                        return AUX_FAILURE;
                    }
                    if (stringValue)
                        strcpy(PhyAppKeys[pPhyCfgNumEntries].value, stringValue);
                }
            }
        }
        pPhyCfgNumEntries++;
    }

    if(stringLocal)
        free(stringLocal);
    if (stringName)
        free(stringName);
    if (stringValue)
        free(stringValue);

    return AUX_SUCCESS;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param   void
 *
 *  @return  void
 *
 *  @description
 *  This function prints all the tags and values found in the phycfg.xml file after parsing
 *
**/
//-------------------------------------------------------------------------------------------
void cline_print_info(void)
{
#ifndef TEST_APP
    uint32_t i;

    for (i = 0; i < pPhyCfgNumEntries; i++)
    {
        if (PhyAppKeys[i].name)
        {
            printf(" --%s=%s\n", PhyAppKeys[i].name, PhyAppKeys[i].value);
        }
    }
    printf("\n");
#endif
    return;
}



//-------------------------------------------------------------------------------------------
/** @ingroup group_source_auxlib_cline
 *
 *  @param[in]   argc Number of command line params
 *  @param[in]   *argv[] Array of command line params
 *  @param[in]   pString String to search for
 *  @param[out]   *pDest Location where to store the payload
 *
 *  @return  0 if AUX_SUCCESS
 *
 *  @description
 *  This function looks for a string passed in from the command line and returns the immediate
 *  next parameter passed after this.
 *
**/
//-------------------------------------------------------------------------------------------
uint32_t cline_get_string(int argc, char *argv[], char* pString, char *pDest)
{
    uint32_t ret = AUX_FAILURE;
    int i = 1, length = (int)strlen(pString);
    char *filename = NULL;

    //print_info_log("Searching for string: %s. Length of string: %d\n", pString, length);
    while (i < argc)
    {
        if (strstr(argv[i], pString) != NULL)
        {
            filename = strstr(argv[i], pString);
            filename += (length + 1);

            //print_info_log("Found %s: Val = %s\n", pString, filename);
            strcpy(pDest, filename);

            ret = AUX_SUCCESS;
            break;
        }

        i++;
    }

    return ret;
}
