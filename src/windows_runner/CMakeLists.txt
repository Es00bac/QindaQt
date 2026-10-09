# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_windows_runner STATIC)
add_library(QindaQt::WindowsRunner ALIAS qindaqt_windows_runner)
target_sources(qindaqt_windows_runner
    PRIVATE src/launch_plan.cpp src/windows_runner.cpp
        src/catalog_binding.cpp src/executable_file.cpp src/executable_open.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/windows_runner/launch_types.h
        include/qindaqt/windows_runner/launch_ports.h
        include/qindaqt/windows_runner/launch_plan.h
        include/qindaqt/windows_runner/windows_runner.h
        include/qindaqt/windows_runner/catalog_binding.h
        include/qindaqt/windows_runner/executable_file.h
        include/qindaqt/windows_runner/executable_open.h)
target_link_libraries(qindaqt_windows_runner PUBLIC Qt6::Core QindaQt::ShellLauncher)
target_compile_features(qindaqt_windows_runner PUBLIC cxx_std_20)
set_target_properties(qindaqt_windows_runner PROPERTIES
    POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_windows_runner)
install(TARGETS qindaqt_windows_runner EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
