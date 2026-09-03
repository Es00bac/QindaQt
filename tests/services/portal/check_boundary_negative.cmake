# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS SOURCE_PORTAL_ROOT BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing portal boundary-negative input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_is_in_build)
if(NOT poison_is_in_build OR poison_root STREQUAL binary_root)
    message(FATAL_ERROR "Portal poison root must be a child of the build tree")
endif()

function(expect_rejection relative_path source_text expected)
    file(REMOVE_RECURSE "${poison_root}")
    file(COPY "${SOURCE_PORTAL_ROOT}/" DESTINATION "${poison_root}")
    cmake_path(GET relative_path PARENT_PATH parent)
    file(MAKE_DIRECTORY "${poison_root}/${parent}")
    file(WRITE "${poison_root}/${relative_path}" "${source_text}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DPORTAL_ROOT=${poison_root}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(status EQUAL 0)
        message(FATAL_ERROR "Portal boundary checker accepted poison ${relative_path}")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR "Portal checker rejected for wrong reason:\n${combined}")
    endif()
endfunction()

expect_rejection(
    "src/appearance_policy.cpp"
    "#include <QDBusArgument>"
    "Appearance policy imports transport"
)
expect_rejection(
    "src/poison.cpp"
    "#include <QtQuick/QQuickItem>"
    "presentation/platform/process authority"
)
expect_rejection(
    "src/poison.cpp"
    "#include <qindaqt/services/settings_service/settings_repository.h>"
    "prohibited product boundary"
)
expect_rejection(
    "data/qindaqt.portal"
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.OpenURI"
    "exact singleton Settings interface"
)
expect_rejection(
    "data/qindaqt-portals.conf"
    "[preferred]\norg.freedesktop.impl.portal.OpenURI=qindaqt"
    "exact Settings/fallback routing policy"
)
expect_rejection(
    "data/qindaqt.portal"
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.Background\nUseIn=QindaQt"
    "exact singleton Settings interface"
)
expect_rejection(
    "data/qindaqt-portals.conf"
    "[preferred]\ndefault=*\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Background=qindaqt"
    "exact Settings/fallback routing policy"
)
expect_rejection(
    "data/qindaqt.portal"
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.Settings\nUseIn=QindaQt"
    "exact singleton Settings interface"
)
expect_rejection(
    "data/qindaqt-portals.conf"
    "[preferred]\ndefault=*\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Settings=qindaqt"
    "exact Settings/fallback routing policy"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Portal source boundary checker rejected all injected poisons")
