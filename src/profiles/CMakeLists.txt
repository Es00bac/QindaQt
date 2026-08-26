# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_profiles STATIC)

target_sources(
    qindaqt_profiles
    PRIVATE
        src/layout_profile.cpp
        src/profile_catalog.cpp
        src/profile_loader.cpp
        src/profile_types.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/profiles/layout_profile.h
            include/qindaqt/profiles/profile_catalog.h
            include/qindaqt/profiles/profile_loader.h
            include/qindaqt/profiles/profile_types.h
)

target_link_libraries(qindaqt_profiles PUBLIC Qt6::Core)
target_compile_features(qindaqt_profiles PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_profiles)

target_compile_definitions(
    qindaqt_profiles
    PUBLIC
        QINDAQT_PROFILE_SCHEMA_VERSION=1
)

install(
    TARGETS qindaqt_profiles
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/profiles/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
)
