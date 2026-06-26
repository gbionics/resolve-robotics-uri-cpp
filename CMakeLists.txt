# SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project(ResolveRoboticsURICpp
        LANGUAGES CXX C
        VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake < 3.21 compatibility
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL TRUE)
    else()
        set(PROJECT_IS_TOP_LEVEL FALSE)
    endif()
endif()

option(BUILD_TESTING "Create tests using CMake" OFF)

if(BUILD_TESTING)
    enable_testing()
endif()


# Get AddInstallRPATHSupport, InstallBasicPackageFiles, AddUninstallTarget
if(NOT DEFINED RRUP_USE_SYSTEM_YCM)
    find_package(YCM QUIET)
    set(RRUP_USE_SYSTEM_YCM_DEFAULT_VALUE ${YCM_FOUND})
else()
    set(RRUP_USE_SYSTEM_YCM_DEFAULT_VALUE ${RRUP_USE_SYSTEM_YCM})
endif()
option(RRUP_USE_SYSTEM_YCM "If ON, look for YCM with find_package(YCM) instead of FetchContent" ${RRUP_USE_SYSTEM_YCM_DEFAULT_VALUE})
if(RRUP_USE_SYSTEM_YCM)
    find_package(YCM REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(YCM
                         GIT_REPOSITORY https://github.com/robotology/ycm
                         GIT_TAG v0.18.2)
    FetchContent_MakeAvailable(YCM)
endif()

# Encourage user to specify a build type (e.g. Release, Debug, etc.), otherwise set it to Release.
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as none was specified.")
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    endif()
endif()

# Add ResolveRoboticsURICpp C++ header-only library
add_library(ResolveRoboticsURICpp INTERFACE ResolveRoboticsURICpp.h)
add_library(ResolveRoboticsURICpp::ResolveRoboticsURICpp ALIAS ResolveRoboticsURICpp)
target_include_directories(ResolveRoboticsURICpp INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                            "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(ResolveRoboticsURICpp PROPERTIES PUBLIC_HEADER ResolveRoboticsURICpp.h)
# Uses C++17 (optional)
target_compile_featureS(ResolveRoboticsURICpp INTERFACE cxx_std_17)

option(RRUC_INSTALL "Install the library" ${PROJECT_IS_TOP_LEVEL})

if(RRUC_INSTALL)
    install(TARGETS ResolveRoboticsURICpp
            EXPORT  ${PROJECT_NAME})
endif()

# Add resolve-robotics-uri-cpp stand-alone add_executable
option(RRUC_BUILD_TOOL "Build the resolve-robotics-uri-cpp tool" ${PROJECT_IS_TOP_LEVEL})
if(RRUC_BUILD_TOOL)
    add_executable(resolve-robotics-uri-cpp resolve-robotics-uri-cpp.cpp)
    target_link_libraries(resolve-robotics-uri-cpp PRIVATE ResolveRoboticsURICpp::ResolveRoboticsURICpp)
    if(RRUC_INSTALL)
        install(TARGETS resolve-robotics-uri-cpp
                EXPORT  ${PROJECT_NAME})
    endif()
endif()

if(RRUC_INSTALL)
    include(InstallBasicPackageFiles)
    install_basic_package_files(${PROJECT_NAME}
                                VERSION ${${PROJECT_NAME}_VERSION}
                                COMPATIBILITY AnyNewerVersion
                                VARS_PREFIX ${PROJECT_NAME}
                                NO_CHECK_REQUIRED_COMPONENTS_MACRO)

    # Uninstall
    include(AddUninstallTarget)
endif()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(test)
endif()

