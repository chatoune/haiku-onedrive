cmake_minimum_required(VERSION 3.16)
project(HaikuOneDrive VERSION 0.0.1 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Haiku libraries
find_library(BE_LIBRARY be)
find_library(NETWORK_LIBRARY network)
find_library(BNETAPI_LIBRARY bnetapi)
find_library(NETSERVICES2_LIBRARY netservices2)
find_library(TRACKER_LIBRARY tracker)

# Check if required libraries are found
if(NOT BE_LIBRARY)
    message(FATAL_ERROR "Could not find be library")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/daemon
    ${CMAKE_SOURCE_DIR}/src/shared
)

# Add subdirectories
add_subdirectory(tests)

# Enable testing
enable_testing()