cmake_minimum_required(VERSION 3.15)

# Project name and version
project(VirtualTestSet VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# COMTRADE parser library
add_library(comtrade_parser STATIC
    ${PROJECT_SOURCE_DIR}/src/comtrade_parser.cpp
)

# SCD parser library
add_library(scd_parser STATIC
    ${PROJECT_SOURCE_DIR}/src/scd_parser.cpp
)

# Phasor injection library
add_library(phasor_injection STATIC
    ${PROJECT_SOURCE_DIR}/src/phasor_injection_test.cpp
)

# COMTRADE replay library
add_library(comtrade_replay STATIC
    ${PROJECT_SOURCE_DIR}/src/comtrade_replay_test.cpp
)
target_link_libraries(comtrade_replay PRIVATE comtrade_parser)

# Main application
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/app.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE comtrade_parser scd_parser phasor_injection comtrade_replay)

# Phasor injection test
add_executable(phasor_test
    ${PROJECT_SOURCE_DIR}/src/phasor_test.cpp
)
target_link_libraries(phasor_test PRIVATE phasor_injection)

# Link libraries
# For Linux: pthread for timing
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
    target_link_libraries(phasor_test PRIVATE pthread)
endif()

# Installation rules
install(TARGETS ${PROJECT_NAME} phasor_test DESTINATION bin)
