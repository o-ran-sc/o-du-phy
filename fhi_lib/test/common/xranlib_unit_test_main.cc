/*******************************************************************************
 *
 * <COPYRIGHT_TAG>
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

void
ut_version_print(void)
{
    char            sysversion[100];
    char           *compilation_date = (char *)__DATE__;
    char           *compilation_time = (char *)__TIME__;
    char            compiler[100];

    //snprintf(sysversion, 99, "Version: %s", VERSIONX);

#if defined(__clang__)
    snprintf(compiler, 99, "family clang: %s", __clang_version__);
#elif defined(__ICC) || defined(__INTEL_COMPILER)
    snprintf(compiler, 99, "family icc: version %d", __INTEL_COMPILER);
#elif defined(__INTEL_LLVM_COMPILER)
    snprintf(compiler, 99, "family icx: version %d", __INTEL_LLVM_COMPILER);
#elif defined(__GNUC__) || defined(__GNUG__)
    snprintf(compiler, 99, "family gcc: version %d.%d.%d", __GNUC__, __GNUC_MINOR__,__GNUC_PATCHLEVEL__);
#endif

    printf("\n\n");
    printf("===========================================================================================================\n");
    printf("UNITTESTS VERSION\n");
    printf("===========================================================================================================\n");

    //printf("%s\n", sysversion);
    printf("build-date: %s\n", compilation_date);
    printf("build-time: %s\n", compilation_time);
    printf("build-with: %s\n", compiler);
}

int main(int argc, char** argv) {
    int all_test_ret = 0;
    ut_version_print();
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
        xranlib = nullptr;
        }

    return all_test_ret;
}



