# SPDX-License-Identifier: BSD-3-Clause

include_guard(GLOBAL)

function(qindaqt_resolve_kwin_abi manifest_path output_variable)
    # AGENT-GUARD: KWin plugins have a patch-release binary ABI. This literal,
    # the immutable source manifest, find_package(EXACT), and PluginFactory_iid
    # must advance together only after the compositor matrix is rerun.
    set(_qindaqt_supported_kwin_abi "6.6.5")

    if(DEFINED QINDAQT_KWIN_ABI_VERSION
       AND NOT "${QINDAQT_KWIN_ABI_VERSION}" STREQUAL "${_qindaqt_supported_kwin_abi}")
        message(FATAL_ERROR
            "QINDAQT_KWIN_ABI_VERSION is not configurable: QindaQt supports "
            "KWin ${_qindaqt_supported_kwin_abi} exactly, but "
            "${QINDAQT_KWIN_ABI_VERSION} was requested"
        )
    endif()

    # Remove the former cache knob even when it contains the supported value;
    # subsequent reconfiguration must not present the ABI as user-selectable.
    unset(QINDAQT_KWIN_ABI_VERSION CACHE)

    file(READ "${manifest_path}" _qindaqt_kwin_manifest)
    string(JSON _qindaqt_manifest_release GET "${_qindaqt_kwin_manifest}" upstream release)
    if(NOT "${_qindaqt_manifest_release}" STREQUAL "${_qindaqt_supported_kwin_abi}")
        message(FATAL_ERROR
            "KWin source manifest release ${_qindaqt_manifest_release} does not match "
            "the supported plugin ABI ${_qindaqt_supported_kwin_abi}"
        )
    endif()

    set(${output_variable} "${_qindaqt_supported_kwin_abi}" PARENT_SCOPE)
endfunction()
