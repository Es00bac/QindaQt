# SPDX-License-Identifier: GPL-3.0-or-later

file(GLOB_RECURSE mutation_sources
     "${SOURCE_ROOT}/src/apps/file_manager/mutation/*.cpp"
     "${SOURCE_ROOT}/src/apps/file_manager/mutation/*.h")
if(NOT mutation_sources)
    message(FATAL_ERROR "No File Manager mutation sources found")
endif()

set(forbidden "src/shell|src/services|QDBus|KWin|LayerShell|QDesktopServices|QQml")
foreach(source IN LISTS mutation_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "${forbidden}")
        message(FATAL_ERROR "Mutation boundary violation in ${source}")
    endif()
endforeach()

# Mutation-sensitive negative control: the same matcher must reject a planted
# service dependency, otherwise a green source scan would be vacuous.
set(poison "#include \"src/services/settings_service/settings_service.h\"")
if(NOT poison MATCHES "${forbidden}")
    message(FATAL_ERROR "Mutation boundary poison was not detected")
endif()
