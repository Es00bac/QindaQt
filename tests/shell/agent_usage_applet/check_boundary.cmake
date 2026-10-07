# SPDX-License-Identifier: GPL-3.0-or-later
file(GLOB_RECURSE applet_sources "${SOURCE_ROOT}/src/shell/agent_usage_applet/src/*"
                                "${SOURCE_ROOT}/src/shell/agent_usage_applet/qml/*")
foreach(source IN LISTS applet_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "QProcess|QFile|QDir|QStandardPaths|QNetwork|QDBus|agent_usage_collector|XMLHttpRequest|Qt[.]createQmlObject")
        message(FATAL_ERROR "Agent usage presentation crossed collection boundary: ${source}")
    endif()
endforeach()
