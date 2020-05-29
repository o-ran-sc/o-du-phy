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

#include "nr5g_fapi_args.h"
#include <stddef.h>
#include <getopt.h>

static const char usage[] =
    "                                                                               \n"
    "    %s <APP PARAMS>                                                            \n"
    "                                                                               \n"
    "Application mandatory parameters:                                              \n"
    "    --cfg FILE : configuration to load                                         \n"
    "                                                                               \n"
    "    --f FILE   : configuration to load                                         \n";

/* display usage */
static void nr5g_fapi_usage(
    const char *prgname)
{
    printf(usage, prgname);
}

const char *nr5g_fapi_parse_args(
    int argc,
    char **argv)
{
    int opt;
    int option_index;
    const char *optname;
    char *cfg_file = NULL;
    char *prgname = argv[0];

    static struct option lgopts[] = {
        {"cfg", 1, 0, 0},
        {NULL, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "f", lgopts, &option_index)) != EOF) {
        switch (opt) {
            case 'f':
                printf("short opts");
                cfg_file = optarg;
                break;

                /* long options */
            case 0:
                printf("long opts");
                optname = lgopts[option_index].name;
                if (0 == strcmp(optname, "cfg")) {
                    cfg_file = optarg;
                }
                break;

            default:
                nr5g_fapi_usage(prgname);
                printf("in default");
                return NULL;
        }
    }

    if (cfg_file)
        printf("config file: %s\n", cfg_file);
    else
        nr5g_fapi_usage(prgname);

    return cfg_file;
}
