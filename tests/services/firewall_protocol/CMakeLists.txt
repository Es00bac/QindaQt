# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_firewall_codec_tests tst_firewall_codec.cpp)
target_link_libraries(qindaqt_firewall_codec_tests
                      PRIVATE QindaQt::FirewallProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_firewall_codec_tests)
add_test(NAME qindaqt.firewall-protocol-codec
         COMMAND qindaqt_firewall_codec_tests)
set_tests_properties(qindaqt.firewall-protocol-codec
                     PROPERTIES LABELS "service;firewall;pure")
