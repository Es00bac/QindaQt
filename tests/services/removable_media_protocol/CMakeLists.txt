# SPDX-License-Identifier: GPL-3.0-or-later
foreach(behavior codec hostile validation)
    qt_add_executable(qindaqt_media_protocol_${behavior}_tests tst_media_${behavior}.cpp)
    target_link_libraries(qindaqt_media_protocol_${behavior}_tests
        PRIVATE QindaQt::RemovableMediaProtocol Qt6::Test)
    qindaqt_enable_warnings(qindaqt_media_protocol_${behavior}_tests)
    add_test(NAME qindaqt.removable-media-protocol-${behavior}
        COMMAND qindaqt_media_protocol_${behavior}_tests)
    set_tests_properties(qindaqt.removable-media-protocol-${behavior} PROPERTIES
        LABELS "service;media;protocol;pure" TIMEOUT 30)
endforeach()
