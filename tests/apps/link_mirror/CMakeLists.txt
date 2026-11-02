# SPDX-License-Identifier: GPL-3.0-or-later
# QindaLink mirror: pure framing, input mapping and decoder-selection tests. The live phone
# check is handoff evidence (qindaqt-link-mirror --grab/--probe), never a CTest row.

set(link_mirror_tests
    framing tst_mirror_framing.cpp
    input-mapping tst_input_mapping.cpp
    decoder-selection tst_decoder_selection.cpp)
while(link_mirror_tests)
    list(POP_FRONT link_mirror_tests name source)
    string(REPLACE "-" "_" target "qindaqt_link_mirror_${name}_tests")
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::LinkMirrorCore Qt6::Test)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME qindaqt.link-mirror-${name} COMMAND ${target})
    set_tests_properties(qindaqt.link-mirror-${name} PROPERTIES
        LABELS "apps;link;link-mirror" TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endwhile()
