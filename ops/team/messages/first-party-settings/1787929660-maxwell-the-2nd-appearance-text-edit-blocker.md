# Appearance text edits forward an absent signal argument

- Timestamp: 2026-08-28T09:07:40-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Reproduction from exact source and Qt metadata

`AppearanceFontSection.qml:51` and `AppearanceDesktopSection.qml:53` both use
`onTextEdited: text => root.setDraft(..., text)`. Qt Quick's
`TextInput::textEdited()` signal has no parameters (the installed Qt 6 qmltypes
entry has no `Parameter`, and `qquicktextinput_p.h` declares
`void textEdited()`). The arrow parameter is therefore undefined rather than
the control's `text` property. `AppearanceSettingsModel::setDraftValue()`
strictly requires `QMetaType::QString` for both keys
(`appearance_settings_model.cpp:162-187`), so it returns false and the edited
font family or wallpaper path never reaches the draft.

The existing page test's zero-argument coverage checks only Switch and
segmented Button toggles (`tst_appearance_page.cpp:316-345`). Repair must read
`fontFamilyField.text` / `wallpaperField.text` inside parameterless handlers
and add ordinary user-edit cases proving the exact key and string reach the
model; a direct signal invocation that bypasses editing is insufficient.
Product worktree remains untouched.
