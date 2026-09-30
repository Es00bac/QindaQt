# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_compositor_attachment STATIC)
add_library(QindaQt::CompositorAttachment ALIAS qindaqt_compositor_attachment)
target_sources(qindaqt_compositor_attachment PRIVATE
    src/compositor_attachment.cpp src/attachment_socket.cpp src/attachment_socket_p.h
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/platform/compositor_attachment/compositor_attachment.h)
target_link_libraries(qindaqt_compositor_attachment PUBLIC Qt6::Core Qt6::DBus
    PRIVATE $<BUILD_INTERFACE:qindaqt_compositor_names>)
target_compile_features(qindaqt_compositor_attachment PUBLIC cxx_std_20)
set_target_properties(qindaqt_compositor_attachment PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_compositor_attachment)
install(TARGETS qindaqt_compositor_attachment EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
