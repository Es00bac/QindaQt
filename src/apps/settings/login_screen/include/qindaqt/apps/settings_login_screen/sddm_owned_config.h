// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace QindaQt::Apps::SettingsLoginScreen {

// AGENT-CONTRACT: the complete set of sddm.conf keys this route owns. Every
// write the route (or its privileged helper) ever performs is expressed as a
// change set of these five keys and nothing else; the helper rejects any
// payload naming a key outside this set, so the GUI and the root-side writer
// share exactly one whitelist. An empty std::optional means "not part of
// this change"; an empty string means "write the key with an empty value"
// (SDDM reads an empty Autologin/User as "autologin off", and an empty
// Autologin/Session as "no pinned session").
struct SddmOwnedChangeSet final {
  std::optional<QString> theme;           // [Theme] Current
  std::optional<QString> numlock;         // [General] Numlock: on|off|none
  std::optional<QString> cursorTheme;     // [Theme] CursorTheme
  std::optional<QString> autologinUser;   // [Autologin] User
  std::optional<QString> autologinSession; // [Autologin] Session (.desktop basename)

  [[nodiscard]] bool isEmpty() const noexcept {
    return !theme.has_value() && !numlock.has_value() &&
           !cursorTheme.has_value() && !autologinUser.has_value() &&
           !autologinSession.has_value();
  }
};

[[nodiscard]] bool isValidSddmNumlockValue(const QString &value) noexcept;

// Wire format between the Settings process and the privileged helper: a
// strict INI document holding only owned keys. serialize is what the client
// pipes to the helper's stdin; deserialize is the only parser the helper
// runs on that input, and it fails on any section or key outside the owned
// set so a crafted payload can never steer a root write elsewhere.
[[nodiscard]] QString
serializeOwnedChangeSet(const SddmOwnedChangeSet &changes);
[[nodiscard]] std::optional<SddmOwnedChangeSet>
deserializeOwnedChangeSet(const QString &text, QString *error);

// Pre-write validation shared by both sides: the GUI validates before it
// ever asks for elevation (a bad value is refused without a password
// prompt), and the helper re-validates after elevating because the caller
// is untrusted. `installedThemeIds` and `installedSessionIds` are the
// directory scan results; `loginUserNames` is the discoverable user list.
// Returns an error naming the offending value, or an empty string when the
// change set is acceptable.
[[nodiscard]] QString validateOwnedChangeSet(
    const SddmOwnedChangeSet &changes, const QStringList &installedThemeIds,
    const QStringList &installedSessionIds, const QStringList &loginUserNames);

// Patches `existingText` (the current content of the route-owned drop-in,
// possibly empty) so the keys in `changes` carry their new values, replacing
// an existing owned line in place or appending it under its section. Every
// other line -- comments, blank lines, and keys this route does not own --
// is preserved verbatim, the same line-patcher contract as
// xdg_autostart_store.cpp: this file is route-owned, but an operator may
// still have hand-edited it, and reformatting someone else's words is how
// trust in a configurator dies.
[[nodiscard]] QString
mergeOwnedChangeSetIntoConfigText(const QString &existingText,
                                  const SddmOwnedChangeSet &changes);

// Reads `targetPath` (absent file is fine), merges the change set, and
// writes the result back atomically (same-directory temporary file +
// rename) with 0644 permissions. Creating the parent directory is the
// caller's decision; this function refuses to.
[[nodiscard]] bool writeOwnedChangeSetToFile(const QString &targetPath,
                                             const SddmOwnedChangeSet &changes,
                                             QString *error);

} // namespace QindaQt::Apps::SettingsLoginScreen
