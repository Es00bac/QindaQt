# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_android_runtime_protocol STATIC)
add_library(QindaQt::AndroidRuntimeProtocol ALIAS qindaqt_android_runtime_protocol)
target_sources(qindaqt_android_runtime_protocol
    PRIVATE src/runtime_snapshot.cpp src/protocol.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/android_runtime/runtime_port.h
        include/qindaqt/services/android_runtime/protocol.h)
target_link_libraries(qindaqt_android_runtime_protocol PUBLIC QindaQt::ShellLauncher Qt6::Core)
target_compile_features(qindaqt_android_runtime_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_android_runtime_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_android_runtime_protocol)

qt_add_library(qindaqt_android_runtime STATIC)
add_library(QindaQt::AndroidRuntime ALIAS qindaqt_android_runtime)
target_sources(qindaqt_android_runtime PRIVATE
    src/packet_channel.cpp src/packet_channel_p.h src/process_lifetime.cpp src/process_lifetime_p.h
    src/renderer_lifetime.cpp src/descriptor_runtime_port.cpp src/producer_endpoint.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/android_runtime/descriptors.h
        include/qindaqt/services/android_runtime/descriptor_runtime_port.h
        include/qindaqt/services/android_runtime/producer_endpoint.h
        include/qindaqt/services/android_runtime/renderer_lifetime.h)
target_link_libraries(qindaqt_android_runtime PUBLIC QindaQt::AndroidRuntimeProtocol Qt6::Core)
target_compile_definitions(qindaqt_android_runtime PRIVATE _GNU_SOURCE)
target_compile_features(qindaqt_android_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_android_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_android_runtime)
install(TARGETS qindaqt_android_runtime_protocol qindaqt_android_runtime EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

option(QINDAQT_BUILD_ANDROID_NATIVE_COLLECTOR "Build the package-owned Android collector/bootstrap" ON)
if(QINDAQT_BUILD_ANDROID_NATIVE_COLLECTOR)
    find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)
    qt_add_library(qindaqt_android_native_bootstrap STATIC
        src/collector_process.cpp src/collector_process_p.h src/collector_recipe_p.h
        src/protected_collector_file.cpp src/protected_collector_file_p.h src/native_bootstrap.cpp)
    add_library(QindaQt::AndroidNativeBootstrap ALIAS qindaqt_android_native_bootstrap)
    target_sources(qindaqt_android_native_bootstrap PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/android_runtime/native_bootstrap.h)
    target_include_directories(qindaqt_android_native_bootstrap PRIVATE src)
    target_link_libraries(qindaqt_android_native_bootstrap PUBLIC QindaQt::AndroidRuntime PRIVATE Qt6::DBus)
    target_compile_definitions(qindaqt_android_native_bootstrap PRIVATE _GNU_SOURCE)
    qindaqt_enable_warnings(qindaqt_android_native_bootstrap)
    qt_add_executable(qindaqt-android-collector src/collector_main.cpp)
    target_link_libraries(qindaqt-android-collector PRIVATE QindaQt::AndroidRuntime)
    target_compile_definitions(qindaqt-android-collector PRIVATE _GNU_SOURCE)
    qindaqt_enable_warnings(qindaqt-android-collector)
    install(TARGETS qindaqt-android-collector RUNTIME DESTINATION "libexec/qindaqt" COMPONENT AndroidApplications)
    install(FILES data/collector.json DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/android-runtime" COMPONENT AndroidApplications)
    install(TARGETS qindaqt_android_native_bootstrap EXPORT QindaQtTargets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()
