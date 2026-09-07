# SPDX-License-Identifier: GPL-3.0-or-later

# Gabbee interoperability probe (see docs/wiki/development/gabbee-interop-evidence.md).
# Deterministic, host-safe rows: synthetic recorder/portal-staging/evidence-schema
# units plus the private-bus GlobalShortcuts registration/routing chain that runs
# Gabbee's real portal client against the real xdg-desktop-portal frontend.
add_test(
    NAME session.gabbee-interop-unit
    COMMAND
        "${CMAKE_COMMAND}" -E env "PYTHONDONTWRITEBYTECODE=1"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/test_gabbee_interop_unit.py"
)
add_test(
    NAME session.gabbee-probe-syntax
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONPYCACHEPREFIX=${CMAKE_CURRENT_BINARY_DIR}/python-cache"
        "${Python3_EXECUTABLE}" -m py_compile
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_probe_support.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_portal_fake.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_portal_driver.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/run_gabbee_portal_chain.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_interop_probe.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/run_gabbee_interop_nested.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/test_gabbee_interop_unit.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_sentinel.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_sink.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_portal.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_lane.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_boot.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/gabbee_terminal_delivery.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/run_gabbee_terminal_pty_proof.py"
)
# Terminal PTY proof units: the sentinel byte contract, the portal chooser
# predicate, and the adapter that drives Gabbee's real AgentInputTextSink.
# Host-safe and lane-free; the sandboxed proof itself is manager-run and is
# deliberately not a registered row (it needs the private runtime lane).
# The two real-sink contract cases skip when the Gabbee checkout is absent.
add_test(
    NAME session.gabbee-terminal-pty-unit
    COMMAND
        "${CMAKE_COMMAND}" -E env "PYTHONDONTWRITEBYTECODE=1"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/gabbee/test_gabbee_terminal_pty_proof_unit.py"
)
set_tests_properties(
    session.gabbee-interop-unit
    session.gabbee-probe-syntax
    session.gabbee-terminal-pty-unit
    PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
        LABELS "unit;portal;session"
        SKIP_RETURN_CODE 77
)
