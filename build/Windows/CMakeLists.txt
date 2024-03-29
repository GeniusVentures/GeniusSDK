cmake_minimum_required(VERSION 3.12)

# --------------------------------------------------------
# if on Windows system, build the windows executables
if (NOT ${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
    message(FATAL_ERROR "${CMAKE_HOST_SYSTEM_NAME} host system name does not match Windows - Please select the correct folder for configuring project")
endif()

# --------------------------------------------------------
# Project
project(GeniusSDK C CXX)

# --------------------------------------------------------
# build common libraries by platforms
include(../CommonCompilerOptions.CMake)

# --------------------------------------------------------
# Set toolchain file
set(CMAKE_TOOLCHAIN_FILE "${PROJECT_ROOT}/cmake/toolchain/cxx17.cmake")

# --------------------------------------------------------
# Allow multiple definitions
set(FORCE_MULTILE ON)
set(MULTIPLE_OPTION /FORCE:MULTIPLE)

# --------------------------------------------------------
# Set extra compiler flags
set(EXTRA_CXX_FLAGS)

# --------------------------------------------------------
# Set custom subfolder for ABI
set(ABI_SUBFOLDER_NAME "")


# --------------------------------------------------------
# Include common build paramaeters
include(../CommonBuildParameters.cmake)
