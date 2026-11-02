# SPDX-License-Identifier: GPL-3.0-or-later
# QindaLink desktop-side tests. Rows are named qindaqt.link-* and use only
# QTemporaryDir state and ephemeral loopback ports (never 47160).

qt_add_executable(qindaqt_link_vectors_test tst_link_vectors.cpp)
target_link_libraries(qindaqt_link_vectors_test PRIVATE QindaQt::LinkProtocol Qt6::Test)
target_compile_definitions(
    qindaqt_link_vectors_test
    PRIVATE QINDAQT_LINK_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vectors"
)
qindaqt_enable_warnings(qindaqt_link_vectors_test)
add_test(NAME qindaqt.link-vectors COMMAND qindaqt_link_vectors_test)
set_tests_properties(qindaqt.link-vectors PROPERTIES LABELS "service;link" TIMEOUT 30)
