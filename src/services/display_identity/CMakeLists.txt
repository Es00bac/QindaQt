# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_identity STATIC)
add_library(QindaQt::DisplayIdentity ALIAS qindaqt_display_identity)

target_sources(
    qindaqt_display_identity
    PRIVATE
        src/identity_registry.cpp
        src/identity_resolver.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_identity/identity_limits.h
            include/qindaqt/services/display_identity/identity_registry.h
            include/qindaqt/services/display_identity/identity_resolver.h
            include/qindaqt/services/display_identity/identity_types.h
)
target_link_libraries(qindaqt_display_identity PUBLIC Qt6::Core)
target_compile_features(qindaqt_display_identity PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_identity PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_identity)

install(
    TARGETS qindaqt_display_identity
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
