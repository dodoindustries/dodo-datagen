cmake_minimum_required(VERSION 3.16)

project(dodo_datagen
    VERSION 0.2.0
    DESCRIPTION "Streaming fake data generator written in C++"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(DODO_BUILD_TESTS "Build the test suite" ON)
option(DODO_WERROR "Treat compiler warnings as errors" OFF)

# Third-party dependencies are fetched at configure time. This needs network
# access the first time the project is configured; afterwards they are cached.
include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# Shared warning flags applied to first-party targets only.
function(dodo_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        # The standard C functions we use (sscanf, getenv) are flagged by MSVC's
        # CRT as "unsafe"; silence those deprecation warnings rather than switch
        # to non-portable _s variants.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        if(DODO_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(DODO_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(dodo_lib STATIC
    src/cli.cpp
    src/schema.cpp
    src/generators.cpp
    src/i18n.cpp
    src/locale.cpp
    src/writers/csv_writer.cpp
    src/writers/json_writer.cpp
    src/writers/sql_writer.cpp
    src/data/en_us.cpp
    src/data/en_gb.cpp
    src/data/es_es.cpp
    src/data/fr_fr.cpp
    src/data/de_de.cpp)

target_include_directories(dodo_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dodo_lib PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(dodo_lib PUBLIC cxx_std_17)
dodo_set_warnings(dodo_lib)

add_executable(dodo-datagen src/main.cpp)
target_link_libraries(dodo-datagen PRIVATE dodo_lib)
dodo_set_warnings(dodo-datagen)

# Short alias command `dodo`, sharing the same entry point.
add_executable(dodo src/main.cpp)
target_link_libraries(dodo PRIVATE dodo_lib)
dodo_set_warnings(dodo)

install(TARGETS dodo-datagen dodo RUNTIME DESTINATION bin)

if(DODO_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE)
    # doctest 2.4.11 ships a pre-3.5 cmake_minimum_required, which CMake 4
    # rejects; this keeps the header-only dependency configurable there.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_MakeAvailable(doctest)
    unset(CMAKE_POLICY_VERSION_MINIMUM)

    add_executable(dodo_tests
        tests/test_rng.cpp
        tests/test_generators.cpp
        tests/test_writers.cpp
        tests/test_schema.cpp
        tests/test_golden.cpp)
    target_link_libraries(dodo_tests PRIVATE dodo_lib doctest::doctest)
    dodo_set_warnings(dodo_tests)

    add_test(NAME dodo_tests COMMAND dodo_tests)
endif()
