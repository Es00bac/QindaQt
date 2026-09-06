# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_SWITCHER_PACKAGE QINDAQT_QML_IMPORTS
                          QINDAQT_QMLLINT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(metadata "${QINDAQT_SWITCHER_PACKAGE}/metadata.json")
set(presentation "${QINDAQT_SWITCHER_PACKAGE}/contents/ui/main.qml")
set(appearance_bridge "${QINDAQT_SWITCHER_PACKAGE}/contents/ui/QindaQtAppearanceBridge.qml")
set(frame "${QINDAQT_SWITCHER_PACKAGE}/contents/ui/QindaQtSwitcherFrame.qml")
set(row "${QINDAQT_SWITCHER_PACKAGE}/contents/ui/QindaQtSwitcherRow.qml")
foreach(required_file IN ITEMS "${metadata}" "${presentation}" "${appearance_bridge}" "${frame}" "${row}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "window-switcher package omits ${required_file}")
    endif()
endforeach()

file(READ "${metadata}" metadata_json)
string(JSON package_structure ERROR_VARIABLE metadata_error
       GET "${metadata_json}" KPackageStructure)
if(metadata_error OR NOT package_structure STREQUAL "KWin/WindowSwitcher")
    message(FATAL_ERROR "window-switcher package has an invalid KPackageStructure")
endif()
string(JSON plugin_id ERROR_VARIABLE metadata_error
       GET "${metadata_json}" KPlugin Id)
if(metadata_error OR NOT plugin_id STREQUAL "qindaqt")
    message(FATAL_ERROR "window-switcher package has an invalid plugin ID")
endif()

file(READ "${presentation}" presentation_qml)
file(READ "${appearance_bridge}" appearance_bridge_qml)
file(READ "${frame}" frame_qml)
file(READ "${row}" row_qml)
string(CONCAT qml "${presentation_qml}" "${frame_qml}" "${row_qml}")
string(FIND "${presentation_qml}" "import QindaQt.Compositor.Appearance" hard_import)
if(NOT hard_import EQUAL -1)
    message(FATAL_ERROR "window-switcher root hard-imports optional appearance module")
endif()
string(FIND "${presentation_qml}" "appearanceBridge.status === Loader.Ready" fallback_guard)
string(FIND "${appearance_bridge_qml}" "import QindaQt.Compositor.Appearance" bridge_import)
if(fallback_guard EQUAL -1 OR bridge_import EQUAL -1)
    message(FATAL_ERROR "window-switcher omits optional appearance bridge fallback")
endif()
foreach(required_contract IN ITEMS
        "KWin.TabBoxSwitcher" "nativeModel: tabBox.model" "model: frame.nativeModel"
        "required property string caption"
        "required property var icon" "required property bool minimized"
        "tabBox.model.activate" "tabBox.model.rowCount()"
        "activationTimer.stop()" "visible: row.minimized"
        "text: qsTr(\"Minimized\")" "onCurrentIndexChanged")
    string(FIND "${qml}" "${required_contract}" contract_offset)
    if(contract_offset EQUAL -1)
        message(FATAL_ERROR
            "window-switcher presentation omits native-model contract: ${required_contract}")
    endif()
endforeach()

# KWin registers org.kde.kwin directly in its process and installs no qmltypes
# description. The fixture describes only that root type so qmllint can still
# validate all package-owned bindings, delegates, accessibility, and imports.
execute_process(
    COMMAND "${QINDAQT_QMLLINT}" --ignore-settings --max-warnings 0
            -I "${QINDAQT_QML_IMPORTS}" "${presentation}" "${frame}" "${row}"
    RESULT_VARIABLE lint_status
    OUTPUT_VARIABLE lint_output
    ERROR_VARIABLE lint_error
)
if(NOT lint_status EQUAL 0)
    message(FATAL_ERROR
        "window-switcher QML validation failed:\n${lint_output}${lint_error}")
endif()
