#!/usr/bin/python
#******************************************************************************
#
#   Copyright (c) 2019 Intel.
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
import subprocess
import os
import shutil
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

# values for Jenkins server
eth_cp_dev = ["0000:19:02.1", "0000:19:0a.1"]
eth_up_dev = ["0000:19:02.0", "0000:19:0a.0"]

# table of all test cases
#                 (ran, cat, mu, bw, test case)
#Cat A
NR_test_cases_A = [(0,  0,   0,  5,   0),
                   (0,  0,   0,  10,  0),
                   (0,  0,   0,  10,  12),
                   (0,  0,   0,  20,  0),
                   (0,  0,   0,  20,  12),
                   (0,  0,   1,  100, 0),
                   (0,  0,   3,  100, 0),
]

LTE_test_cases_A = [(1,  0,   0,  5,   0),
                    (1,  0,   0,  10,  0),
                    (1,  0,   0,  20,  0),
]

#Cat B
NR_test_cases_B  =  [(0, 1,   1,  100, 0),
                     (0, 1,   1,  100, 2),
                     (0, 1,   1,  100, 1),
                     (0, 1,   1,  100, 101),
                     (0, 1,   1,  100, 102),
                     (0, 1,   1,  100, 103),
                     (0, 1,   1,  100, 104),
                     (0, 1,   1,  100, 105),
                     #(0, 1,   1,  100, 106), 25G not enough
                     (0, 1,   1,  100, 107),
                     (0, 1,   1,  100, 108),
                     #(0, 1,   1,  100, 109), 25G not enough
                     (0, 1,   1,  100, 201),
                     #(0, 1,   1,  100, 202), 25G not enough
                     #(0, 1,   1,  100, 203),
                     (0, 1,   1,  100, 204),
                     (0, 1,   1,  100, 205),
                     (0, 1,   1,  100, 206),
                     (0, 1,   1,  100, 211),
                     #(0, 1,   1,  100, 212), 25G not enough
                     (0, 1,   1,  100, 213),
                     (0, 1,   1,  100, 214),
                     (0, 1,   1,  100, 215),
                     (0, 1,   1,  100, 216)
]

LTE_test_cases_B = [(1,  1,   0,   5,  0),
                    (1,  1,   0,  10,  0),
                    (1,  1,   0,  20,  0),
]

V_test_cases_B = [
                   # (0,  1,   1,  100,  301), 25G not enough
                    (0,  1,   1,  100,  302),
                    (0,  1,   1,  100,  303),
                    (0,  1,   1,  100,  304),
                    (0,  1,   1,  100,  305),
                    (0,  1,   1,  100,  306)
]

all_test_cases = NR_test_cases_A + LTE_test_cases_A + LTE_test_cases_B + NR_test_cases_B + V_test_cases_B

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

    parser.add_argument("--ran", type=int, default=0, help="Radio Access Tehcnology 0 (5G NR) or 1 (LTE)", metavar="ran", dest="rantech")
    parser.add_argument("--cat", type=int, default=0, help="Category: 0 (A) or 1 (B)", metavar="cat", dest="category")
    parser.add_argument("--mu", type=int, default=0, help="numerology [0,1,3]", metavar="num", dest="numerology")
    parser.add_argument("--bw",  type=int, default=20, help="bandwidth [5,10,20,100]", metavar="bw", dest="bandwidth")
    parser.add_argument("--testcase", type=int, default=0, help="test case number", metavar="testcase", dest="testcase")
    parser.add_argument("--verbose", type=int, default=0, help="enable verbose output", metavar="verbose", dest="verbose")

    # Parse arguments
    options = parser.parse_args(args)
    #parser.print_help()
    logging.debug("Options: ran=%d category=%d num=%d bw=%d testcase=%d",
                  options.rantech, options.category, options.numerology, options.bandwidth, options.testcase)
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
            nPrbElm = 0;

    if nPrbElm == 0 :
        prb_map = list(range(0, nRB*12))

    return prb_map

def compare_resuts(rantech, cat, mu, bw, tcase, xran_path, test_cfg, direction):
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

    print("compare results: {} [compression {}]\n".format(dic_dir.get(direction), comp))

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
                file_tst = xran_path+"/app/logs/"+"o-ru-rx_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-du-play_ant"+str(i)+".txt"
            elif direction == 1:
                # UL
                nRB = nUlRB
                file_tst = xran_path+"/app/logs/"+"o-du-rx_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-ru-play_ant"+str(i)+".txt"
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

            for slot_idx in range(0, numSlots):
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

    #if (direction == 0) | (cat == 0) | (srs_enb == 0): #DL or Cat A
        #done
    return res

    print("compare results: {} [compression {}]\n".format('SRS', comp))

    #srs
    symbMask    = srsSym
    try:
        flowId = ccNum*antElmTRx
        for i in range(0, flowId):
            #read ref and test files
            tst = []
            ref = []

            if direction == 1:
                # UL
                nRB = nUlRB
                file_tst = xran_path+"/app/logs/"+"o-du-srs_log_ant"+str(i)+".txt"
                file_ref = xran_path+"/app/logs/"+"o-ru-play_srs_ant"+str(i)+".txt"
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

            for slot_idx in range(0, numSlots):
                for sym_idx in range(0, 14):
                    if symbMask & (1 << sym_idx):
                        print("SRS check sym ", sym_idx)
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
                        for line_idx in range(0, nRB*12):
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
                            if False : #SRS sent as not compressed
                                #comp == 1:
                                # discard LSB bits as BFP compression is not Bit Exact
                                tst_i_value = int(line_tst.split(" ")[0]) & 0xFF80
                                tst_q_value = int(line_tst.split(" ")[1]) & 0xFF80
                                ref_i_value = int(line_ref.split(" ")[0]) & 0xFF80
                                ref_q_value = int(line_ref.split(" ")[1]) & 0xFF80

                                print("check:","ant:[",i,"]:",offset, slot_idx, sym_idx, line_idx,":","tst: ", tst_i_value, " ", tst_q_value, " " , "ref: ", ref_i_value, " ", ref_q_value, " ")
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
        pass


    return res

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
        exe_line = line.replace(":", ",")
        if exe_line.find("/") > 0 :
            exe_line = exe_line.replace('./', "'")
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

    src_bin = xran_path+"/app/mlog-o-du-c0.bin"
    src_csv = xran_path+"/app/mlog-o-du-hist.csv"
    dst_bin = xran_path+"/app/mlog-o-du-c0-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".bin"
    dst_csv = xran_path+"/app/mlog-o-du-hist-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".csv"

    try:
        d_bin  = shutil.copyfile(src_bin, dst_bin)
        d_csv  = shutil.copyfile(src_csv, dst_csv)
    except IOError:
        logging.info("MLog is not present\n")
        res = 1
        return res
    else:
        logging.info("Mlog was copied\n")


    print("Destination path:", d_bin)
    print("Destination path:", d_csv)

    d_bin  = shutil.copyfile(src_bin, dst_bin)
    d_csv  = shutil.copyfile(src_csv, dst_csv)

    src_bin = xran_path+"/app/mlog-o-ru-c0.bin"
    src_csv = xran_path+"/app/mlog-o-ru-hist.csv"
    dst_bin = xran_path+"/app/mlog-o-ru-c0-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".bin"
    dst_csv = xran_path+"/app/mlog-o-ru-hist-ran"+str(rantech)+"-cat"+str(cat)+"-mu"+str(mu)+"-bw"+str(bw)+"-tcase"+str(tcase)+".csv"

    d_bin  = shutil.copyfile(src_bin, dst_bin)
    d_csv  = shutil.copyfile(src_csv, dst_csv)

    try:
        d_bin  = shutil.copyfile(src_bin, dst_bin)
        d_csv  = shutil.copyfile(src_csv, dst_csv)
    except IOError:
        logging.info("MLog is not present\n")
        res = 1
        return res
    else:
        logging.info("Mlog was copied\n")

    return res


def run_tcase(rantech, cat, mu, bw, tcase, verbose, xran_path):

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
            test_config =xran_path+"/app/usecase/mu{0:d}_{1:d}mhz".format(mu, bw)
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

    test_cfg.append(test_config+"/config_file_o_du.dat")
    test_cfg.append(test_config+"/config_file_o_ru.dat")

    wd = os.getcwd()
    os.chdir(xran_path+"/app/")

    processes     = []
    logfile_xu    = []
    log_file_name = []
    timer         = []

    os.system('pkill -9 "sample-app"')
    os.system('rm -rf ./logs')

    for i in range(2):
        log_file_name.append("sampleapp_log_{}_{}_cat_{}_mu{}_{}mhz_tst_{}.log".format(dic_ran_tech.get(rantech), dic_xu.get(i),cat, mu, bw, tcase))
        with open(log_file_name[i], "w") as f:
            run_cmd = [app, "-c", test_cfg[i], "-p", "2", eth_up_dev[i], eth_cp_dev[i]]
            #, stdout=f, stderr=f
            if (verbose==1):
                p = subprocess.Popen(run_cmd)
            else:
                p = subprocess.Popen(run_cmd, stdout=f, stderr=f)
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
        p.communicate()[0]
        p.wait()
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

    make_copy_mlog(rantech, cat, mu, bw, tcase, xran_path)

    usecase_cfg = parse_dat_file(rantech, cat, mu, bw, tcase, xran_path, test_cfg)

    res = compare_resuts(rantech, cat, mu, bw, tcase, xran_path, test_cfg, 0)
    if res != 0:
        os.chdir(wd)
        print("FAIL")
        return res

    res = compare_resuts(rantech, cat, mu, bw, tcase, xran_path, test_cfg, 1)
    if res != 0:
        os.chdir(wd)
        print("FAIL")
        return res

    os.chdir(wd)
    print("PASS")

    del_dat_file_vars(usecase_cfg)

    return res

def main():
    test_results = []
    test_executed_total = 0
    run_total = 0
    cat   = 0
    mu    = 0
    bw    = 0
    tcase = 0
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

    #custom config for dev station
    if host_name == "sc12-xran-sub6":
        eth_cp_dev[0] = "0000:21:02.1"
        eth_cp_dev[1] = "0000:21:0a.1"
        eth_up_dev[0] = "0000:21:02.0"
        eth_up_dev[1] = "0000:21:0a.0"

    # Parse input arguments
    if len(sys.argv) == 1 :
        run_total = len(all_test_cases)
        print(run_total)
        print("Run All test cases {}\n".format(run_total))
    else:
        options = parse_args(sys.argv[1:])
        rantech = options.rantech
        cat     = options.category
        mu      = options.numerology
        bw      = options.bandwidth
        tcase   = options.testcase
        verbose = options.verbose


    if (run_total):
        for test_run_ix in range(0, run_total):
            rantech = all_test_cases[test_run_ix][0]
            cat     = all_test_cases[test_run_ix][1]
            mu      = all_test_cases[test_run_ix][2]
            bw      = all_test_cases[test_run_ix][3]
            tcase   = all_test_cases[test_run_ix][4]
            verbose = 0

            logging.info("Test# %d out of %d: ran %d cat %d mu %d bw %d test case %d\n",test_run_ix, run_total, rantech, cat, mu, bw, tcase)
            res = run_tcase(rantech, cat, mu, bw, tcase, verbose,  xran_path)
            if (res != 0):
                test_results.append((rantech, cat, mu, bw, tcase,'FAIL'))
                continue

            test_results.append((rantech, cat, mu, bw, tcase,'PASS'))

            with open('testresult.txt', 'w') as reshandle:
                json.dump(test_results, reshandle)
    else:
        res = run_tcase(rantech, cat, mu, bw, tcase, verbose, xran_path)
        if (res != 0):
            test_results.append((rantech, cat, mu, bw, tcase,'FAIL'))
        test_results.append((rantech, cat, mu, bw, tcase,'PASS'))

        with open('testresult.txt', 'w') as reshandle:
            json.dump(test_results, reshandle)

    return res

if __name__ == '__main__':
    START_TIME = datetime.now()
    res = main()
    END_TIME = datetime.now()
    logging.debug("Start time: %s, end time: %s", START_TIME, END_TIME)
    logging.info("Execution time: %s", END_TIME - START_TIME)
    logging.shutdown()
    sys.exit(res)
