# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_library(qindaqt_display_journal STATIC)
add_library(QindaQt::DisplayJournal ALIAS qindaqt_display_journal)

target_sources(
    qindaqt_display_journal
    PRIVATE
        src/file_journal_hooks_p.h
        src/file_journal_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_journal/file_journal_store.h
)
target_link_libraries(
    qindaqt_display_journal
    PUBLIC QindaQt::DisplayWriter QindaQt::DisplayTransaction Qt6::Core
)
target_compile_features(qindaqt_display_journal PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_journal PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_journal)

install(
    TARGETS qindaqt_display_journal
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DisplayJournalDevelopment
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT DisplayJournalDevelopment
)
