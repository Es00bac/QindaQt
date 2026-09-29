# SPDX-License-Identifier: GPL-3.0-or-later
# OpenSSL 3.2 introduced Argon2; runtime provider availability fails closed.
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
add_library(qindaqt_keyring_storage STATIC
    src/secure_buffer.cpp src/crypto.cpp src/format.cpp
    src/atomic_file.cpp src/collection_store.cpp)
add_library(QindaQt::KeyringStorage ALIAS qindaqt_keyring_storage)
target_include_directories(qindaqt_keyring_storage PUBLIC include)
target_link_libraries(qindaqt_keyring_storage PRIVATE OpenSSL::Crypto)
target_compile_features(qindaqt_keyring_storage PUBLIC cxx_std_20)
set_target_properties(qindaqt_keyring_storage PROPERTIES CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_keyring_storage)
endif()
