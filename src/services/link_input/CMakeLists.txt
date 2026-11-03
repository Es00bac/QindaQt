# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_input STATIC)
add_library(QindaQt::LinkInput ALIAS qindaqt_link_input)

target_sources(
    qindaqt_link_input
    PRIVATE
        src/input_feature.cpp
        src/input_frames.cpp
        src/portal_session.cpp
        src/response_sink.h
        src/restore_token_store.cpp
        src/text_typer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_input/input_feature.h
            include/qindaqt/services/link_input/input_frames.h
            include/qindaqt/services/link_input/input_sink.h
            include/qindaqt/services/link_input/portal_session.h
            include/qindaqt/services/link_input/restore_token_store.h
            include/qindaqt/services/link_input/text_typer.h
)
target_link_libraries(qindaqt_link_input PUBLIC QindaQt::LinkTransport Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_link_input PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_input PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_input)
