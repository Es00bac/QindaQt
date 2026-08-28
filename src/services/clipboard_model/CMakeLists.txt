# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_clipboard_model STATIC)
add_library(QindaQt::ClipboardModel ALIAS qindaqt_clipboard_model)

target_sources(
    qindaqt_clipboard_model
    PRIVATE
        src/clipboard_codec.cpp
        src/clipboard_descriptor.cpp
        src/clipboard_history.cpp
        src/clipboard_history_detail.cpp
        src/clipboard_history_mutations.cpp
        src/clipboard_history_search.cpp
        src/clipboard_media.cpp
        src/clipboard_normalize.cpp
        src/clipboard_codec_p.h
        src/clipboard_history_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/clipboard_model/clipboard_codec.h
            include/qindaqt/services/clipboard_model/clipboard_descriptor.h
            include/qindaqt/services/clipboard_model/clipboard_history.h
            include/qindaqt/services/clipboard_model/clipboard_media.h
            include/qindaqt/services/clipboard_model/clipboard_types.h
)
target_link_libraries(qindaqt_clipboard_model PUBLIC Qt6::Core)
target_compile_features(qindaqt_clipboard_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_clipboard_model PROPERTIES CXX_EXTENSIONS OFF)
# AGENT-CONTRACT: additive C1 packaging requirement. The archive itself stays
# STATIC for every C0 consumer, but it must be position-independent so the
# C1 Clipboard applet can embed it in its relocatable shared library
# (src/shell/clipboard_applet). No symbol, ABI, or authority change.
set_target_properties(qindaqt_clipboard_model PROPERTIES POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_clipboard_model)

install(
    TARGETS qindaqt_clipboard_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# AGENT-CONTRACT: additive per-consumer COMPONENT, mirroring the TextEditor
# precedent in src/controls/CMakeLists.txt. The Clipboard applet's public
# headers include qindaqt/services/clipboard_model/clipboard_types.h, so the
# packaged ClipboardApplet component must stage those public headers beside
# its own; the archive is embedded in the applet library and consumers do not
# relink it. Added here because src/services/clipboard_model is not the
# Clipboard applet's owned path.
install(
    TARGETS qindaqt_clipboard_model
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT ClipboardApplet
)
