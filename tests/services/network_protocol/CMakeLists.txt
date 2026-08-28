# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_network_identity_tests tst_network_identity.cpp)
target_link_libraries(qindaqt_network_identity_tests
                      PRIVATE QindaQt::NetworkProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_identity_tests)
add_test(NAME qindaqt.network-protocol-identity
         COMMAND qindaqt_network_identity_tests)
set_tests_properties(qindaqt.network-protocol-identity
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_validation_tests tst_network_validation.cpp)
target_link_libraries(qindaqt_network_validation_tests
                      PRIVATE QindaQt::NetworkProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_validation_tests)
add_test(NAME qindaqt.network-protocol-validation
         COMMAND qindaqt_network_validation_tests)
set_tests_properties(qindaqt.network-protocol-validation
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_codec_tests tst_network_codec.cpp)
target_link_libraries(qindaqt_network_codec_tests
                      PRIVATE QindaQt::NetworkProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_codec_tests)
add_test(NAME qindaqt.network-protocol-codec COMMAND qindaqt_network_codec_tests)
set_tests_properties(qindaqt.network-protocol-codec
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_redaction_tests tst_network_redaction.cpp)
target_link_libraries(qindaqt_network_redaction_tests
                      PRIVATE QindaQt::NetworkProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_redaction_tests)
add_test(NAME qindaqt.network-protocol-redaction
         COMMAND qindaqt_network_redaction_tests)
set_tests_properties(qindaqt.network-protocol-redaction
                     PROPERTIES LABELS "service;network;pure")
