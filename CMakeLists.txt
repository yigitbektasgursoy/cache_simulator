cmake_minimum_required(VERSION 3.15)
project(cache_simulator CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCE_FILES
    src/address.cpp
    src/cache.cpp
    src/cache_policy.cpp
    src/memory.cpp
    src/metrics.cpp
)

# Main executable
add_executable(cache_simulator src/main.cpp ${SOURCE_FILES})

# Test executable
add_executable(cache_test tests/cache_test.cpp ${SOURCE_FILES})
add_executable(adress_test tests/address_test.cpp src/address.cpp)


# Enable testing
enable_testing()
add_test(NAME cache_test COMMAND cache_test)

# Add an option to enable sanitizers in debug mode
option(ENABLE_SANITIZERS "Enable sanitizers in debug mode" OFF)
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug AND NOT MSVC)
    message(STATUS "Enabling sanitizers in debug mode")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Installation
install(TARGETS cache_simulator DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/cache_simulator)

# Print configuration
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
