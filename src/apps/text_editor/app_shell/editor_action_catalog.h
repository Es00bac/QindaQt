// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

namespace QindaQt::Apps::TextEditor {

// AGENT-CONTRACT: These string literals are the AppShell action-registry
// identity for the exact File/Edit/Tabs/Settings commands documented in
// docs/wiki/apps/text-editor.md. They are a separate identifier space from
// the stable QAction object names (fileNewAction, ...): ActionRegistry
// requires a lowercase dotted identifier shape, while the QAction object
// names are an already-published compatibility surface. Keep both stable;
// changing either is a documented application-contract change.
namespace AppShellActionIds {
inline constexpr const char *FileNew = "file.new";
inline constexpr const char *FileOpen = "file.open";
inline constexpr const char *FileCloseTab = "file.close-tab";
inline constexpr const char *FileSave = "file.save";
inline constexpr const char *FileSaveAs = "file.save-as";
inline constexpr const char *FileQuit = "file.quit";
inline constexpr const char *EditUndo = "edit.undo";
inline constexpr const char *EditRedo = "edit.redo";
inline constexpr const char *EditCut = "edit.cut";
inline constexpr const char *EditCopy = "edit.copy";
inline constexpr const char *EditPaste = "edit.paste";
inline constexpr const char *EditSelectAll = "edit.select-all";
inline constexpr const char *EditFind = "edit.find";
inline constexpr const char *EditReplace = "edit.replace";
inline constexpr const char *EditFindNext = "edit.find-next";
inline constexpr const char *EditFindPrevious = "edit.find-previous";
inline constexpr const char *EditFindClose = "edit.find-close";
inline constexpr const char *TabNext = "tabs.next";
inline constexpr const char *TabPrevious = "tabs.previous";
inline constexpr const char *RestoreDocuments = "settings.restore-documents";
} // namespace AppShellActionIds

// Builds the atomic ActionSpec replacement for the editor's documented S2
// command set. Pure and window-free so its shape can be
// checked against the real ActionRegistry validation without constructing a
// window. Enabled defaults match the widget actions' construction-time
// state; EditorWindow corrects the few actions whose true initial state
// depends on runtime input (for example clipboard availability) immediately
// after publishing this catalog.
[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> editorActionCatalog();

} // namespace QindaQt::Apps::TextEditor
