# SPDX-FileCopyrightText: Fondazione Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause

# Test dependencies
find_package(Catch2 3.0.1 REQUIRED)

function(rruc_add_unit_test)

    set(options )
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LINKS)

    set(prefix "rruc_add_unit_test")

    cmake_parse_arguments(${prefix}
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN})

    set(name ${${prefix}_NAME})
    set(unit_test_files ${${prefix}_SOURCES})

    set(targetname ${name}UnitTests)
    add_executable(${targetname}
        "${unit_test_files}")

    target_link_libraries(${targetname} PRIVATE Catch2::Catch2WithMain ${${prefix}_LINKS})
    target_compile_definitions(${targetname} PRIVATE CATCH_CONFIG_FAST_COMPILE CATCH_CONFIG_DISABLE_MATCHERS)

    add_test(NAME ${targetname} COMMAND ${targetname})
endfunction()

rruc_add_unit_test(NAME ResolveRoboticsURICpp 
                   SOURCES ResolveRoboticsURICppUnitTests.cpp 
                   LINKS ResolveRoboticsURICpp::ResolveRoboticsURICpp)

if(RRUC_BUILD_TOOL)
    foreach(legacy OFF ON)
        add_test(NAME RobotSpecificCLI_${legacy}
                 COMMAND ${CMAKE_COMMAND}
                         -DTOOL=$<TARGET_FILE:resolve-robotics-uri-cpp>
                         -DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/cli_${legacy}
                         -DLEGACY=${legacy}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/RobotSpecificCLI.cmake)
    endforeach()
endif()
