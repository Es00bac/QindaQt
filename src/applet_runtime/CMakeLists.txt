# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_applet_runtime STATIC)
add_library(QindaQt::AppletRuntime ALIAS qindaqt_applet_runtime)

target_sources(
    qindaqt_applet_runtime
    PRIVATE
        src/applet_instance_resolver.cpp
        src/builtin_applet_registry.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/applet_runtime/applet_instance_resolver.h
            include/qindaqt/applet_runtime/builtin_applet_registry.h
)
target_link_libraries(
    qindaqt_applet_runtime
    PUBLIC
        QindaQt::AppletHost
        qindaqt_applets
        qindaqt_profiles
        Qt6::Core
)
target_compile_features(qindaqt_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_applet_runtime PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_applet_runtime)

install(
    TARGETS qindaqt_applet_runtime
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
