// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace QindaQt::Shell::DesktopMenu {

// Every command the desktop menu can ask for. Each names exactly one existing
// shell controller boundary (ADR-0260 routing table); none carries a program,
// a path, a URL, or anything else that could be executed as given.
enum class DesktopCommand {
  AboutComputer,        // Settings route "about-computer"
  SystemSettings,       // SystemMenuController::openSettings()
  KeyboardShortcuts,    // Settings route "input", destination "shortcuts"
  LockScreen,           // session actions requestLock()
  LogOut,               // confirmation, then session actions requestLogout()
  Suspend,              // session actions requestSuspend()
  Restart,              // confirmation, then session actions requestReboot()
  ShutDown,             // confirmation, then session actions requestPowerOff()
  NewFileManagerWindow, // launcher activation of File Manager's desktop entry
  NewFolder,            // desktop-icons surface: New Folder (primary output)
  Find,                 // launcher popup (search) through requestOpen()
  Paste,                // desktop-icons surface: Paste (primary output)
  SelectAll,            // desktop-icons surface: Select All (every output)
  ClipboardHistory,     // clipboard applet popup through requestOpen()
  ShowDesktop,          // WorkspaceController::toggleShowingDesktop()
  GatherOverview,       // gather overview toggle (its one door)
  CleanUp,              // desktop-icons surface: Clean Up (primary output)
  OpenPlace,            // PlacesController::open(argument)
  SwitchWorkspace,      // WorkspaceController::switchTo(argument, revision)
  Help,                 // launcher activation of the Welcome desktop entry
  ShortcutNote,         // desktop shortcut note toggle (ADR-0084)
};

struct DesktopMenuCommand final {
  DesktopCommand kind = DesktopCommand::AboutComputer;
  // Place id (OpenPlace) or workspace id (SwitchWorkspace); otherwise empty.
  QString argument;
  // The workspace snapshot revision the item was built from, so a switch
  // requested from a stale menu fails the controller's own revision fence.
  quint64 revision = 0;

  friend bool operator==(const DesktopMenuCommand &, const DesktopMenuCommand &) = default;
};

// One capability as the menu sees it. Not `present` omits the entry (the
// route does not exist in this session or layout); `present` but not
// `enabled` shows it disabled (the owner exists but refuses right now).
struct Capability final {
  bool present = false;
  bool enabled = false;

  [[nodiscard]] static Capability available() { return {true, true}; }
  [[nodiscard]] static Capability disabled() { return {true, false}; }
  friend bool operator==(const Capability &, const Capability &) = default;
};

struct DesktopPlace final {
  QString id;
  QString label;

  friend bool operator==(const DesktopPlace &, const DesktopPlace &) = default;
};

struct DesktopWorkspace final {
  QString id;
  QString name;
  bool current = false;

  friend bool operator==(const DesktopWorkspace &, const DesktopWorkspace &) = default;
};

// The complete, value-only input of the desktop menu: what each owning
// controller can do right now. Built by the shell composition from borrowed
// controllers; the builder never reaches a controller itself.
struct DesktopMenuFacts final {
  Capability aboutComputer;
  Capability systemSettings;
  Capability keyboardShortcuts;
  Capability lockScreen;
  Capability logOut;
  Capability suspend;
  Capability restart;
  Capability shutDown;

  Capability newFileManagerWindow;
  Capability find;
  Capability help;

  // Desktop-icons surface commands: present only while a primary surface
  // is attached (layouts without desktop icons omit them).
  Capability newFolder;
  Capability paste;
  Capability selectAll;
  Capability cleanUp;

  Capability clipboardHistory;
  Capability showDesktop;
  bool showingDesktop = false;
  Capability gatherOverview;

  Capability places;
  QList<DesktopPlace> placeList;

  Capability workspaces;
  QList<DesktopWorkspace> workspaceList;
  quint64 workspaceRevision = 0;

  Capability shortcutNote;
  bool shortcutNoteVisible = false;

  friend bool operator==(const DesktopMenuFacts &, const DesktopMenuFacts &) = default;
};

} // namespace QindaQt::Shell::DesktopMenu
