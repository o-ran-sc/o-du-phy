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

#include <cmath>
#include <fstream>
#include <numeric>

#ifndef _WIN64
#include <unistd.h>
#include <sys/syscall.h>
#else
#include <Windows.h>
#endif

#include "common.hpp"

#ifndef CPU_ID
#define CPU_ID 4
#endif

/* Required to avoid linker errors */
json KernelTests::conf;
std::string KernelTests::test_type;
unsigned long KernelTests::tsc;


long BenchmarkParameters::repetition = 40;
long BenchmarkParameters::loop = 30;
unsigned BenchmarkParameters::cpu_id = CPU_ID;

int bind_to_cpu(const unsigned cpu)
{
#ifndef _WIN64
    const auto pid = syscall(SYS_gettid);
    cpu_set_t mask {};
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    return sched_setaffinity(__pid_t(pid), sizeof(mask), &mask);
#else
    return -1;
#endif
}

std::pair<double, double> calculate_statistics(const std::vector<long> values)
{
    const auto sum = std::accumulate(values.begin(), values.end(), 0L);

    const auto number_of_iterations = BenchmarkParameters::repetition *
            BenchmarkParameters::loop;

    const auto mean = sum / (double) number_of_iterations;

    auto stddev_accumulator = 0.0;
    for (auto v : values)
        stddev_accumulator = pow((v / BenchmarkParameters::loop) - mean, 2);

    const auto stddev = sqrt(stddev_accumulator / BenchmarkParameters::repetition);

    return {mean, stddev};
}

std::vector<unsigned> get_sequence(const unsigned number)
{
    std::vector<unsigned> sequence(number);
    std::iota(sequence.begin(), sequence.end(), 0);

    return sequence;
}

char* read_data_to_aligned_array(const std::string &filename)
{
    std::ifstream input_stream(filename, std::ios::binary);

    std::vector<char> buffer((std::istreambuf_iterator<char>(input_stream)),
                              std::istreambuf_iterator<char>());

    if(buffer.size() == 0)
        throw reading_input_file_exception();

    auto aligned_buffer = aligned_malloc<char>((int) buffer.size(), 64);

    if(aligned_buffer == nullptr)
        throw std::runtime_error("Failed to allocate memory for the test vector!");

    std::copy(buffer.begin(), buffer.end(), aligned_buffer);

    return aligned_buffer;
}

json read_json_from_file(const std::string &filename)
{
    json result;

    std::ifstream json_stream(filename);
    if(!json_stream.is_open())
        throw missing_config_file_exception();

    json_stream >> result;

    return result;
}

unsigned long tsc_recovery()
{
#ifndef _WIN64
    constexpr auto ns_per_sec = 1E9;

    struct timespec sleeptime = {.tv_nsec = __syscall_slong_t(5E8) };

    struct timespec t_start, t_end;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0)
    {
        unsigned long start = tsc_tick();

        nanosleep(&sleeptime,NULL);
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);

        unsigned long end = tsc_tick();

        unsigned long ns = (unsigned long)((t_end.tv_sec - t_start.tv_sec) * ns_per_sec + t_end.tv_nsec - t_start.tv_nsec);

        double secs = (double) ns / ns_per_sec;

        unsigned long resolution_timer = (unsigned long)((end - start)/secs);
        unsigned long tick_per_usec = (resolution_timer / 1000000);

        std::cout << "[----------] System clock (rdtsc) resolution " << resolution_timer << " [Hz]" << std::endl;
        std::cout << "[----------] Ticks per us " << tick_per_usec << std::endl;

        return tick_per_usec;
    }
#else

    LARGE_INTEGER tick_per_sec;
    QueryPerformanceFrequency(&tick_per_sec);

    std::cout << "[----------] System clock (rdtsc) resolution unknown" << std::endl;
    std::cout << "[----------] Ticks per us " << (tick_per_sec.QuadPart / 1000000) << std::endl;
    return (unsigned long) tick_per_sec.QuadPart;

#endif
    return 0;
}

unsigned long tsc_tick()
{
#ifndef _WIN64
    unsigned long hi, lo;

    __asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));

    return lo | (hi << 32);
#else
	return 0;
#endif
}

void KernelTests::print_and_store_results(const std::string &isa, const std::string &parameters,
                                          const std::string &module_name, const std::string &test_name,
                                          const std::string &unit, const int para_factor,
                                          const double mean, const double stddev)
{
    std::cout << "[----------] " << "Mean" << " = " << std::fixed << mean << " us" << std::endl;
    std::cout << "[----------] " << "Stddev" << " = " << stddev << " us" << std::endl;

#ifndef _WIN64
    /* Two properties below should uniquely identify a test case */
    RecordProperty("kernelname", module_name);
    RecordProperty("parameters", parameters);

    RecordProperty("isa", isa);
    RecordProperty("unit", unit);
    RecordProperty("parallelization_factor", para_factor);

    RecordProperty("mean", std::to_string(mean));
    RecordProperty("stddev", std::to_string(stddev));
#endif
}
