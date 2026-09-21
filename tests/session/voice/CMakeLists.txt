# SPDX-License-Identifier: GPL-3.0-or-later

# The client half of the voice interop probe. Built unconditionally so a
# contract change breaks the build here, not only on a host that has a
# provider installed.
qt_add_executable(qindaqt_voice_probe voice_probe_main.cpp)
target_link_libraries(qindaqt_voice_probe PRIVATE QindaQt::VoiceClient Qt6::DBus)
qindaqt_enable_warnings(qindaqt_voice_probe)

# The cross-implementation row: a real provider on a private bus, driven by
# the production client. Skips (77) when no provider checkout is present.
add_test(
    NAME session.voice-interop
    COMMAND
        "${CMAKE_COMMAND}" -E env "PYTHONDONTWRITEBYTECODE=1"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/voice_interop_probe.py"
        --probe "$<TARGET_FILE:qindaqt_voice_probe>"
)
add_test(
    NAME session.voice-probe-syntax
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONPYCACHEPREFIX=${CMAKE_CURRENT_BINARY_DIR}/python-cache"
        "${Python3_EXECUTABLE}" -m py_compile
        "${CMAKE_CURRENT_SOURCE_DIR}/voice_interop_probe.py"
)
set_tests_properties(
    session.voice-interop session.voice-probe-syntax
    PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;QT_QPA_PLATFORM=offscreen"
        LABELS "session;voice;interop"
        SKIP_RETURN_CODE 77
        TIMEOUT 180
)
