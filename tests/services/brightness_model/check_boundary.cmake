# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE brightness_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/brightness_model/*.h"
    "${SOURCE_ROOT}/src/services/brightness_model/*.cpp"
)
list(APPEND brightness_sources "${SOURCE_ROOT}/src/services/brightness_model/CMakeLists.txt")

foreach(source IN LISTS brightness_sources)
    file(READ "${source}" content)
    if(content MATCHES "qindaqt/services/display_" OR content MATCHES "QindaQt::Display"
       OR content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtQuick/"
       OR content MATCHES "<QtCore/Q(File|Settings|Timer|ElapsedTimer|Object|Process|Thread)")
        message(FATAL_ERROR "Forbidden brightness-model dependency in ${source}")
    endif()
endforeach()

message(STATUS "Brightness model boundary is display-transport and presentation independent")
