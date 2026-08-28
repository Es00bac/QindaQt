# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_transaction STATIC)
add_library(QindaQt::DisplayTransaction ALIAS qindaqt_display_transaction)

target_sources(
    qindaqt_display_transaction
    PRIVATE
        src/transaction_journal.cpp
        src/transaction_machine.cpp
        src/transaction_machine_events.cpp
        src/transaction_machine_p.h
        src/transaction_machine_revert.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_transaction/transaction_journal.h
            include/qindaqt/services/display_transaction/transaction_machine.h
            include/qindaqt/services/display_transaction/transaction_ports.h
            include/qindaqt/services/display_transaction/transaction_types.h
)
target_link_libraries(
    qindaqt_display_transaction
    PUBLIC QindaQt::DisplayProtocol QindaQt::DisplayTopology Qt6::Core
)
target_compile_features(qindaqt_display_transaction PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_transaction PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_transaction)

install(
    TARGETS qindaqt_display_transaction
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
