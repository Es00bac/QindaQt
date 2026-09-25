# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS DBus Gui Qml Quick Sql)

add_subdirectory(core)
add_subdirectory(ui)

qt_add_executable(qindalutris main.cpp)

# AGENT-CONTRACT: the interface is QML built on QindaTK (dev-libs/qindatk),
# the same contract the System Monitor follows (ADR-0220): QindaTK supplies
# the controls and the QindaTK.QindaQt bridge feeds the desktop's QST-1
# tokens into the toolkit theme. Nothing links QindaTK's C++ API.
qt_add_qml_module(qindalutris
    URI QindaQt.QindaLutris
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/LibraryPage.qml
        qml/parts/CoverGrid.qml
        qml/parts/CoverTile.qml
        qml/parts/GameDetail.qml
        qml/parts/AddWineDialog.qml
        qml/parts/AppMenuBar.qml
)

target_link_libraries(qindalutris
    PRIVATE
        QindaQt::QindaLutrisModel
        QindaQt::QindaLutrisView
        QindaQt::AppShell
        QindaQt::AppShellMenuExport
        Qt6::DBus
        Qt6::Gui
        Qt6::Quick
)
target_compile_features(qindalutris PRIVATE cxx_std_20)
set_target_properties(qindalutris PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindalutris)

# AGENT-GUARD: same installed-runtime contract as the other first-party
# applications -- AppShell's backing library lives in Qt's QML tree, so the
# executable needs the relative $ORIGIN rpath to start from a staged prefix.
set(qindalutris_bindir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set(qindalutris_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH qindalutris_qmldir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
cmake_path(RELATIVE_PATH qindalutris_qmldir
    BASE_DIRECTORY "${qindalutris_bindir}"
    OUTPUT_VARIABLE qindalutris_qml_relative)
set_target_properties(qindalutris PROPERTIES
    INSTALL_RPATH "$ORIGIN/${qindalutris_qml_relative}/QindaQt/AppShell")

install(TARGETS qindalutris
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaLutris)
install(FILES org.qindaqt.QindaLutris.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT QindaLutris)
install(FILES icons/org.qindaqt.QindaLutris.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT QindaLutris)

# The compatibility database snapshot (ADR-0275 §3). Its own component so the
# dated overlay package games-util/qindalutris-compat-db can ship it apart
# from the application; regenerate with tools/qindalutris-compat/generate.py.
install(FILES compat/compat-db-v1.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindalutris" COMPONENT QindaLutrisCompatDb)
