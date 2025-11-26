cmake_minimum_required(VERSION 3.15)

# Project name and version
project(VirtualTestSet VERSION 1.0.0 LANGUAGES C CXX)

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

# Platform-specific configuration
if(WIN32)
    # Windows with MSVC or MinGW
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 or later
    
    # Find Npcap SDK
    # User should set NPCAP_SDK_DIR environment variable or pass via -DNPCAP_SDK_DIR=path
    if(NOT DEFINED NPCAP_SDK_DIR)
        if(DEFINED ENV{NPCAP_SDK_DIR})
            set(NPCAP_SDK_DIR $ENV{NPCAP_SDK_DIR})
        else()
            # Try common installation paths
            set(NPCAP_SDK_DIR "C:/Program Files/Npcap SDK" "C:/Npcap-sdk")
        endif()
    endif()
    
    # Find the Npcap SDK directory
    find_path(PCAP_INCLUDE_DIR 
        NAMES pcap/pcap.h pcap.h
        PATHS ${NPCAP_SDK_DIR}
        PATH_SUFFIXES Include
        NO_DEFAULT_PATH
    )
    
    # For MinGW, prefer libwpcap.a (import library), otherwise use wpcap.lib
    find_library(PCAP_LIBRARY
        NAMES libwpcap.a
        PATHS ${NPCAP_SDK_DIR}
        PATH_SUFFIXES Lib/x64
        NO_DEFAULT_PATH
    )
    
    if(NOT PCAP_LIBRARY)
        # Fallback to wpcap.lib for MSVC
        find_library(PCAP_LIBRARY
            NAMES wpcap
            PATHS ${NPCAP_SDK_DIR}
            PATH_SUFFIXES Lib Lib/x64
            NO_DEFAULT_PATH
        )
    endif()
    
    if(NOT PCAP_INCLUDE_DIR OR NOT PCAP_LIBRARY)
        message(FATAL_ERROR 
            "\n"
            "==============================================================================\n"
            "ERROR: Npcap SDK not found!\n"
            "==============================================================================\n"
            "\n"
            "This project requires Npcap SDK for Windows packet capture.\n"
            "\n"
            "Installation steps:\n"
            "  1. Download Npcap SDK from: https://npcap.com/#download\n"
            "  2. Extract to: C:\\npcap-sdk\n"
            "  3. Set environment variable:\n"
            "     - MSYS2: export NPCAP_SDK_DIR=\"/c/npcap-sdk\"\n"
            "     - PowerShell: $env:NPCAP_SDK_DIR=\"C:\\npcap-sdk\"\n"
            "     - Or set system-wide in Windows Environment Variables\n"
            "\n"
            "Alternative: Specify path directly:\n"
            "  cmake -G Ninja -DNPCAP_SDK_DIR=\"C:/npcap-sdk\" ..\n"
            "\n"
            "Current search path: ${NPCAP_SDK_DIR}\n"
            "See WINDOWS_SETUP.md for complete instructions.\n"
            "==============================================================================\n"
        )
    else()
        message(STATUS "Found Npcap SDK: ${NPCAP_SDK_DIR}")
        message(STATUS "Pcap include: ${PCAP_INCLUDE_DIR}")
        message(STATUS "Pcap library: ${PCAP_LIBRARY}")
        include_directories(${PCAP_INCLUDE_DIR})
    endif()
    
    # Windows-specific compiler flags
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    else()
        # MinGW or similar
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    endif()
else()
    # Unix-like systems (Linux, macOS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

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
target_link_libraries(comtrade_replay PUBLIC comtrade_parser)

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

# Link libraries based on platform
if(WIN32)
    # Windows: Link Npcap, WinSock2, and iphlpapi
    if(PCAP_LIBRARY)
        # Link wpcap to the static libraries that use raw_socket.h
        target_link_libraries(phasor_injection PUBLIC ${PCAP_LIBRARY} ws2_32 iphlpapi)
        target_link_libraries(comtrade_replay PUBLIC ${PCAP_LIBRARY} ws2_32 iphlpapi)
        
        # Link to executables
        target_link_libraries(${PROJECT_NAME} PRIVATE ${PCAP_LIBRARY} ws2_32 iphlpapi)
        target_link_libraries(phasor_test PRIVATE ${PCAP_LIBRARY} ws2_32 iphlpapi)
    else()
        message(FATAL_ERROR "Npcap library not found. Cannot build without Npcap SDK.")
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux: pthread for timing
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
    target_link_libraries(phasor_test PRIVATE pthread)
endif()

# Installation rules
install(TARGETS ${PROJECT_NAME} phasor_test DESTINATION bin)
