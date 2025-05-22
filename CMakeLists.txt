cmake_minimum_required(VERSION 3.14)
project(cache_simulator CXX)

# Set CMake policies, including CMP0135 for FetchContent timestamp behavior
cmake_policy(SET CMP0135 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann_json using FetchContent
include(FetchContent)
FetchContent_Declare(
        nlohmann_json_dep
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
set(JSON_Install OFF CACHE INTERNAL "Disable installing nlohmann_json from this project")
set(JSON_BuildTests OFF CACHE INTERNAL "Disable building nlohmann_json tests from this project")
FetchContent_MakeAvailable(nlohmann_json_dep)

# Create a library with the common source files
add_library(cache_lib
        src/address.cpp
        src/cache.cpp
        src/cache_policy.cpp
        src/memory.cpp
        src/metrics.cpp)

# Set include directories for the library
target_include_directories(cache_lib PUBLIC include)

# Link against nlohmann_json
target_link_libraries(cache_lib PUBLIC nlohmann_json::nlohmann_json)

# Create the main simulator executable
add_executable(cache_simulator src/main.cpp)
target_link_libraries(cache_simulator PRIVATE cache_lib)

# Option to build tests
option(BUILD_TESTS "Build the tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Add core component tests
    add_executable(address_test tests/address_test.cpp)
    target_link_libraries(address_test PRIVATE cache_lib)
    add_test(NAME AddressTest COMMAND address_test)

    add_executable(cache_test tests/cache_test.cpp)
    target_link_libraries(cache_test PRIVATE cache_lib)
    add_test(NAME CacheTest COMMAND cache_test)

    add_executable(cache_policy_test tests/cache_policy_test.cpp)
    target_link_libraries(cache_policy_test PRIVATE cache_lib)
    add_test(NAME CachePolicyTest COMMAND cache_policy_test)

    add_executable(memory_test tests/memory_test.cpp)
    target_link_libraries(memory_test PRIVATE cache_lib)
    add_test(NAME MemoryTest COMMAND memory_test)

    # Add specialized feature tests
    add_executable(test_cache_inclusion tests/test_cache_inclusion.cpp)
    target_link_libraries(test_cache_inclusion PRIVATE cache_lib)
    add_test(NAME CacheInclusionTest COMMAND test_cache_inclusion)

    add_executable(test_json_config tests/test_json_config.cpp)
    target_link_libraries(test_json_config PRIVATE cache_lib)
    add_test(NAME JsonConfigTest COMMAND test_json_config)

    # Add utility tool
    add_executable(visualize_cache tests/visualize_cache.cpp)
    target_link_libraries(visualize_cache PRIVATE cache_lib)
endif()

# Create directories for examples and traces
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/traces)

# Copy the example configurations to the build directory
file(GLOB EXAMPLE_CONFIGS "${CMAKE_SOURCE_DIR}/examples/*.json")
foreach(CONFIG_FILE ${EXAMPLE_CONFIGS})
    get_filename_component(FILENAME ${CONFIG_FILE} NAME)
    configure_file(${CONFIG_FILE} ${CMAKE_BINARY_DIR}/examples/${FILENAME} COPYONLY)
endforeach()