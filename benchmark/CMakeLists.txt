cmake_minimum_required(VERSION 3.16)
project(logic_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenMP REQUIRED)

option(BENCHMARK_DEBUG "Keep debug-friendly benchmark behavior" ON)
option(BENCHMARK_FORCE_ISA "Build AVX targets even if host CPU flags are missing" OFF)

set(HOST_CPU_FLAGS "")
if(EXISTS "/proc/cpuinfo")
    file(STRINGS "/proc/cpuinfo" CPUINFO_FLAGS_LINE REGEX "^[Ff]lags[ 	]*:")
    if(CPUINFO_FLAGS_LINE)
        list(GET CPUINFO_FLAGS_LINE 0 CPUINFO_FLAGS_FIRST)
        string(REGEX REPLACE "^[^:]*:[ 	]*" "" HOST_CPU_FLAGS "${CPUINFO_FLAGS_FIRST}")
        string(TOLOWER "${HOST_CPU_FLAGS}" HOST_CPU_FLAGS)
    endif()
endif()

if(HOST_CPU_FLAGS STREQUAL "")
    execute_process(
        COMMAND lscpu
        RESULT_VARIABLE LSCPU_RESULT
        OUTPUT_VARIABLE LSCPU_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(LSCPU_RESULT EQUAL 0)
        string(REGEX MATCH "[Ff]lags:[^
]*" LSCPU_FLAGS_LINE "${LSCPU_OUTPUT}")
        if(LSCPU_FLAGS_LINE)
            string(REGEX REPLACE "^[^:]*:[ 	]*" "" HOST_CPU_FLAGS "${LSCPU_FLAGS_LINE}")
            string(TOLOWER "${HOST_CPU_FLAGS}" HOST_CPU_FLAGS)
        endif()
    endif()
endif()

function(host_has_cpu_flag flag out_var)
    if(HOST_CPU_FLAGS MATCHES "(^| )${flag}( |$)")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

host_has_cpu_flag("avx2" HOST_HAS_AVX2)
host_has_cpu_flag("avx512f" HOST_HAS_AVX512F)
host_has_cpu_flag("avx512bw" HOST_HAS_AVX512BW)

if(BENCHMARK_FORCE_ISA)
    set(HOST_HAS_AVX2 TRUE)
    set(HOST_HAS_AVX512F TRUE)
    set(HOST_HAS_AVX512BW TRUE)
endif()

set(COMMON_COMPILE_OPTIONS -O3 -Wno-ignored-attributes)

function(apply_common_compile_logic target_name)
    target_compile_options(${target_name} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_link_libraries(${target_name} PRIVATE OpenMP::OpenMP_CXX)
    if(NOT BENCHMARK_DEBUG)
        target_compile_definitions(${target_name} PRIVATE NDEBUG)
    endif()
endfunction()

function(add_benchmark_target target_name source_file)
    add_executable(${target_name} ${source_file})
    apply_common_compile_logic(${target_name})

    if(target_name MATCHES "avx512")
        target_compile_options(${target_name} PRIVATE -mavx512f -mavx512bw)
    elseif(target_name MATCHES "avx2")
        target_compile_options(${target_name} PRIVATE -mavx2)
    endif()
endfunction()

function(add_target_if_supported target_name source_file)
    if(target_name MATCHES "avx512")
        if(HOST_HAS_AVX512F AND HOST_HAS_AVX512BW)
            add_benchmark_target(${target_name} ${source_file})
        else()
            message(STATUS "Skipping ${target_name}: missing CPU feature(s): avx512f,avx512bw")
        endif()
    elseif(target_name MATCHES "avx2")
        if(HOST_HAS_AVX2)
            add_benchmark_target(${target_name} ${source_file})
        else()
            message(STATUS "Skipping ${target_name}: missing CPU feature(s): avx2")
        endif()
    else()
        add_benchmark_target(${target_name} ${source_file})
    endif()
endfunction()

add_executable(checker utility/checker.cpp)
apply_common_compile_logic(checker)

add_executable(generator utility/generator.cc)
target_compile_options(generator PRIVATE -O3)
if(NOT BENCHMARK_DEBUG)
    target_compile_definitions(generator PRIVATE NDEBUG)
endif()

add_executable(benchmark_runner utility/benchmark_runner.cc)
target_compile_options(benchmark_runner PRIVATE -O3)
if(NOT BENCHMARK_DEBUG)
    target_compile_definitions(benchmark_runner PRIVATE NDEBUG)
endif()

add_target_if_supported(legacy source/legacy.cpp)
add_target_if_supported(recursion source/recursion.cpp)
add_target_if_supported(simd_ord_fill source/simd_ord_fill.cpp)
add_target_if_supported(simd_ord_precalc source/simd_ord_precalc.cpp)
add_target_if_supported(simd_avx2_fill source/simd_avx2_fill.cpp)
add_target_if_supported(simd_avx2_precalc source/simd_avx2_precalc.cpp)
add_target_if_supported(simd_avx2_omp_parallel source/simd_avx2_omp_parallel.cpp)
add_target_if_supported(simd_avx512_precalc source/simd_avx512_precalc.cpp)
add_target_if_supported(simd_avx512_omp_parallel source/simd_avx512_omp_parallel.cpp)
