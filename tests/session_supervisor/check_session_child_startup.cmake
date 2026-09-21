# SPDX-License-Identifier: GPL-3.0-or-later
#
# A QindaQt session never activates `graphical-session.target`. It runs its own
# session supervisor and its own session bus, and nothing in the tree pulls that
# target in. So a systemd user unit whose only activation is
#
#     [Install]
#     WantedBy=graphical-session.target
#
# is started by NOTHING. It installs, it looks correct, `systemctl --user cat`
# prints it happily, and it never runs.
#
# That is not hypothetical. The XEmbed tray proxy (ADR-0229) shipped exactly
# that way: the service was built, installed and documented, while
# `_NET_SYSTEM_TRAY_S0` stayed unowned on the session's XWayland display and
# every Wine, Proton and Steam tray icon had nowhere to dock. The symptom was
# indistinguishable from "the proxy does not work".
#
# There is one legitimate way to name that target anyway: also ship a D-Bus
# activation file, so the bus starts the service on demand and the unit is only
# a stop-with-the-session hint. That is how the portal gets away with it. This
# check allows precisely that and nothing else.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-GUARD: one pattern, recursing from src, and no wildcard in the
# directory part. file(GLOB_RECURSE) recurses under the LITERAL leading
# directory of the expression; a wildcard there (src/*/data/*.service.in)
# silently matches nothing, which is how this check ran blind and still
# reported success until the count assertion below was reached with 0.
file(GLOB_RECURSE unit_templates "${SOURCE_ROOT}/src/*.service.in")
list(LENGTH unit_templates unit_count)
if(unit_count LESS 5)
    message(FATAL_ERROR
        "expected at least 5 systemd unit templates under src/*/data, found "
        "${unit_count} — units have moved and this check has gone blind")
endif()

set(violations "")
set(supervised 0)
set(bus_activated 0)
set(default_target 0)

foreach(unit IN LISTS unit_templates)
    get_filename_component(unit_name "${unit}" NAME)
    get_filename_component(unit_dir "${unit}" DIRECTORY)
    file(RELATIVE_PATH shown "${SOURCE_ROOT}" "${unit}")
    file(READ "${unit}" unit_source)

    # A D-Bus activation template is itself a unit template by filename, so
    # skip those: they are the activation, not the thing being activated.
    if(unit_name MATCHES "^org\\.")
        continue()
    endif()

    # AGENT-GUARD: anchored after a newline so a directive is matched and a
    # COMMENT is not. The tray proxy unit quotes the very directive it no
    # longer has, in the comment explaining why - an unanchored match read
    # that as the real thing and reported the fixed file as broken.
    if(NOT unit_source MATCHES "\nWantedBy=graphical-session\\.target")
        if(unit_source MATCHES "\nWantedBy=default\\.target")
            math(EXPR default_target "${default_target} + 1")
        else()
            math(EXPR supervised "${supervised} + 1")
        endif()
        continue()
    endif()

    # It names the dead target. The only acceptable reason is that the bus
    # starts it instead.
    file(GLOB activation "${unit_dir}/org.*.service.in")
    if(activation)
        math(EXPR bus_activated "${bus_activated} + 1")
        continue()
    endif()

    list(APPEND violations
         "${shown}: WantedBy=graphical-session.target, which QindaQt never "
         "activates, and no D-Bus activation file sits beside it — so nothing "
         "starts this service. Either give the session supervisor an optional "
         "child for it (see xembedTrayProxyExecutable) and drop the [Install] "
         "section, or ship a D-Bus activation file.")
endforeach()

# The tray proxy is the case that taught this lesson; pin its shape exactly so
# a well-meaning re-add is caught with the reason attached.
set(proxy_unit
    "${SOURCE_ROOT}/src/services/xembed_tray_proxy/data/qindaqt-xembed-tray-proxy.service.in")
if(NOT EXISTS "${proxy_unit}")
    message(FATAL_ERROR
        "the XEmbed tray proxy unit template has moved; update this check")
endif()
file(READ "${proxy_unit}" proxy_source)
if(proxy_source MATCHES "\n[ \t]*\\[Install\\]")
    list(APPEND violations
         "the XEmbed tray proxy unit has an [Install] section again. The "
         "session supervisor owns its startup; a second automatic owner means "
         "two trays racing for one selection, which ADR-0229 rates worse than "
         "one missing icon.")
endif()

# ... and that the supervisor really is that owner.
set(supervisor
    "${SOURCE_ROOT}/src/session_supervisor/src/session_process_supervisor.cpp")
file(READ "${supervisor}" supervisor_source)
if(NOT supervisor_source MATCHES "m_xembedTrayProxy->start\\(")
    list(APPEND violations
         "${supervisor}: nothing starts the XEmbed tray proxy. With no "
         "[Install] section in its unit either, the proxy would ship and never "
         "run — the exact defect this check exists for.")
endif()
foreach(required_stop "m_xembedTrayProxy->stop\\(" "m_globalShortcutDaemon->stop\\(")
    string(REGEX MATCHALL "${required_stop}" stop_matches "${supervisor_source}")
    list(LENGTH stop_matches stop_count)
    if(stop_count LESS 2)
        list(APPEND violations
             "${supervisor}: ${required_stop} appears ${stop_count} time(s); "
             "both the orderly stop() and the abnormal finishSession() paths "
             "must stop it. Each of these children owns a session singleton "
             "(the tray selection, KGlobalAccel's bus name), so a survivor "
             "makes the NEXT session quietly wrong.")
    endif()
endforeach()

if(violations)
    string(REPLACE ";" "\n  " rendered "${violations}")
    message(FATAL_ERROR "session child startup is unsound:\n  ${rendered}")
endif()

message(STATUS
    "session child startup sound: ${unit_count} unit template(s) — "
    "${default_target} wanted by default.target, ${bus_activated} D-Bus "
    "activated, ${supervised} supervisor-owned")
