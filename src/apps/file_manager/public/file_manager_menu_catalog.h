// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringView>

#include <optional>

namespace QindaQt::Apps::FileManager::MenuCatalog {

// AGENT-CONTRACT (ADR-0260): the one definition of File Manager's menu
// vocabulary. File Manager's AppShell action catalog
// (app_shell/file_manager_action_catalog.cpp) and the shell's desktop menu
// (src/shell/desktop_menu: the File Manager's menu shown in the global menu
// while no application is active, as Finder's is on macOS) both project these
// values. Neither consumer may spell a shared label, menu title, description,
// or shortcut itself: a wording change lands here and reaches both, so the
// desktop and a File Manager window cannot drift apart.
//
// Pure values: no QObject, no I/O, no ambient state; safe on any thread.
// Only shortcutSequence() needs a QGuiApplication, and only for entries that
// name a platform standard key.

struct MenuDefinition final {
  QString id;
  QString label;
  int order = 0;

  friend bool operator==(const MenuDefinition &, const MenuDefinition &) = default;
};

// One key binding. When `standardKey` is set the platform theme resolves it
// (Undo, Cut, Copy, Paste, Zoom In/Out) exactly as File Manager always did;
// otherwise `text` is the key-sequence string ("Ctrl+Shift+N") File Manager
// has always handed to QKeySequence.
struct ShortcutDefinition final {
  std::optional<QKeySequence::StandardKey> standardKey;
  QString text;

  [[nodiscard]] bool isEmpty() const noexcept
  {
    return !standardKey.has_value() && text.isEmpty();
  }
  friend bool operator==(const ShortcutDefinition &, const ShortcutDefinition &) = default;
};

struct ActionDefinition final {
  QString id;
  QString menuId;
  QString label;
  QString description;
  ShortcutDefinition shortcut;
  int order = 0;
  bool destructive = false;
  bool checkable = false;

  friend bool operator==(const ActionDefinition &, const ActionDefinition &) = default;
};

// "File Manager": the application's short title (its desktop entry's
// GenericName), shown wherever the File Manager's menu stands for the desktop.
[[nodiscard]] QString applicationTitle();
// The desktop entry id and icon name File Manager installs.
[[nodiscard]] QString desktopEntryId();
[[nodiscard]] QString applicationIconName();

// The File, Edit, View, and Go menus in bar order.
[[nodiscard]] QList<MenuDefinition> menus();
[[nodiscard]] std::optional<MenuDefinition> findMenu(QStringView id);

// Every action a File Manager window offers, in File Manager's catalog order.
[[nodiscard]] QList<ActionDefinition> windowActions();
// File Manager commands that need no window of their own (today: opening a
// new window). File Manager's window menu does not offer them yet; the
// desktop menu does, under these exact words.
[[nodiscard]] QList<ActionDefinition> applicationActions();
// Searches windowActions() then applicationActions().
[[nodiscard]] std::optional<ActionDefinition> findAction(QStringView id);

[[nodiscard]] QKeySequence shortcutSequence(const ShortcutDefinition &shortcut);

} // namespace QindaQt::Apps::FileManager::MenuCatalog
