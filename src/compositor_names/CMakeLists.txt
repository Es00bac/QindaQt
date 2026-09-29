# SPDX-License-Identifier: GPL-3.0-or-later

# The external names of qindaqt-kwin, QindaQt's compositor (ADR-0289): program,
# config files and D-Bus names. Header-only and dependency-free apart from
# Qt Core, so the session, services, Settings, shell and compositor plugin can
# all name the compositor through one definition.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

add_library(qindaqt_compositor_names INTERFACE)
add_library(QindaQt::CompositorNames ALIAS qindaqt_compositor_names)
target_include_directories(
    qindaqt_compositor_names
    INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(qindaqt_compositor_names INTERFACE Qt6::Core)
target_compile_features(qindaqt_compositor_names INTERFACE cxx_std_20)
