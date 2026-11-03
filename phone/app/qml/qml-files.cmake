# Single list of the phone UI's QML sources, shared by the Android app and the desktop preview host
# (phone/preview) so both load exactly the same files. Include it, then pass ${QINDALINK_QML_FILES}
# to qt_add_qml_module. The files are given by absolute path, so each gets a flat resource alias.
set(QINDALINK_QML_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(_names
    Main.qml ConnectionSection.qml ScreenFrame.qml HomeScreen.qml PairingScreen.qml PermissionsScreen.qml PermissionRow.qml
    InputStateNotice.qml ToolsScreen.qml TouchpadScreen.qml KeyboardScreen.qml MacrosScreen.qml VaultScreen.qml
    VaultSecretField.qml VaultGateView.qml VaultListView.qml VaultDetailView.qml VaultGeneratorPanel.qml
    VaultEditView.qml VaultBackupView.qml LinkText.js)
set(QINDALINK_QML_FILES "")
foreach(_n IN LISTS _names)
    set_source_files_properties("${QINDALINK_QML_DIR}/${_n}" PROPERTIES QT_RESOURCE_ALIAS "${_n}")
    list(APPEND QINDALINK_QML_FILES "${QINDALINK_QML_DIR}/${_n}")
endforeach()
