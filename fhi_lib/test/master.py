#!/usr/bin/python
#******************************************************************************
#
#   Copyright (c) 2020 Intel.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#******************************************************************************/

"""This script runs test cases with O-DU and O-RU
"""
import logging
import sys
import argparse
import re
import signal
import subprocess
import os
import shutil
import copy
from itertools import dropwhile
from datetime import datetime
from time import gmtime, strftime
import json
from threading import Timer
import socket

timeout_sec = 60*3 #3 min max

nLteNumRbsPerSymF1 = [
    #  5MHz    10MHz   15MHz   20 MHz
        [25,    50,     75,     100]  # LTE Numerology 0 (15KHz)
]

nNumRbsPerSymF1 = [
    #  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
        [25,    52,     79,     106,    133,    160,    216,    270,    0,         0,      0,      0,      0],         # Numerology 0 (15KHz)
        [11,    24,     38,     51,     65,     78,     106,    133,    162,       0,    217,    245,    273],         # Numerology 1 (30KHz)
        [0,     11,     18,     24,     31,     38,     51,     65,     79,        0,    107,    121,    135]          # Numerology 2 (60KHz)
]

nNumRbsPerSymF2 = [
    # 50Mhz  100MHz  200MHz   400MHz
    [66,    132,    264,     0],       # Numerology 2 (60KHz)
    [32,    66,     132,     264]      # Numerology 3 (120KHz)
]


nRChBwOptions_keys = ['5','10','15','20', '25', '30', '40', '50', '60','70', '80', '90', '100', '200', '400']
nRChBwOptions_values = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14]
nRChBwOptions = dict(zip(nRChBwOptions_keys, nRChBwOptions_values))

nRChBwOptions_keys_mu2and3 = ['50', '100', '200', '400']
nRChBwOptions_values_mu2and3 = [0,1,2,3]
nRChBwOptions_mu2and3 = dict(zip(nRChBwOptions_keys_mu2and3, nRChBwOptions_values_mu2and3))

vf_addr_o_xu=[]

# values for Jenkins server
vf_addr_o_xu_jenkins = [
    #vf_addr_o_xu_a                     vf_addr_o_xu_b                vf_addr_o_xu_c
    ["0000:19:02.0,0000:19:0a.0", "0000:19:02.1,0000:19:0a.1", "0000:19:02.2,0000:19:0a.2" ], #O-DU
    ["0000:af:02.0,0000:af:0a.0", "0000:af:02.1,0000:af:0a.1", "0000:af:02.2,0000:af:0a.2" ], #O-RU
]

vf_addr_o_xu_sc12 = [ # 2x2x25G with loopback FVL0:port0 to FVL1:port 0 FVL0:port1 to FVL1:port 1
    #vf_addr_o_xu_a                     vf_addr_o_xu_b                vf_addr_o_xu_c
    ["0000:88:02.0,0000:88:0a.0", "0000:88:02.1,0000:88:0a.1", "0000:88:02.2,0000:88:0a.2" ], #O-DU
    ["0000:86:02.0,0000:86:0a.0", "0000:86:02.1,0000:86:0a.1", "0000:86:02.2,0000:86:0a.2" ], #O-RU
]

vf_addr_o_xu_sc12_cvl = [
    #vf_addr_o_xu_a                     vf_addr_o_xu_b                vf_addr_o_xu_c
    ["0000:af:01.0,0000:af:09.0", "0000:af:11.0,0000:af:19.0", "0000:22:01.0,0000:22:09.0" ], #O-DU
    ["0000:af:01.0,0000:af:09.0", "0000:af:11.0,0000:af:19.0", "0000:1a:01.0,0000:1a:09.0" ], #O-RU
]

vf_addr_o_xu_scs1_30 = [
    ["0000:65:01.0,0000:65:01.1,0000:65:01.2,0000:65:01.3", "0000:65:01.4,0000:65:01.5,0000:65:01.6,0000:65:01.7", "0000:65:02.0,0000:65:02.1,0000:65:02.2,0000:65:02.3" ], #O-DU
    ["0000:65:09.0,0000:65:09.1,0000:65:09.2,0000:65:09.3", "0000:65:09.4,0000:65:09.5,0000:65:09.6,0000:65:09.7", "0000:65:0a.0,0000:65:0a.1,0000:65:0a.2,0000:65:0a.3" ], #O-RU
]

vf_addr_o_xu_scs1_repo = [
    ["0000:18:01.0,0000:18:01.1,0000:18:01.2,0000:18:01.3", "0000:18:01.4,0000:18:01.5,0000:18:01.6,0000:18:01.7", "0000:18:02.0,0000:18:02.1,0000:18:02.2,0000:18:02.3" ], #O-DU
    ["0000:18:11.0,0000:18:11.1,0000:18:11.2,0000:18:11.3", "0000:18:11.4,0000:18:11.5,0000:18:11.6,0000:18:11.7", "0000:18:12.0,0000:18:12.1,0000:18:12.2,0000:18:12.3" ], #O-RU
]

vf_addr_o_xu_icelake_scs1_1 = [
        #vf_addr_o_xu_a                                     vf_addr_o_xu_b                                             vf_addr_o_xu_c
    ["0000:51:01.0,0000:51:09.0", "0000:51:11.0,0000:51:19.0", "0000:18:01.0,0000:18:09.0" ], #O-DU
    ["0000:17:01.0,0000:17:09.0", "0000:17:11.0,0000:17:19.0", "0000:65:01.0,0000:65:09.0" ], #O-RU
]

vf_addr_o_xu_icx_npg_scs1_coyote4 = [
        #vf_addr_o_xu_a                                     vf_addr_o_xu_b                                             vf_addr_o_xu_c
    ["0000:51:01.0,0000:51:09.0", "0000:51:11.0,0000:51:19.0", "0000:54:01.0,0000:54:11.0" ], #O-DU
    ["0000:17:01.0,0000:17:09.0", "0000:17:11.0,0000:17:19.0", "0000:65:01.0,0000:65:09.0" ], #O-RU
]

vf_addr_o_xu_scs1_35 = [
    ["0000:88:01.0,0000:88:01.1,0000:88:01.2,0000:88:01.3", "0000:88:01.4,0000:88:01.5,0000:88:01.6,0000:88:01.7", "0000:88:02.0,0000:88:02.1,0000:88:02.2,0000:88:02.3" ], #O-DU
    ["0000:88:11.0,0000:88:11.1,0000:88:11.2,0000:88:11.3", "0000:88:11.4,0000:88:11.5,0000:88:11.6,0000:88:11.7", "0000:88:12.0,0000:88:12.1,0000:88:12.2,0000:88:12.3" ], #O-RU
]

vf_addr_o_xu_csl_npg_scs1_33 = [
    #vf_addr_o_xu_a                     vf_addr_o_xu_b                vf_addr_o_xu_c
    ["0000:1a:01.0,0000:1a:01.1", "0000:1a:01.2,0000:1a:01.3", "0000:1a:01.4,0000:1a:01.5" ], #O-DU
    ["0000:1a:11.0,0000:1a:11.1", "0000:1a:11.2,0000:1a:11.3", "0000:1a:11.4,0000:1a:11.5" ], #O-RU
]

# table of all test cases
#                 (ran, cat, mu, bw, test case, "test case description")
#Cat A
NR_test_cases_A = [(0,  0,   0,  5,   0,  "NR_Sub6_Cat_A_5MHz_1_Cell_0"),
                   (0,  0,   0,  10,  0,  "NR_Sub6_Cat_A_10MHz_1_Cell_0"),
                   (0,  0,   0,  10,  12, "NR_Sub6_Cat_A_10MHz_12_Cell_12"),
                   (0,  0,   0,  20,  0,  "NR_Sub6_Cat_A_20MHz_1_Cell_0"),
                   (0,  0,   0,  20,  12, "NR_Sub6_Cat_A_20MHz_12_Cell_12"),
                   (0,  0,   0,  20,  20, "NR_Sub6_Cat_A_20MHz_1_Cell_owd_req_resp"),
                   (0,  0,   0,  20,  21, "NR_Sub6_Cat_A_20MHz_1_Cell_owd_rem_req"),
                   (0,  0,   0,  20,  22, "NR_Sub6_Cat_A_20MHz_1_Cell_owd_req_wfup"),
                   (0,  0,   0,  20,  23, "NR_Sub6_Cat_A_20MHz_1_Cell_owd_rem_req_wfup"),
                   (0,  0,   1,  100, 0,  "NR_Sub6_Cat_A_100MHz_1_Cell_0"),
                   (0,  0,   3,  100, 0,  "NR_mmWave_Cat_A_100MHz_1_Cell_0"),
                   (0,  0,   3,  100, 7,  "NR_mmWave_Cat_A_100MHz_1_Cell_0_sc"),
]

LTE_test_cases_A = [(1,  0,   0,  5,   0, "LTE_Cat_A_5Hz_1_Cell_0"),
                    (1,  0,   0,  10,  0, "LTE_Cat_A_10Hz_1_Cell_0"),
                    (1,  0,   0,  20,  0, "LTE_Cat_A_20Hz_1_Cell_0"),
]

#Cat B
NR_test_cases_B  =  [(0, 1,   1,  100, 0, "NR_Sub6_Cat_B_100MHz_1_Cell_0"),
                     (0, 1,   1,  100, 2, "NR_Sub6_Cat_B_100MHz_1_Cell_2"),
                     (0, 1,   1,  100, 1, "NR_Sub6_Cat_B_100MHz_1_Cell_1"),
                     (0, 1,   1,  100, 101, "NR_Sub6_Cat_B_100MHz_1_Cell_101"),
                     (0, 1,   1,  100, 102, "NR_Sub6_Cat_B_100MHz_1_Cell_102"),
                     (0, 1,   1,  100, 103, "NR_Sub6_Cat_B_100MHz_1_Cell_103"),
                     (0, 1,   1,  100, 104, "NR_Sub6_Cat_B_100MHz_1_Cell_104"),
                     (0, 1,   1,  100, 105, "NR_Sub6_Cat_B_100MHz_1_Cell_105"),
                     (0, 1,   1,  100, 106, "NR_Sub6_Cat_B_100MHz_1_Cell_106"),
                     (0, 1,   1,  100, 107, "NR_Sub6_Cat_B_100MHz_1_Cell_107"),
                     (0, 1,   1,  100, 108, "NR_Sub6_Cat_B_100MHz_1_Cell_108"),
                     (0, 1,   1,  100, 109, "NR_Sub6_Cat_B_100MHz_1_Cell_109"),
                     (0, 1,   1,  100, 201, "NR_Sub6_Cat_B_100MHz_1_Cell_201"),
                     (0, 1,   1,  100, 202, "NR_Sub6_Cat_B_100MHz_1_Cell_202"),
                     (0, 1,   1,  100, 203, "NR_Sub6_Cat_B_100MHz_1_Cell_203"),
                     (0, 1,   1,  100, 204, "NR_Sub6_Cat_B_100MHz_1_Cell_204"),
                     (0, 1,   1,  100, 205, "NR_Sub6_Cat_B_100MHz_1_Cell_205"),
                     (0, 1,   1,  100, 206, "NR_Sub6_Cat_B_100MHz_1_Cell_206"),
                     (0, 1,   1,  100, 211, "NR_Sub6_Cat_B_100MHz_1_Cell_211"),
                     (0, 1,   1,  100, 212, "NR_Sub6_Cat_B_100MHz_1_Cell_212"),
                     (0, 1,   1,  100, 213, "NR_Sub6_Cat_B_100MHz_1_Cell_213"),
                     (0, 1,   1,  100, 214, "NR_Sub6_Cat_B_100MHz_1_Cell_214"),
                     (0, 1,   1,  100, 215, "NR_Sub6_Cat_B_100MHz_1_Cell_215"),
                     (0, 1,   1,  100, 216, "NR_Sub6_Cat_B_100MHz_1_Cell_216"),
                     #(0, 1,   1,  100, 401, "NR_Sub6_Cat_B_100MHz_1_Cell_401") 25G not enough
]

LTE_test_cases_B = [(1,  1,   0,   5,  0, "LTE_Cat_B_5MHz_1_Cell_0"),
                    (1,  1,   0,  10,  0, "LTE_Cat_B_10MHz_1_Cell_0"),
                    (1,  1,   0,  20,  0, "LTE_Cat_B_20MHz_1_Cell_0"),
                    (1,  1,   0,  5,   1, "LTE_Cat_B_5Hz_1_Cell_0_sc"),
                    (1,  1,   0,  10,  1, "LTE_Cat_B_10Hz_1_Cell_0_sc"),
                    (1,  1,   0,  20,  1, "LTE_Cat_B_20Hz_1_Cell_0_sc"),

]

V_test_cases_B = [
                    (0,  1,   1,  100,  301, "NR_Sub6_Cat_B_100MHz_1_Cell_301"),
                    (0,  1,   1,  100,  302, "NR_Sub6_Cat_B_100MHz_1_Cell_302"),
                    (0,  1,   1,  100,  303, "NR_Sub6_Cat_B_100MHz_1_Cell_303"),
                    (0,  1,   1,  100,  304, "NR_Sub6_Cat_B_100MHz_1_Cell_304"),
                    (0,  1,   1,  100,  305, "NR_Sub6_Cat_B_100MHz_1_Cell_305"),
                    (0,  1,   1,  100,  306, "NR_Sub6_Cat_B_100MHz_1_Cell_306"),
                    (0,  1,   1,  100,  602, "NR_Sub6_Cat_B_100MHz_1_Cell_602_sc"),
]

V_test_cases_B_2xUL = [
                    (0,  1,   1,  100,  311, "NR_Sub6_Cat_B_100MHz_1_Cell_311"),
                    (0,  1,   1,  100,  312, "NR_Sub6_Cat_B_100MHz_1_Cell_312"),
                    (0,  1,   1,  100,  313, "NR_Sub6_Cat_B_100MHz_1_Cell_313"),
                    (0,  1,   1,  100,  314, "NR_Sub6_Cat_B_100MHz_1_Cell_314"),
                    (0,  1,   1,  100,  315, "NR_Sub6_Cat_B_100MHz_1_Cell_315"),
                    (0,  1,   1,  100,  316, "NR_Sub6_Cat_B_100MHz_1_Cell_316"),
                    (0,  1,   1,  100,  612, "NR_Sub6_Cat_B_100MHz_1_Cell_612_sc"),

]

V_test_cases_B_mtu_1500 = [
                    (0,  1,   1,  100,  501, "NR_Sub6_Cat_B_100MHz_1_Cell_501"),
                    (0,  1,   1,  100,  502, "NR_Sub6_Cat_B_100MHz_1_Cell_502"),
                    (0,  1,   1,  100,  503, "NR_Sub6_Cat_B_100MHz_1_Cell_503"),
                    (0,  1,   1,  100,  504, "NR_Sub6_Cat_B_100MHz_1_Cell_504"),
                    (0,  1,   1,  100,  505, "NR_Sub6_Cat_B_100MHz_1_Cell_505"),
                    (0,  1,   1,  100,  506, "NR_Sub6_Cat_B_100MHz_1_Cell_506"),
                    (0,  1,   1,  100,  802, "NR_Sub6_Cat_B_100MHz_1_Cell_802_sc"),
]

V_test_cases_B_mtu_1500_2xUL = [
                    (0,  1,   1,  100,  511, "NR_Sub6_Cat_B_100MHz_1_Cell_511"),
                    (0,  1,   1,  100,  512, "NR_Sub6_Cat_B_100MHz_1_Cell_512"),
                    (0,  1,   1,  100,  513, "NR_Sub6_Cat_B_100MHz_1_Cell_513"),
                    (0,  1,   1,  100,  514, "NR_Sub6_Cat_B_100MHz_1_Cell_514"),
                    (0,  1,   1,  100,  515, "NR_Sub6_Cat_B_100MHz_1_Cell_515"),
                    (0,  1,   1,  100,  516, "NR_Sub6_Cat_B_100MHz_1_Cell_516"),
                    (0,  1,   1,  100,  812, "NR_Sub6_Cat_B_100MHz_1_Cell_812_sc"),
]

V_test_cases_B_3Cells = [
                    (0,  1,   1,  100,  3301, "NR_Sub6_Cat_B_100MHz_1_Cell_3301"),
                    (0,  1,   1,  100,  3311, "NR_Sub6_Cat_B_100MHz_1_Cell_3311")
]

V_test_cases_B_3Cells_mtu_1500 = [
                    (0,  1,   1,  100,  3501, "NR_Sub6_Cat_B_100MHz_1_Cell_3501"),
                    (0,  1,   1,  100,  3511, "NR_Sub6_Cat_B_100MHz_1_Cell_3511")
]

all_test_cases = NR_test_cases_A + LTE_test_cases_A + LTE_test_cases_B + NR_test_cases_B + V_test_cases_B + V_test_cases_B_2xUL

dic_dir      = dict({0:'DL', 1:'UL'})
dic_xu       = dict({0:'o-du', 1:'o-ru'})
dic_ran_tech = dict({0:'5g_nr', 1:'lte'})

def init_logger(console_level, logfile_level):
    """Initializes console and logfile logger with given logging levels"""
    # File logger
    logging.basicConfig(filename="runtests.log",
                        filemode='w',
                        format="%(asctime)s: %(levelname)s: %(message)s",
                        level=logfile_level)
    # Console logger
    logger = logging.getLogger()
    handler = logging.StreamHandler()
    handler.setLevel(console_level)
    formatter = logging.Formatter("%(levelname)s: %(message)s")
    handler.setFormatter(formatter)
    logger.addHandler(handler)

def parse_args(args):
    """Configures parser and parses command line configuration"""
    # Parser configuration
    parser = argparse.ArgumentParser(description="Run test cases: category numerology bandwidth test_num")

    parser.add_argument("--rem_o_ru_host", type=str, default="", help="remot host to run O-RU", metavar="root@10.10.10.1", dest="rem_o_ru_host")
    parser.add_argument("--ran", type=int, default=0, help="Radio Access Tehcnology 0 (5G NR) or 1 (LTE)", metavar="ran", dest="rantech")
    parser.add_argument("--cat", type=int, default=0, help="Category: 0 (A) or 1 (B)", metavar="cat", dest="category")
    parser.add_argument("--mu", type=int, default=0, help="numerology [0,1,3]", metavar="num", dest="numerology")
    parser.add_argument("--bw",  type=int, default=20, help="bandwidth [5,10,20,100]", metavar="bw", dest="bandwidth")
    parser.add_argument("--testcase", type=int, default=0, help="test case number", metavar="testcase", dest="testcase")
    parser.add_argument("--verbose", type=int, default=0, help="enable verbose output", metavar="verbose", dest="verbose")


    # Parse arguments
    options = parser.parse_args(args)
    #parser.print_help()
    logging.info("Options: rem_o_ru_host=%s ran=%d category=%d num=%d bw=%d testcase=%d",
                  options.rem_o_ru_host, options.rantech, options.category, options.numerology, options.bandwidth, options.testcase)
    return options

def is_comment(s):
    """ function to check if a line
         starts with some character.
         Here # for comment
    """
    # return true if a line starts with #
    return s.startswith('#')

class GetOutOfLoops( Exception ):
    pass

def get_re_map(nRB, direction):
    prb_map        = []
    PrbElemContent = []
    if direction == 0:
        #DL
        if 'nPrbElemDl' in globals():
            nPrbElm = nPrbElemDl
            for i in range(0, nPrbElm):
                elm = str('PrbElemDl'+str(i))
                #print(elm)
                if elm in globals():
                    PrbElemContent.insert(i,list(globals()[elm]))
                    xRBStart = PrbElemContent[i][0]
                    xRBSize  = PrbElemContent[i][1]
                    #print(PrbElemContent,"RBStart: ", xRBStart, "RBSize: ",xRBSize, list(range(xRBStart, xRBStart + xRBSize)))
                    prb_map = prb_map + list(range(xRBStart*12, xRBStart*12 + xRBSize*12))
        else:
            nPrbElm = 0;

    elif direction == 1:
        #UL
        if 'nPrbElemUl' in globals():
            nPrbElm = nPrbElemUl
            for i in range(0, nPrbElm):
                elm = str('PrbElemUl'+str(i))
                #print(elm)
                if (elm in globals()):
                    PrbElemContent.insert(i,list(globals()[elm]))
                    xRBStart = PrbElemContent[i][0]
                    xRBSize  = PrbElemContent[i][1]
                    #print(PrbElemContent,"RBStart: ", xRBStart, "RBSize: ",xRBSize, list(range(xRBStart, xRBStart + xRBSize)))
                    prb_map = prb_map + list(range(xRBStart*12, xRBStart*12 + xRBSize*12))
        else:
            nPrbElm = 0

    elif direction == 2:
        #UL
        if 'nPrbElemSrs' in globals():
            nPrbElm = nPrbElemUl
            for i in range(0, nPrbElm):
                elm = str('PrbElemSrs'+str(i))
                #print(elm)
                if (elm in globals()):
                    PrbElemContent.insert(i,list(globals()[elm]))
                    xRBStart = PrbElemContent[i][0]
                    xRBSize  = PrbElemContent[i][1]
                    #print(PrbElemContent,"RBStart: ", xRBStart, "RBSize: ",xRBSize, list(range(xRBStart, xRBStart + xRBSize)))
                    prb_map = prb_map + list(range(xRBStart*12, xRBStart*12 + xRBSize*12))
        else:
            nPrbElm = 0

    if nPrbElm == 0 :
        prb_map = list(range(0, nRB*12))

    return prb_map

def check_for_string_present_in_file(file_name, search_string):
    res = 1
    with open(file_name, 'r') as read_obj:
        for line in read_obj:
             if search_string in line:
                 read_obj.close()
                 res = 0
                 return res
    read_obj.close()
    return res

def check_owdm_test_results(xran_path, o_xu_id):
    res = 0
    file_owd_oru = xran_path+"/app/logs/"+"o-ru"+str(o_xu_id)+"-owd_results.txt"
    file_owd_odu = xran_path+"/app/logs/"+"o-du"+str(o_xu_id)+"-owd_results.txt"
    print("file_owd_oru :", file_owd_oru)
    print("file_owd_odu :", file_owd_odu)
    res = check_for_string_present_in_file(file_owd_oru, 'passed')
    res = res or check_for_string_present_in_file(file_owd_odu, 'passed')
    
    return res

def compare_results(o_xu_id, rantech, cat, mu, bw, tcase, xran_path, test_cfg, direction):
    res = 0
    re_map = []
    if rantech==1:
        if mu == 0:
            nDlRB = nLteNumRbsPerSymF1[mu][nRChBwOptions.get(str(nDLBandwidth))]
            nUlRB = nLteNumRbsPerSymF1[mu][nRChBwOptions.get(str(nULBandwidth))]
        else:
            print("Incorrect arguments\n")
            res = -1
            return res
    elif rantech==0:
        if mu < 3:
            nDlRB = nNumRbsPerSymF1[mu][nRChBwOptions.get(str(nDLBandwidth))]
            nUlRB = nNumRbsPerSymF1[mu][nRChBwOptions.get(str(nULBandwidth))]
        elif (mu >=2) & (mu <= 3):
            nDlRB = nNumRbsPerSymF2[mu - 2][nRChBwOptions_mu2and3.get(str(nDLBandwidth))]
            nUlRB = nNumRbsPerSymF2[mu - 2][nRChBwOptions_mu2and3.get(str(nULBandwidth))]
            print(nDlRB, nUlRB)
        else:
            print("Incorrect arguments\n")
            res = -1
            return res

    if 'compression' in globals():
        comp = compression
    else:
        comp = 0

    if 'srsEanble' in globals():
        srs_enb = srsEanble
    else:
        srs_enb = 0

    if 'rachEanble' in globals():
        rach = rachEanble
    else:
        rach = 0

    print("O-RU {} compare results: {} [compression {}]\n".format(o_xu_id, dic_dir.get(direction), comp))

    #if cat == 1:
    #    print("WARNING: Skip checking IQs and BF Weights for CAT B for now\n");
    #    return res

    #get slot config
    if nFrameDuplexType == 1:
        SlotConfig = []
        for i in range(nTddPeriod):
            if i == 0:
                SlotConfig.insert(i, sSlotConfig0)
            elif i == 1:
                SlotConfig.insert(i, sSlotConfig1)
            elif i == 2:
                SlotConfig.insert(i, sSlotConfig2)
            elif i == 3:
                SlotConfig.insert(i, sSlotConfig3)
            elif i == 4:
                SlotConfig.insert(i, sSlotConfig4)
            elif i == 5:
                SlotConfig.insert(i, sSlotConfig5)
            elif i == 6:
                SlotConfig.insert(i, sSlotConfig6)
            elif i == 7:
                SlotConfig.insert(i, sSlotConfig7)
            elif i == 8:
                SlotConfig.insert(i, sSlotConfig8)
            elif i == 9:
                SlotConfig.insert(i, sSlotConfig9)
            else :
                raise Exception('i should not exceed nTddPeriod %d. The value of i was: {}'.format(nTddPeriod, i))
        #print(SlotConfig, type(sSlotConfig0))

    try:

        if (direction == 1) & (cat == 1): #UL
            flowId = ccNum*antNumUL
        else:
            flowId = ccNum*antNum

        if direction == 0:
            re_map = get_re_map(nDlRB, direction)
        elif direction == 1:
            re_map = get_re_map(nUlRB, direction)
        else:
            raise Exception('Direction is not supported %d'.format(direction))

        for i in range(0, flowId):
            #read ref and test files
            tst = []
            ref = []
            if direction == 0:
                # DL
                nRB = nDlRB
                file_tst = xran_path+"/app/logs/"+"o-ru"+str(o_xu_id)+"-rx_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-du"+str(o_xu_id)+"-play_ant"+str(i)+".txt"
            elif direction == 1:
                # UL
                nRB = nUlRB
                file_tst = xran_path+"/app/logs/"+"o-du"+str(o_xu_id)+"-rx_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-ru"+str(o_xu_id)+"-play_ant"+str(i)+".txt"
            else:
                raise Exception('Direction is not supported %d'.format(direction))

            print("test result   :", file_tst)
            print("test reference:", file_ref)
            if os.path.exists(file_tst):
                try:
                    file_tst = open(file_tst, 'r')
                except OSError:
                    print ("Could not open/read file:", file_tst)
                    sys.exit()
            else:
                print(file_tst, "doesn't exist")
                res = -1
                return res
            if os.path.exists(file_ref):
                try:
                    file_ref = open(file_ref, 'r')
                except OSError:
                    print ("Could not open/read file:", file_ref)
                    sys.exit()
            else:
                print(file_tst, "doesn't exist")
                res = -1
                return res

            tst = file_tst.readlines()
            ref = file_ref.readlines()

            print(len(tst))
            print(len(ref))

            file_tst.close();
            file_ref.close();

            print(numSlots)

            #skip last slot for UL as we stop on PPS boundary (OTA) and all symbols might not be received by O-DU
            for slot_idx in range(0, numSlots - (1*direction)):
                for sym_idx in range(0, 14):
                    if nFrameDuplexType==1:
                        #skip sym if TDD
                        if direction == 0:
                            #DL
                            sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                            if(sym_dir != 0):
                                continue
                        elif direction == 1:
                            #UL
                            sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                            if(sym_dir != 1):
                                continue

                    #print("Check:","[",i,"]", slot_idx, sym_idx)
                    for line_idx in re_map:
                        offset = (slot_idx*nRB*12*14) + sym_idx*nRB*12 + line_idx
                        try:
                            line_tst = tst[offset].rstrip()
                        except IndexError:
                            res = -1
                            print("FAIL:","IndexError on tst: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(tst))
                            raise GetOutOfLoops
                        try:
                             line_ref = ref[offset].rstrip()
                        except IndexError:
                            res = -1
                            print("FAIL:","IndexError on ref: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(ref))
                            raise GetOutOfLoops

                        if comp == 1:
                            # discard LSB bits as BFP compression is not "bit exact"
                            tst_i_value = int(line_tst.split(" ")[0]) & 0xFF80
                            tst_q_value = int(line_tst.split(" ")[1]) & 0xFF80
                            ref_i_value = int(line_ref.split(" ")[0]) & 0xFF80
                            ref_q_value = int(line_ref.split(" ")[1]) & 0xFF80

                            #print("check:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                            if (tst_i_value != ref_i_value) or  (tst_q_value != ref_q_value) :
                                print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                                res = -1
                                raise GetOutOfLoops
                        else:
                            #if line_idx == 0:
                                #print("Check:", offset,"[",i,"]", slot_idx, sym_idx,":",line_tst, line_ref)
                            if line_ref != line_tst:
                                print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst:", line_tst, "ref:", line_ref)
                                res = -1
                                raise GetOutOfLoops
    except GetOutOfLoops:
        return res

    if (direction == 1) & (rach == 1) & 0: #UL
        print("O-RU {} compare results: {} [compression {}]\n".format(o_xu_id, 'PRACH', comp))

        #rach
        try:
            if mu == 3: #FR2
                re_map = range(0, 144)
                nRB = 12
            elif nFrameDuplexType==0: #FR1 FDD
                if prachConfigIndex < 87:
                    re_map = range(0, 840)
                    nRB = 70
                else:
                    re_map = range(0, 144)
                    nRB = 12
            else: #FR1 TDD
                if prachConfigIndex < 67:
                    re_map = range(0, 144)
                    nRB = 12
                else:
                    re_map = range(0, 840)
                    nRB = 70
            if cat == 1:
                flowId = ccNum*antNumUL
            else:
                flowId = ccNum*antNum

            for i in range(0, flowId):
                #read ref and test files
                tst = []
                ref = []

                file_tst = xran_path+"/app/logs/"+"o-du"+str(o_xu_id)+"-prach_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-ru"+str(o_xu_id)+"-play_prach_ant"+str(i)+".txt"
                print("test result   :", file_tst)
                print("test reference:", file_ref)
                if os.path.exists(file_tst):
                    try:
                        file_tst = open(file_tst, 'r')
                    except OSError:
                        print ("Could not open/read file:", file_tst)
                        sys.exit()
                else:
                    print(file_tst, "doesn't exist")
                    res = -1
                    return res
                if os.path.exists(file_ref):
                    try:
                        file_ref = open(file_ref, 'r')
                    except OSError:
                        print ("Could not open/read file:", file_ref)
                        sys.exit()
                else:
                    print(file_tst, "doesn't exist")
                    res = -1
                    return res

                tst = file_tst.readlines()
                ref = file_ref.readlines()

                print(len(tst))
                print(len(ref))

                file_tst.close();
                file_ref.close();

                print(numSlots)

                #skip last slot for UL as we stop on PPS boundary (OTA) and all symbols might not be received by O-DU
                for slot_idx in range(0, numSlots - (1*direction)):
                for sym_idx in range(0, 14):
                    if nFrameDuplexType==1:
                        #skip sym if TDD
                        if direction == 0:
                            #DL
                            sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                            if(sym_dir != 0):
                                continue
                        elif direction == 1:
                            #UL
                            sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                            if(sym_dir != 1):
                                continue

                    #print("Check:","[",i,"]", slot_idx, sym_idx)
                    for line_idx in re_map:
                        offset = (slot_idx*nRB*12*14) + sym_idx*nRB*12 + line_idx
                        try:
                            line_tst = tst[offset].rstrip()
                        except IndexError:
                            res = -1
                            print("FAIL:","IndexError on tst: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(tst))
                            raise GetOutOfLoops
                        try:
                             line_ref = ref[offset].rstrip()
                        except IndexError:
                            res = -1
                            print("FAIL:","IndexError on ref: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(ref))
                            raise GetOutOfLoops

                        if comp == 1:
                            # discard LSB bits as BFP compression is not "bit exact"
                            tst_i_value = int(line_tst.split(" ")[0]) & 0xFF80
                            tst_q_value = int(line_tst.split(" ")[1]) & 0xFF80
                            ref_i_value = int(line_ref.split(" ")[0]) & 0xFF80
                            ref_q_value = int(line_ref.split(" ")[1]) & 0xFF80

                            #print("check:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                            if (tst_i_value != ref_i_value) or  (tst_q_value != ref_q_value) :
                                print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                                res = -1
                                raise GetOutOfLoops
                        else:
                            #if line_idx == 0:
                                #print("Check:", offset,"[",i,"]", slot_idx, sym_idx,":",line_tst, line_ref)
                            if line_ref != line_tst:
                                print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst:", line_tst, "ref:", line_ref)
                                res = -1
                                raise GetOutOfLoops
    except GetOutOfLoops:
        return res

    if (direction == 0) | (cat == 0) | (srs_enb == 0): #DL or Cat A
        #done
    return res

    print("O-RU {} compare results: {} [compression {}]\n".format(o_xu_id, 'SRS', comp))

    #srs
    symbMask    = srsSym
    re_map = get_re_map(nUlRB, 2)
    try:
        flowId = ccNum*antElmTRx
        for i in range(0, flowId):
            #read ref and test files
            tst = []
            ref = []

            if direction == 1:
                # UL
                nRB = nUlRB
                file_tst = xran_path+"/app/logs/"+"o-du"+str(o_xu_id)+"-srs_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-ru"+str(o_xu_id)+"-play_srs_ant"+str(i)+".txt"
            else:
                raise Exception('Direction is not supported %d'.format(direction))

            print("test result   :", file_tst)
            print("test reference:", file_ref)
            if os.path.exists(file_tst):
                try:
                    file_tst = open(file_tst, 'r')
                except OSError:
                    print ("Could not open/read file:", file_tst)
                    sys.exit()
            else:
                print(file_tst, "doesn't exist")
                res = -1
                return res
            if os.path.exists(file_ref):
                try:
                    file_ref = open(file_ref, 'r')
                except OSError:
                    print ("Could not open/read file:", file_ref)
                    sys.exit()
            else:
                print(file_tst, "doesn't exist")
                res = -1
                return res

            tst = file_tst.readlines()
            ref = file_ref.readlines()

            print(len(tst))
            print(len(ref))

            file_tst.close();
            file_ref.close();

            print(numSlots)

            for slot_idx in range(0, numSlots - (1*direction)):
                for sym_idx in range(0, 14):
                    if symbMask & (1 << sym_idx) and slot_idx%nTddPeriod == 3:
                        print("SRS check sym ", slot_idx,  sym_idx)
                        if nFrameDuplexType==1:
                            #skip sym if TDD
                            if direction == 0:
                                #DL
                                sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                                if(sym_dir != 0):
                                    continue
                            elif direction == 1:
                                #UL
                                sym_dir = SlotConfig[slot_idx%nTddPeriod][sym_idx]
                                # ignore if DL symbol for now
                                #if(sym_dir != 1):
                                #    continue

                        print("Check:","[",i,"]", slot_idx, sym_idx)
                        for line_idx in re_map:
                            offset = (slot_idx*nRB*12*14) + sym_idx*nRB*12 + line_idx
                            try:
                                line_tst = tst[offset].rstrip()
                            except IndexError:
                                res = -1
                                print("FAIL:","IndexError on tst: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(tst))
                                raise GetOutOfLoops
                            try:
                                line_ref = ref[offset].rstrip()
                            except IndexError:
                                res = -1
                                print("FAIL:","IndexError on ref: ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx, len(ref))
                                raise GetOutOfLoops

                            if comp == 1:
                                # discard LSB bits as BFP compression is not "bit exact"
                                tst_i_value = int(line_tst.split(" ")[0]) & 0xFF80
                                tst_q_value = int(line_tst.split(" ")[1]) & 0xFF80
                                ref_i_value = int(line_ref.split(" ")[0]) & 0xFF80
                                ref_q_value = int(line_ref.split(" ")[1]) & 0xFF80

                                #print("check:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                                if (tst_i_value != ref_i_value) or  (tst_q_value != ref_q_value) :
                                    print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
                                    res = -1
                                    raise GetOutOfLoops
                            else:
                                #if line_idx == 0:
                                    #print("Check:", offset,"[",i,"]", slot_idx, sym_idx,":",line_tst, line_ref)
                                if line_ref != line_tst:
                                    print("FAIL:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst:", line_tst, "ref:", line_ref)
                                    res = -1
                                    raise GetOutOfLoops
    except GetOutOfLoops:
        #don't threat SRS as error for now
        res = 0
        return res


    return res

def parse_usecase_cfg(rantech, cat, mu, bw, tcase, xran_path, usecase_cfg):
    #parse config files
    logging.info("parse config files %s\n", usecase_cfg[0])
    lineList = list()
    sep = '#'
    with open(usecase_cfg[0],'r') as fh:
        for curline in dropwhile(is_comment, fh):
            my_line = curline.rstrip().split(sep, 1)[0].strip()
            if my_line:
                lineList.append(my_line)

    global_env = {}
    local_env  = {}

    for line in lineList:
        exe_line = line.replace(":", ",0x")
        if exe_line.find("../") > 0 :
            exe_line = exe_line.replace('../', "'../")
            exe_line = exe_line+"'"
        elif exe_line.find("./") > 0 :
            exe_line = exe_line.replace('./', "'./")
            exe_line = exe_line+"'"

        code = compile(str(exe_line), '<string>', 'exec')
        exec (code, global_env, local_env)

    for k, v in local_env.items():
        globals()[k] = v
        print(k, v)

    print("Number of O-RU:", oXuNum)

    return local_env

def parse_dat_file(rantech, cat, mu, bw, tcase, xran_path, test_cfg):
    #parse config files
    logging.info("parse config files %s\n", test_cfg[0])
    lineList = list()

    sep = '#'
    with open(test_cfg[0],'r') as fh:
        for curline in dropwhile(is_comment, fh):
            my_line = curline.rstrip().split(sep, 1)[0].strip()
            if my_line:
                lineList.append(my_line)
    global_env = {}
    local_env = {}

    for line in lineList:
        exe_line = line.replace(":", ",0x")
        if exe_line.find("../") > 0 :
            exe_line = exe_line.replace('../', "'../")
            exe_line = exe_line+"'"
        elif exe_line.find("./") > 0 :
            exe_line = exe_line.replace('./', "'./")
            exe_line = exe_line+"'"

        code = compile(str(exe_line), '<string>', 'exec')
        exec (code, global_env, local_env)

    for k, v in local_env.items():
        globals()[k] = v
        print(k, v)

    return local_env

def del_dat_file_vars(local_env):

    for k, v in local_env.items():
        del globals()[k]

    return 0

def make_copy_mlog(rantech, cat, mu, bw, tcase, xran_path):
    res = 0

    src_bin = xran_path+"/app/mlog-o-du.bin"
    src_csv = xran_path+"/app/mlog-o-du_hist.csv"
    dst_bin = xran_path+"/app/mlog-o-du-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".bin"
    dst_csv = xran_path+"/app/mlog-o-du_hist-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".csv"

    try:
        d_bin  = shutil.copyfile(src_bin, dst_bin)
        d_csv  = shutil.copyfile(src_csv, dst_csv)
    except IOError:
        logging.info("O-DU MLog is not present\n")
        res = 1
        return res
    else:
        logging.info("O-DU Mlog was copied\n")


    print("Destination path:", d_bin)
    print("Destination path:", d_csv)

    d_bin  = shutil.copyfile(src_bin, dst_bin)
    d_csv  = shutil.copyfile(src_csv, dst_csv)

    src_bin = xran_path+"/app/mlog-o-ru.bin"
    src_csv = xran_path+"/app/mlog-o-ru_hist.csv"
    dst_bin = xran_path+"/app/mlog-o-ru-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".bin"
    dst_csv = xran_path+"/app/mlog-o-ru_hist-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".csv"

    d_bin  = shutil.copyfile(src_bin, dst_bin)
    d_csv  = shutil.copyfile(src_csv, dst_csv)

    try:
        d_bin  = shutil.copyfile(src_bin, dst_bin)
        d_csv  = shutil.copyfile(src_csv, dst_csv)
    except IOError:
        logging.info("O-RU MLog is not present\n")
        res = 1
        return res
    else:
        logging.info("O-RU Mlog was copied\n")

    return res


def run_tcase(rem_o_ru_host, rantech, cat, mu, bw, tcase, verbose, xran_path, vf_addr_o_xu):

    if rantech == 1: #LTE
        if cat == 1:
            test_config =xran_path+"/app/usecase/lte_b/mu{0:d}_{1:d}mhz".format(mu, bw)
        elif cat == 0 :
            test_config =xran_path+"/app/usecase/lte_a/mu{0:d}_{1:d}mhz".format(mu, bw)
        else:
            print("Incorrect cat arguments\n")
            return -1
    elif rantech == 0: #5G NR
        if cat == 1:
            test_config =xran_path+"/app/usecase/cat_b/mu{0:d}_{1:d}mhz".format(mu, bw)
        elif cat == 0 :
            test_config =xran_path+"/app/usecase/cat_a/mu{0:d}_{1:d}mhz".format(mu, bw)
        else:
            print("Incorrect cat argument\n")
            return -1
    else:
        print("Incorrect rantech argument\n")
        return -1

    if(tcase > 0) :
        test_config = test_config+"/"+str(tcase)

    app = xran_path+"/app/build/sample-app"

    logging.debug("run: %s %s", app, test_config)
    logging.debug("Started script: master.py, XRAN path %s", xran_path)

    test_cfg = []
    global oXuOwdmEnabled
    oXuOwdmEnabled = 0 #Default is owdm measurements are disabled
    test_cfg.append(test_config+"/usecase_du.cfg")
    test_cfg.append(test_config+"/usecase_ru.cfg")

    usecase_dirname = os.path.dirname(os.path.realpath(test_cfg[0]))
    print(usecase_dirname)

    wd = os.getcwd()
    os.chdir(xran_path+"/app/")

    processes     = []
    logfile_xu    = []
    log_file_name = []
    timer         = []

    os.system('pkill -9 "sample-app"')
    os.system('rm -rf ./logs')

    usecase_cfg = parse_usecase_cfg(rantech, cat, mu, bw, tcase, xran_path, test_cfg)
    REM_O_RU_HOST=rem_o_ru_host

    for i in range(2):

        log_file_name.append("sampleapp_log_{}_{}_cat_{}_mu{}_{}mhz_tst_{}.log".format(dic_ran_tech.get(rantech), dic_xu.get(i),cat, mu, bw, tcase))
        with open(log_file_name[i], "w") as f:
            run_cmd = [app, "--usecasefile", test_cfg[i], "--num_eth_vfs", "6", "--vf_addr_o_xu_a", vf_addr_o_xu[i][0], "--vf_addr_o_xu_b", vf_addr_o_xu[i][1],"--vf_addr_o_xu_c", vf_addr_o_xu[i][2]]
            #, stdout=f, stderr=f
            if (verbose==1):
                if i == 0 or REM_O_RU_HOST == "":
                p = subprocess.Popen(run_cmd)
            else:
                    CMD = ' '.join([str(elem) for elem in run_cmd])
                    ssh = ["ssh", "%s" % REM_O_RU_HOST, "cd " + xran_path + "/app"+"; hostname; pwd; pkill -9 sample-app; rm -rf ./logs;" + CMD]
                    print(ssh)
                    print("my_cmd: ", ' '.join([str(elem) for elem in ssh]))
                    p = subprocess.Popen(ssh, shell=False)
            else:
                if i == 0 or REM_O_RU_HOST == "":
                p = subprocess.Popen(run_cmd, stdout=f, stderr=f)
                else :
                    CMD = ' '.join([str(elem) for elem in run_cmd])
                    ssh = ["ssh", "%s" % REM_O_RU_HOST, "cd " + xran_path + "/app"+"; hostname; pwd; pkill -9 sample-app; rm -rf ./logs; " + CMD]
                    p = subprocess.Popen(ssh, shell=False, stdout=f, stderr=f)
                    #stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            t = Timer(timeout_sec, p.kill)
            t.start()
            timer.append(t)
            logfile_xu.insert(i, f)
        processes.append((p, logfile_xu[i]))

    logging.info("Running O-DU and O-RU see output in:\n    O-DU: %s\n    O-RU: %s\n", xran_path+"/app/"+logfile_xu[0].name, xran_path+"/app/"+logfile_xu[1].name)
    #while (gmtime().tm_sec % 30) <> 0:
        #pass
    print(strftime("%a, %d %b %Y %H:%M:%S +0000", gmtime()))
    i = 0
    for p, f in processes:
        try:
        p.communicate()[0]
        p.wait()
        except (KeyboardInterrupt, SystemExit):
            for i in range(2):
                timer[i].cancel();
                timer[i].cancel();
            for pp, ff in processes:
                pp.send_signal(signal.SIGINT)
                pp.wait()
            raise

        if p.returncode != 0:
            print("Application {} failed p.returncode:{}".format(dic_xu.get(i), p.returncode))
            print("FAIL")
            #logging.info("FAIL\n")
            #logging.shutdown()
            #sys.exit(p.returncode)
        i += 1
        f.close()

    for i in range(2):
        timer[i].cancel();
        timer[i].cancel();

    logging.info("O-DU and O-RU are done\n")

    if REM_O_RU_HOST:
        sys_cmd = "scp -r "+REM_O_RU_HOST+":"+ xran_path+"/app/logs/*.txt "+ xran_path+"/app/logs/"
        print(sys_cmd)
        os.system(sys_cmd)
        sys_cmd = "scp -r "+REM_O_RU_HOST+":"+ xran_path+"/app/mlog-o-ru* "+ xran_path+"/app/"
        print(sys_cmd)
        os.system(sys_cmd)

    make_copy_mlog(rantech, cat, mu, bw, tcase, xran_path)
    #oXuNum check only O-RU 0 for now
    if 'oXuOwdmEnabled==1' in globals():
         OwdmTest=1
    else:
         OwdmTest=0

    for o_xu_id in range(0, oXuNum):
        o_xu_test_cfg = []
        if o_xu_id == 0:
            o_xu_test_cfg.append(usecase_dirname+"/"+oXuCfgFile0)
        elif o_xu_id == 1:
            o_xu_test_cfg.append(usecase_dirname+"/"+oXuCfgFile1)
        elif o_xu_id == 2:
            o_xu_test_cfg.append(usecase_dirname+"/"+oXuCfgFile2)
        elif o_xu_id == 3:
            o_xu_test_cfg.append(usecase_dirname+"/"+oXuCfgFile3)

        logging.info("O-RU %d parse config files %s\n", o_xu_id, o_xu_test_cfg)

        usecase_cfg_per_o_ru = parse_dat_file(rantech, cat, mu, bw, tcase, xran_path, o_xu_test_cfg)

        res = compare_results(o_xu_id,rantech, cat, mu, bw, tcase, xran_path, o_xu_test_cfg, 0)
        if OwdmTest == 1:
        # overwrite PASS/FAIL in res if the owd tests have failed
             res1 = check_owdm_test_results(xran_path, o_xu_id)
             print("res1 :", res1)
             if res1 !=0 :
                  res = -1     
    if res != 0:
        os.chdir(wd)
        print("FAIL")
            del_dat_file_vars(usecase_cfg_per_o_ru)
        return res

        res = compare_results(o_xu_id, rantech, cat, mu, bw, tcase, xran_path, o_xu_test_cfg, 1)
    if res != 0:
        os.chdir(wd)
        print("FAIL")
            del_dat_file_vars(usecase_cfg_per_o_ru)
        return res

    os.chdir(wd)
    print("PASS")

        del_dat_file_vars(usecase_cfg_per_o_ru)

    return res

def main():
    test_results = []
    test_executed_total = 0
    run_total = 0
    test_fail_cnt = 0
    test_pass_cnt = 0
    cat   = 0
    mu    = 0
    bw    = 0
    tcase = 0
    tcase_description = "n/a"

    """Processes input files to produce IACA files"""
    # Find path to XRAN
    if os.getenv("XRAN_DIR") is not None:
        xran_path = os.getenv("XRAN_DIR")
    else:
        print("please set 'export XRAN_DIR' in the OS")
        return -1

    # Set up logging with given level (DEBUG, INFO, ERROR) for console end logfile
    init_logger(logging.INFO, logging.DEBUG)
    host_name =  socket.gethostname()
    logging.info("host: %s Started script: master.py from XRAN path %s",host_name, xran_path)

    options = parse_args(sys.argv[1:])
    rem_o_ru_host = options.rem_o_ru_host

    if host_name == "sc12-xran-sub6":
        if rem_o_ru_host:
            vf_addr_o_xu = vf_addr_o_xu_sc12_cvl
        else:
            vf_addr_o_xu = vf_addr_o_xu_sc12
    elif host_name == "csl-npg-scs1-30":
        vf_addr_o_xu = vf_addr_o_xu_scs1_30
    elif host_name == "npg-scs1-repo.la.intel.com":
        vf_addr_o_xu = vf_addr_o_xu_scs1_repo
    elif host_name == "icelake-scs1-1":
        vf_addr_o_xu = vf_addr_o_xu_icelake_scs1_1
    elif host_name == "icx-npg-scs1-coyote4":
        vf_addr_o_xu = vf_addr_o_xu_icx_npg_scs1_coyote4
    elif host_name == "csl-npg-scs1-35":
        vf_addr_o_xu = vf_addr_o_xu_scs1_35
    elif host_name == "csl-npg-scs1-33":
        vf_addr_o_xu = vf_addr_o_xu_csl_npg_scs1_33
    else:
        vf_addr_o_xu = vf_addr_o_xu_jenkins

    print(vf_addr_o_xu[0][0],vf_addr_o_xu[0][1],vf_addr_o_xu[0][2])
    print(vf_addr_o_xu[1][0],vf_addr_o_xu[1][1],vf_addr_o_xu[1][2])

    # Parse input arguments
    if len(sys.argv) == 1 or (len(sys.argv) == 3 and rem_o_ru_host):
        run_total = len(all_test_cases)
        print(run_total)
        print("Run All test cases {}\n".format(run_total))
    else:
        rantech = options.rantech
        cat     = options.category
        mu      = options.numerology
        bw      = options.bandwidth
        tcase   = options.testcase
        verbose = options.verbose

        print(rem_o_ru_host)

    if (run_total):
        for test_run_ix in range(0, run_total):
            rantech = all_test_cases[test_run_ix][0]
            cat     = all_test_cases[test_run_ix][1]
            mu      = all_test_cases[test_run_ix][2]
            bw      = all_test_cases[test_run_ix][3]
            tcase   = all_test_cases[test_run_ix][4]
            tcase_description = all_test_cases[test_run_ix][5]
            verbose = 0

            logging.info("Test# %d out of %d [PASS %d FAIL %d]: ran %d cat %d mu %d bw %d test case %d [%s]\n",test_run_ix, run_total, test_pass_cnt, test_fail_cnt, rantech, cat, mu, bw, tcase, tcase_description)
            res = run_tcase(rem_o_ru_host, rantech, cat, mu, bw, tcase, verbose,  xran_path, vf_addr_o_xu)
            if (res != 0):
                test_fail_cnt += 1
                test_results.append((rantech, cat, mu, bw, tcase,'FAIL', tcase_description))
                continue

            test_pass_cnt += 1
            test_results.append((rantech, cat, mu, bw, tcase,'PASS', tcase_description))
    else:
        res = run_tcase(rem_o_ru_host, rantech, cat, mu, bw, tcase, verbose, xran_path, vf_addr_o_xu)
        if (res != 0):
            test_results.append((rantech, cat, mu, bw, tcase,'FAIL'))
        else:
        test_results.append((rantech, cat, mu, bw, tcase,'PASS'))

        with open('testresult.txt', 'w') as reshandle:
            json.dump(test_results, reshandle)

    return res

if __name__ == '__main__':
    print("Python version")
    print (sys.version)
    print("Version info.")
    print (sys.version_info)
    if (sys.version_info[0] < 3):
        raise Exception ("Must be Python 3")
    START_TIME = datetime.now()
    res = main()
    END_TIME = datetime.now()
    logging.debug("Start time: %s, end time: %s", START_TIME, END_TIME)
    logging.info("Execution time: %s", END_TIME - START_TIME)
    logging.shutdown()
    sys.exit(res)
