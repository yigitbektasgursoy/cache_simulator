cmake_minimum_required(VERSION 3.20)
project(CacheSimulator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add nlohmann_json
include(FetchContent)
FetchContent_Declare(
    nlohmann_json_dep
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json_dep)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Include directories
include_directories(include)

# Create cache library
add_library(cache_lib STATIC
    src/address.cpp
    src/cache.cpp
    src/memory.cpp
    src/cache_policy.cpp
    src/metrics.cpp
)

# Link nlohmann_json to the cache library
target_link_libraries(cache_lib nlohmann_json::nlohmann_json)

# Main cache simulator executable
add_executable(cache_simulator src/main.cpp)
target_link_libraries(cache_simulator cache_lib nlohmann_json::nlohmann_json)
