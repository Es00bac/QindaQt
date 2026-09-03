# SPDX-License-Identifier: GPL-3.0-or-later

# Mutation-sensitive guard for user-visible/documented Launcher L1 contracts
# that ordinary Markdown validation cannot distinguish from stale prose.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing launcher source root")
endif()
set(contract_failed FALSE)

function(require_text relative_path expected)
    file(READ "${SOURCE_ROOT}/${relative_path}" contents)
    string(FIND "${contents}" "${expected}" hit)
    if(hit EQUAL -1)
        message(STATUS
            "${relative_path} is missing required Launcher contract: ${expected}")
        set(contract_failed TRUE PARENT_SCOPE)
    endif()
endfunction()

function(forbid_text relative_path rejected)
    file(READ "${SOURCE_ROOT}/${relative_path}" contents)
    string(FIND "${contents}" "${rejected}" hit)
    if(NOT hit EQUAL -1)
        message(STATUS
            "${relative_path} retains rejected Launcher contract: ${rejected}")
        set(contract_failed TRUE PARENT_SCOPE)
    endif()
endfunction()

require_text(
    "docs/wiki/shell/applet-runtime.md"
    "production QML dispatcher renders all seven")
forbid_text(
    "docs/wiki/shell/applet-runtime.md"
    "hosting it in the production panel dispatcher is a later lane")

require_text(
    "src/shell/qml/BuiltinAppletContent.qml"
    "entryPoint === \"qindaqt.applets.launcher\"")
require_text(
    "src/shell/runtime/launcherappletcomposition.cpp"
    "Launcher::QProcessLaunchSpawner")
require_text(
    "src/shell/runtime/launcherappletcomposition.cpp"
    "Launcher::SessionBusActivator")

require_text(
    "docs/wiki/shell/launcher.md"
    "`/bin/true` and `/bin/false` are inert process-start fixtures only")
require_text(
    "docs/wiki/adr/0062-bound-launcher-execution-behind-injected-seams.md"
    "`/bin/true` and `/bin/false` fixture executables")
require_text(
    "tests/shell/launcher/tst_launch_executor.cpp"
    "inert fixtures /bin/true and /bin/false")

require_text(
    "src/shell/launcher/src/launcher_persistence.h"
    "every element must be a valid, unique")
require_text(
    "src/shell/launcher/src/launcher_persistence.h"
    "false for any shape, element, duplicate, or size violation")
forbid_text(
    "src/shell/launcher/src/launcher_persistence.h"
    "duplicates collapse keeping first occurrence")
forbid_text(
    "src/shell/launcher/src/launcher_persistence.h"
    "list is truncated at the model ceiling")

require_text(
    "src/shell/launcher/src/application_scanner.cpp"
    "return errno == ENOENT ? PathPresence::Missing : PathPresence::Indeterminate")
require_text(
    "tests/shell/launcher/tst_application_scanner.cpp"
    "root cannot reproduce ancestor traversal denial")
require_text(
    "docs/wiki/shell/launcher.md"
    "only a syscall-confirmed `ENOENT` is normal absence")
forbid_text(
    "src/shell/launcher/src/application_scanner.cpp"
    "if (!rootInfo.exists() && !rootInfo.isSymLink())")

if(contract_failed)
    message(FATAL_ERROR "Launcher contract text guard failed")
endif()
message(STATUS "Launcher documentation and safety-comment contracts passed")
