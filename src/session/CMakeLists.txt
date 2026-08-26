# SPDX-License-Identifier: GPL-3.0-or-later

add_library(qindaqt_session_support STATIC
    installpaths.cpp
    installpaths.h
    kwincommandbuilder.cpp
    kwincommandbuilder.h
    sessioncommandline.cpp
    sessioncommandline.h
    sessiondefaults.cpp
    sessiondefaults.h
    sessionenvironment.cpp
    sessionenvironment.h
    sessionoptions.h
)
add_library(QindaQt::SessionSupport ALIAS qindaqt_session_support)

target_compile_features(qindaqt_session_support PUBLIC cxx_std_20)
target_include_directories(qindaqt_session_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_session_support PUBLIC Qt6::Core)
target_compile_definitions(
    qindaqt_session_support
    # AGENT-CONTRACT: The launcher binary is an installed-session component.
    # Build-tree tests must pass their transient plugin root explicitly instead
    # of leaking a developer build directory into an installed executable.
    PRIVATE
        QINDAQT_INSTALL_BIN_DIR="${KDE_INSTALL_BINDIR}"
        QINDAQT_INSTALL_PLUGIN_DIR="${KDE_INSTALL_PLUGINDIR}"
        QINDAQT_INSTALL_FULL_PLUGIN_DIR="${KDE_INSTALL_FULL_PLUGINDIR}"
)
set_target_properties(qindaqt_session_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_session_support)

qt_add_executable(qindaqt-wm main.cpp)
target_link_libraries(qindaqt-wm PRIVATE QindaQt::SessionSupport Qt6::Core)
target_compile_definitions(qindaqt-wm PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}")
set_target_properties(qindaqt-wm PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-wm)

install(TARGETS qindaqt-wm RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
