# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Gui Quick)
find_package(LayerShellQt 6.6.6 REQUIRED)
add_library(qindaqt_authentication_overlay STATIC src/overlay_surface.cpp)
add_library(QindaQt::AuthenticationOverlay ALIAS qindaqt_authentication_overlay)
target_include_directories(qindaqt_authentication_overlay PUBLIC include)
target_link_libraries(qindaqt_authentication_overlay PUBLIC Qt6::Gui Qt6::Quick PRIVATE LayerShellQt::Interface)
target_compile_features(qindaqt_authentication_overlay PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_authentication_overlay)
