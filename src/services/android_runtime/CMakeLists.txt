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
