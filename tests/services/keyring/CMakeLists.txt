# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Core Test)
foreach(behavior secure_buffer collection_store storage_failures storage_decoding)
    add_executable(tst_keyring_${behavior} tst_${behavior}.cpp)
    set_target_properties(tst_keyring_${behavior} PROPERTIES AUTOMOC ON)
    target_link_libraries(tst_keyring_${behavior} PRIVATE QindaQt::KeyringStorage Qt6::Core Qt6::Test)
    add_test(NAME keyring_${behavior} COMMAND tst_keyring_${behavior})
endforeach()

target_include_directories(tst_keyring_storage_decoding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/keyring/src")
