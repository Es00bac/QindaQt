# Focused regression evidence for the manager's desktop-identity correction
# (display-platform-architecture/1787922986). The executable's declared
# identity must stay bound to the installed org.qindaqt.Settings desktop
# entry: source declaration, entry Exec contract, and installation must all
# agree, or settings windows lose their product identity on Wayland.
set(settings_dir "${SETTINGS_SOURCE_DIR}")
set(main_cpp "${settings_dir}/main.cpp")
set(desktop_entry "${settings_dir}/org.qindaqt.Settings.desktop")
set(cmake_lists "${settings_dir}/CMakeLists.txt")

foreach(artifact IN ITEMS main_cpp desktop_entry cmake_lists)
    if(NOT EXISTS "${${artifact}}")
        message(FATAL_ERROR "missing settings artifact: ${${artifact}}")
    endif()
endforeach()

file(READ "${main_cpp}" main_source)
if(NOT main_source MATCHES "setDesktopFileName\\(QStringLiteral\\(\"org\\.qindaqt\\.Settings\"\\)\\)")
    message(FATAL_ERROR
        "qindaqt-settings does not declare the installed desktop identity; "
        "expected setDesktopFileName(QStringLiteral(\"org.qindaqt.Settings\")) in main.cpp")
endif()

# AGENT-GUARD: The declaration must precede engine/window creation, not just
# exist somewhere in main().
string(FIND "${main_source}" "setDesktopFileName(QStringLiteral(\"org.qindaqt.Settings\"))" identity_offset)
string(FIND "${main_source}" "QQmlApplicationEngine engine" engine_offset)
if(identity_offset LESS 0 OR engine_offset LESS 0 OR identity_offset GREATER engine_offset)
    message(FATAL_ERROR "desktop identity must be declared before window creation in main.cpp")
endif()

file(READ "${desktop_entry}" desktop_source)
if(NOT desktop_source MATCHES "\nExec=qindaqt-settings\n")
    message(FATAL_ERROR "org.qindaqt.Settings.desktop must Exec the qindaqt-settings binary")
endif()

file(READ "${cmake_lists}" lists_source)
if(NOT lists_source MATCHES "org\\.qindaqt\\.Settings\\.desktop")
    message(FATAL_ERROR "settings CMakeLists must install org.qindaqt.Settings.desktop")
endif()

# Non-vacuous compiled evidence: the built executable must actually embed the
# identity constant, so a stale source declaration cannot pass this gate.
file(STRINGS "${SETTINGS_EXECUTABLE}" identity_strings REGEX "org\\.qindaqt\\.Settings")
if(NOT identity_strings)
    message(FATAL_ERROR
        "built qindaqt-settings does not embed the org.qindaqt.Settings identity")
endif()

message(STATUS "settings desktop identity contract verified: org.qindaqt.Settings")
