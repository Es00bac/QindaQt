// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Apps::SettingsLoginScreen {

// The effective value of every key this route owns, merged the way SDDM
// itself merges it (man sddm.conf(5)): every file in the configuration
// directories in lexical order, then the legacy main file, later winning.
// An empty string means "not set anywhere" -- for Numlock SDDM's default
// is "none", for the others "unset".
struct SddmEffectiveConfig final {
  QString theme;
  QString numlock;
  QString cursorTheme;
  QString autologinUser;
  QString autologinSession;

  friend bool operator==(const SddmEffectiveConfig &,
                         const SddmEffectiveConfig &) = default;
};

// Read result: the merged values plus, for each owned key that is set
// anywhere, the file that last set it. The route uses the winner map for
// two truths: "this value comes from <file>" and, critically, "a file that
// sorts after this route's own drop-in overrides the choice the route
// wrote" -- the silent-failure case a configurator must never hide.
struct SddmConfigReadResult final {
  SddmEffectiveConfig values;
  // Key names are the SddmEffectiveConfig field tags: "theme", "numlock",
  // "cursorTheme", "autologinUser", "autologinSession".
  QHash<QString, QString> winnerFileByKey;
  // Files that were expected but unreadable (permissions, vanishing
  // mid-scan). Values from them are simply absent; this is reported, not
  // hidden.
  QStringList unreadableFiles;
  // Owned keys that the route's own drop-in sets but that a later file
  // (higher precedence) sets too -- the write "succeeded" yet SDDM will use
  // someone else's value. The page must say so; a configurator that
  // silently loses is worse than one that cannot write at all.
  QStringList shadowedKeys;
  // The later files doing the shadowing, for the message.
  QStringList shadowingFiles;
};

// `scanDirectoriesInOrder` are the sddm.conf.d directories from lowest to
// highest precedence (production: /usr/lib/sddm/sddm.conf.d, then
// /etc/sddm.conf.d); `legacyMainFile` is /etc/sddm.conf, which SDDM reads
// last and therefore wins over every drop-in. All paths are injected so
// tests never touch the real /etc.
// `ownedDropInFile` (optional) is the file this route writes: keys it sets
// that a later file also sets are reported in shadowedKeys/shadowingFiles.
[[nodiscard]] SddmConfigReadResult
readSddmConfig(const QStringList &scanDirectoriesInOrder,
               const QString &legacyMainFile,
               const QString &ownedDropInFile = QString());

} // namespace QindaQt::Apps::SettingsLoginScreen
