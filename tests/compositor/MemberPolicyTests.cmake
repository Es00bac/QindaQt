# SPDX-License-Identifier: GPL-3.0-or-later
# Member/transient policy unit tests. Included from tests/compositor/CMakeLists.txt
# inside its KWin-plugin condition; kept separate so that file stays within the
# source-shape limit.

foreach(member_policy_test IN ITEMS hybridmemberpolicy hybridtransientpolicy)
    qt_add_executable(
        qindaqt_${member_policy_test}_tests
        "tst_${member_policy_test}.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/${member_policy_test}.cpp"
    )
    target_include_directories(
        qindaqt_${member_policy_test}_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_${member_policy_test}_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME "compositor.${member_policy_test}"
        COMMAND qindaqt_${member_policy_test}_tests
    )
endforeach()
target_compile_features(qindaqt_hybridmemberpolicy_tests PRIVATE cxx_std_20)
target_compile_features(qindaqt_hybridtransientpolicy_tests PRIVATE cxx_std_20)
target_sources(qindaqt_hybridmemberpolicy_tests PRIVATE hybridmemberpolicy_testfixtures.h)

qt_add_executable(
    qindaqt_hybridmemberpolicy_containers_tests
    tst_hybridmemberpolicycontainers.cpp
    hybridmemberpolicy_testfixtures.h
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridmemberpolicy.cpp"
)
target_include_directories(
    qindaqt_hybridmemberpolicy_containers_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybridmemberpolicy_containers_tests
    PRIVATE Qt6::Core Qt6::Test
)
target_compile_features(qindaqt_hybridmemberpolicy_containers_tests PRIVATE cxx_std_20)
add_test(
    NAME compositor.hybridmemberpolicy-containers
    COMMAND qindaqt_hybridmemberpolicy_containers_tests
)
