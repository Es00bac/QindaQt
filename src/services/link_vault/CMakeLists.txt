# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_link_vault STATIC)
add_library(QindaQt::LinkVault ALIAS qindaqt_link_vault)

# AGENT-NOTE: composed in the qindaqt-link-service executable only, like link_input;
# it is not part of the installed export set.
target_sources(
    qindaqt_link_vault
    PRIVATE
        src/secret_plan.cpp
        src/vault_typing_feature.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_vault/secret_plan.h
            include/qindaqt/services/link_vault/vault_typing_feature.h
)
target_link_libraries(qindaqt_link_vault PUBLIC QindaQt::LinkInput QindaQt::LinkFocus QindaQt::LinkTransport Qt6::Core)
target_compile_features(qindaqt_link_vault PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_vault PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_vault)
