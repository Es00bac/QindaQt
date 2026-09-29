# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_KWIN_ABI_MODULE OR NOT DEFINED QINDAQT_KWIN_MANIFEST)
    message(FATAL_ERROR "ABI module and manifest paths are required")
endif()

include("${QINDAQT_KWIN_ABI_MODULE}")
qindaqt_resolve_kwin_abi("${QINDAQT_KWIN_MANIFEST}" resolved_abi)

# qindaqt-kwin fork release (ADR-0289).
if(NOT resolved_abi STREQUAL "6.6.6.1")
    message(FATAL_ERROR "resolved unexpected qindaqt-kwin ABI: ${resolved_abi}")
endif()
if(DEFINED CACHE{QINDAQT_KWIN_ABI_VERSION})
    message(FATAL_ERROR "the legacy KWin ABI cache entry survived resolution")
endif()
