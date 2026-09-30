# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)
find_package(KF6Config 6.0 REQUIRED CONFIG)
qt_add_library(qindaqt_application_uri_opener STATIC)
add_library(QindaQt::ApplicationUriOpener ALIAS qindaqt_application_uri_opener)
target_sources(qindaqt_application_uri_opener PRIVATE application_uri_opener.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/application_uri/application_uri_opener.h)
target_link_libraries(qindaqt_application_uri_opener PUBLIC qindaqt_settings_default_apps Qt6::Core PRIVATE QindaQt::ShellLauncher)
target_compile_features(qindaqt_application_uri_opener PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_application_uri_opener)
qt_add_executable(qindaqt-uri-relay app/uri_relay.cpp)
target_link_libraries(qindaqt-uri-relay PRIVATE Qt6::Core)
qindaqt_enable_warnings(qindaqt-uri-relay)
install(TARGETS qindaqt_application_uri_opener EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)
install(TARGETS qindaqt-uri-relay RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
