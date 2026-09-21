// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Apps::SettingsLoginScreen {

// One theme directory under the SDDM theme root. `id` is the directory name
// (what [Theme] Current= takes); `name` is the human name from
// metadata.desktop ([SddmGreeterTheme] Name=), falling back to the id when
// the file or the key is absent; `previewPath` is the theme's preview.png
// or screenshot.png when one exists (empty otherwise); `qindaqt` marks the
// themes the QindaThemes repository ships, recognised by the
// `.qindaqt-sddm-theme` marker file they install.
struct SddmThemeEntry final {
  QString id;
  QString name;
  QString previewPath;
  bool qindaqt = false;

  friend bool operator==(const SddmThemeEntry &,
                         const SddmThemeEntry &) = default;
};

// QindaQt themes first, then every other installed theme, each group
// alphabetical by name (case-insensitive). Directories without a Main.qml
// are not themes and are skipped.
[[nodiscard]] QList<SddmThemeEntry>
listSddmThemes(const QString &themesDirectory);

// One session .desktop entry. `id` is the file's basename (what
// [Autologin] Session= takes); `name` is the entry's Name=; `wayland`
// records which session directory family it came from.
struct SddmSessionEntry final {
  QString id;
  QString name;
  QString comment;
  bool wayland = false;

  friend bool operator==(const SddmSessionEntry &,
                         const SddmSessionEntry &) = default;
};

// Wayland sessions first, then X11 sessions, each alphabetical by name
// (case-insensitive). Entries with Hidden=true, no Name= or no Exec= are
// skipped -- SDDM would not offer them either.
[[nodiscard]] QList<SddmSessionEntry>
listSddmSessions(const QStringList &waylandSessionDirectories,
                 const QStringList &xSessionDirectories);

// Login-capable local users for the autologin picker: passwd entries with
// uid >= 1000 whose shell is not a known non-login shell (matching the
// floor SDDM's own user list applies by default). `passwdPath` is the only
// source -- no NSS, no getpwent -- so tests point it at a fixture.
[[nodiscard]] QStringList listSddmLoginUsers(const QString &passwdPath);

} // namespace QindaQt::Apps::SettingsLoginScreen
