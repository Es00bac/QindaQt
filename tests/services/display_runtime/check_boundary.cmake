# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing Display runtime boundary source root")
endif()

set(runtime "${SOURCE_ROOT}/src/services/display_runtime")
set(service "${SOURCE_ROOT}/src/services/display_service")
file(GLOB_RECURSE public_headers LIST_DIRECTORIES false
     "${runtime}/include/*.h" "${runtime}/include/*.hh"
     "${runtime}/include/*.hpp" "${runtime}/include/*.hxx")
foreach(header IN LISTS public_headers)
    file(READ "${header}" content)
    if(content MATCHES "session_lock_state" OR content MATCHES "QtWayland"
       OR content MATCHES "wayland-client" OR content MATCHES "FileJournalStore"
       OR content MATCHES "KWin/" OR content MATCHES "KScreen")
        message(FATAL_ERROR "Private platform dependency escaped D6: ${header}")
    endif()
endforeach()

file(GLOB_RECURSE runtime_sources LIST_DIRECTORIES false
     "${runtime}/*.h" "${runtime}/*.hh" "${runtime}/*.hpp" "${runtime}/*.hxx"
     "${runtime}/*.c" "${runtime}/*.cc" "${runtime}/*.cpp" "${runtime}/*.cxx")
foreach(source IN LISTS runtime_sources)
    file(READ "${source}" content)
    if(content MATCHES "#include[ ]*[<\"]kwin" OR content MATCHES "KWin::"
       OR content MATCHES "KScreen::" OR content MATCHES "kscreen-doctor")
        message(FATAL_ERROR "Private/oracle dependency entered D6: ${source}")
    endif()
endforeach()

# D2 remains independently injectable: process composition, files, Wayland,
# and platform session authority belong to D6/D4/D5, not display_service.
file(GLOB_RECURSE service_sources LIST_DIRECTORIES false
     "${service}/include/*.h" "${service}/src/*.h" "${service}/src/*.cpp")
foreach(source IN LISTS service_sources)
    file(READ "${source}" content)
    if(content MATCHES "display_runtime" OR content MATCHES "display_writer"
       OR content MATCHES "display_journal" OR content MATCHES "session_lock_state"
       OR content MATCHES "QtWayland" OR content MATCHES "wayland-client"
       OR content MATCHES "org.freedesktop.login1")
        message(FATAL_ERROR "D6 dependency leaked into D2: ${source}")
    endif()
endforeach()

file(READ "${runtime}/app/main.cpp" process_source)
foreach(required IN ITEMS "FileJournalStore" "journalStore->load()"
                          "makeProductionOutputManagementPort"
                          "makeQtSessionSafetyPort" "runtime.start()")
    if(NOT process_source MATCHES "${required}")
        message(FATAL_ERROR "Packaged Display1 omits required composition: ${required}")
    endif()
endforeach()
if(process_source MATCHES "UnavailableTransactionPort")
    message(FATAL_ERROR "Packaged Display1 retained the non-mutating placeholder")
endif()
string(FIND "${process_source}" "journalStore->load()" load_position)
string(FIND "${process_source}" "makeProductionOutputManagementPort" writer_position)
string(FIND "${process_source}" "runtime.start()" start_position)
if(load_position LESS 0 OR writer_position LESS load_position
   OR start_position LESS writer_position)
    message(FATAL_ERROR "Packaged Display1 startup order is not load -> writer -> start")
endif()

message(STATUS "Display runtime composition and D2/D6 boundary verified")
