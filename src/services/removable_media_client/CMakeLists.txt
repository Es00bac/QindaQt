# SPDX-License-Identifier: LGPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
qt_add_library(qindaqt_removable_media_client STATIC)
add_library(QindaQt::RemovableMediaClient ALIAS qindaqt_removable_media_client)
target_sources(qindaqt_removable_media_client PRIVATE src/media_client.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/removable_media_client/media_source.h
    include/qindaqt/services/removable_media_client/media_client.h)
target_link_libraries(qindaqt_removable_media_client PUBLIC
    QindaQt::RemovableMediaProtocol Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_removable_media_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_removable_media_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_removable_media_client)
install(TARGETS qindaqt_removable_media_client EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtRuntime
    FILE_SET HEADERS COMPONENT QindaQtRuntime)
