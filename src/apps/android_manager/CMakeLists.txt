# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_android_manager STATIC)
add_library(QindaQt::AndroidManager ALIAS qindaqt_android_manager)
target_sources(qindaqt_android_manager
    PRIVATE src/application_controller.cpp src/setup_port.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/android_manager/setup_port.h
        include/qindaqt/android_manager/runtime_port.h
        include/qindaqt/android_manager/application_controller.h)
target_link_libraries(qindaqt_android_manager PUBLIC QindaQt::ApplicationCatalog QindaQt::AndroidRuntimeProtocol Qt6::Core)
target_compile_features(qindaqt_android_manager PUBLIC cxx_std_20)
set_target_properties(qindaqt_android_manager PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_android_manager)
install(TARGETS qindaqt_android_manager EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(QINDAQT_ANDROID_MANAGER_MODEL_ONLY)
    return()
endif()
find_package(QindaTK CONFIG REQUIRED)

# Confine native process/filesystem reach to this adapter target; model/UI
# consumers borrow only SetupPort, never QProcess or executable paths.
qt_add_library(qindaqt_android_manager_native_setup STATIC native_setup_port.cpp native_setup_port.h)
target_link_libraries(qindaqt_android_manager_native_setup PRIVATE QindaQt::AndroidManager)
qindaqt_enable_warnings(qindaqt_android_manager_native_setup)

qt_add_executable(qindaqt-android-applications main.cpp)
qt_add_qml_module(qindaqt-android-applications
    URI QindaQt.AndroidApplications VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml)
target_link_libraries(qindaqt-android-applications PRIVATE QindaQt::AndroidManager
    qindaqt_android_manager_native_setup QindaQt::AndroidNativeBootstrap QindaTK::qindatk Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
qindaqt_enable_warnings(qindaqt-android-applications)

install(TARGETS qindaqt-android-applications RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT AndroidApplications)
# AGENT-CONTRACT: visible setup is only for the focused Portage component
# which RDEPENDs on app-portage/qindaportage. Ordinary desktop builds stay hidden.
option(QINDAQT_ANDROID_SETUP_MENU_ENABLED "Expose the dependency-qualified Android setup menu" OFF)
set(android_desktop_entry "${CMAKE_CURRENT_SOURCE_DIR}/org.qindaqt.AndroidApplications.desktop")
if(QINDAQT_ANDROID_SETUP_MENU_ENABLED)
    file(READ "${android_desktop_entry}" android_setup_desktop)
    string(REPLACE "NoDisplay=true\n" "" android_setup_desktop "${android_setup_desktop}")
    string(REPLACE "# AGENT-GUARD: Keep hidden until the production setup/runtime port is actionable."
        "# AGENT-CONTRACT: Setup menu requires app-portage/qindaportage; helper readiness is not guest readiness."
        android_setup_desktop "${android_setup_desktop}")
    set(android_desktop_entry "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.AndroidApplications.desktop")
    file(GENERATE OUTPUT "${android_desktop_entry}" CONTENT "${android_setup_desktop}")
endif()
install(FILES "${android_desktop_entry}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT AndroidApplications)
