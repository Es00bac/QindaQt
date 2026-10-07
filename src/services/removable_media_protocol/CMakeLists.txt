# SPDX-License-Identifier: LGPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)
qt_add_library(qindaqt_removable_media_protocol STATIC)
add_library(QindaQt::RemovableMediaProtocol ALIAS qindaqt_removable_media_protocol)
target_sources(qindaqt_removable_media_protocol PRIVATE
    src/media_value_validation.cpp src/media_message_validation.cpp
    src/media_validation_p.h src/media_codec_p.h src/media_codec_fields.cpp
    src/media_snapshot_codec.cpp src/media_action_codec.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/removable_media_protocol/media_limits.h
    include/qindaqt/services/removable_media_protocol/media_types.h
    include/qindaqt/services/removable_media_protocol/media_validation.h
    include/qindaqt/services/removable_media_protocol/media_codec.h)
target_link_libraries(qindaqt_removable_media_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_removable_media_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_removable_media_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_removable_media_protocol)
install(TARGETS qindaqt_removable_media_protocol EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtRemovableMediaProtocol
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtRemovableMediaProtocol)
