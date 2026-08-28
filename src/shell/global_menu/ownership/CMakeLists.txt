# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_global_menu_ownership STATIC)
add_library(QindaQt::GlobalMenuOwnership ALIAS qindaqt_global_menu_ownership)

target_sources(
    qindaqt_global_menu_ownership
    PRIVATE
        src/active_provider_selector.cpp
        src/invocation_guard.cpp
        src/provider_authenticator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/ownership/active_provider_selector.h
            include/qindaqt/shell/global_menu/ownership/active_window_source.h
            include/qindaqt/shell/global_menu/ownership/credential_source.h
            include/qindaqt/shell/global_menu/ownership/invocation_guard.h
            include/qindaqt/shell/global_menu/ownership/menu_provider_registration.h
            include/qindaqt/shell/global_menu/ownership/provider_authenticator.h
            include/qindaqt/shell/global_menu/ownership/window_identity.h
)
target_link_libraries(
    qindaqt_global_menu_ownership PUBLIC QindaQt::GlobalMenuProtocol Qt6::Core
)
target_compile_features(qindaqt_global_menu_ownership PUBLIC cxx_std_20)
set_target_properties(qindaqt_global_menu_ownership PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_global_menu_ownership)

install(
    TARGETS qindaqt_global_menu_ownership
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
