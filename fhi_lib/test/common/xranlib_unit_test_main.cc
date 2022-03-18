/*******************************************************************************
 *
 * Copyright (c) 2020 Intel.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 *
 *******************************************************************************/
#include <rte_config.h>
#include <rte_eal.h>
#include <rte_debug.h>

#include "common.hpp"
#include "xran_lib_wrap.hpp"

static int parse_input_parameter(std::string executable, std::string option)
{
    std::size_t delim_pos = option.find("=");
    std::string param = option.substr(delim_pos + 1);

    try
    {
        return std::stoi(param);
    }
    catch(std::logic_error &e)
    {
        std::cout << executable << ": invalid argument '"<< param << "' for '" << option  << "'" << std::endl;
        std::cout << "Try '" << executable << " --usage' for more information." << std::endl;
        exit(-1);
    }
}

xranLibWraper *xranlib;

int main(int argc, char** argv) {
    int all_test_ret = 0;

    /* Enable xml output by default */
    ::testing::GTEST_FLAG(output) = "xml:test_results.xml";

    ::testing::InitGoogleTest(&argc, argv);

    const std::string executable = argv[0];

    for(int index = 1; index < argc; index++) {

        const std::string option = argv[index];

        if (option.find("--nb_repetitions=") != std::string::npos)
        {
            BenchmarkParameters::repetition = parse_input_parameter(executable, option);
        }
        else if (option.find("--nb_loops=") != std::string::npos)
        {
            BenchmarkParameters::loop = parse_input_parameter(executable, option);
        }
        else if (option.find("--cpu_nb=") != std::string::npos)
        {
            BenchmarkParameters::cpu_id = (unsigned) parse_input_parameter(executable, option);

            if (BenchmarkParameters::cpu_id == 0)
                std::cout << std::endl << "Warning: Core #0 is running the VM's OS. "
                          << "Measurements won't be accurate. It shouldn't be used!"
                          << std::endl << std::endl;
        }
        /* --usage used instead of --help to avoid conflict with gtest --help */
        else if (!option.compare("--usage"))
        {
            std::cout << "Usage: " << executable << " [GTEST_OPTION]... [OPTION]..." << std::endl;
            std::cout << "Runs unittests with given gtest options." << std::endl;
            std::cout << std::endl;
            std::cout << "Available options: " << std::endl;
            std::cout << "--nb_repetitions=NUMBER Sets how many times results are measured" << std::endl;
            std::cout << "--nb_loops=NUMBER Sets how many times function is called per repetition"
                      << std::endl;
            std::cout << "--cpu_nb=NUMBER Sets core number to run tests on" << std::endl;
            std::cout << "--usage Prints this message" << std::endl;

            return 0;
        }
        else
        {
            std::cout << executable << ": inavlid option " << option << std::endl;
            std::cout << "Try '" << executable << " --usage' for more information." << std::endl;

            return -1;
        }
    }

    xranlib = new xranLibWraper;

    if(xranlib->SetUp() < 0) {
        return (-1);
        }

    all_test_ret = RUN_ALL_TESTS();

    xranlib->TearDown();

    if(xranlib != nullptr) {
        delete xranlib;
        xranlib == nullptr;
        }

    return all_test_ret;
}



