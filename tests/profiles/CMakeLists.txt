# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtProfileTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
endif()

get_filename_component(
    QINDAQT_PROFILE_TEST_REPOSITORY_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    ABSOLUTE
)

function(qindaqt_add_profile_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_profiles Qt6::Test)
    target_compile_definitions(
        ${target}
        PRIVATE QINDAQT_SOURCE_DIR="${QINDAQT_PROFILE_TEST_REPOSITORY_ROOT}"
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_profile_test(
    qindaqt_profile_tests
    tst_profiles.cpp
    qindaqt.profile-formats
)
qindaqt_add_profile_test(
    qindaqt_profile_json_validation_tests
    tst_profile_json_validation.cpp
    qindaqt.profile-json-validation
)
qindaqt_add_profile_test(
    qindaqt_profile_value_validation_tests
    tst_profile_value_validation.cpp
    qindaqt.profile-value-validation
)
