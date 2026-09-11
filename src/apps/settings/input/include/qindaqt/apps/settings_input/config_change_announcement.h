// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArrayList>
#include <QDBusConnection>
#include <QHash>
#include <QString>

namespace QindaQt::Apps::SettingsInput {

// Announces a completed write of one KDE configuration file the way KConfig
// does for configs opened by bare name: the org.kde.kconfig.notify
// ConfigChanged signal on object path "/<file name>" carrying the changed
// groups and keys. KWin's config watchers re-read kcminputrc and kxkbrc on
// this signal and apply repeat and layout changes to the running session.
//
// AGENT-NOTE: KConfig announces nothing for a config opened by absolute path
// (checked on a private bus with kwriteconfig6), and the route opens injected
// absolute paths so tests can relocate them. Without this announcement a
// running KWin keeps the old values until the next session (ADR-0134).
//
// Returns false when the bus is not connected, nothing changed, the file
// name cannot form a D-Bus object path element, or the signal was not queued.
[[nodiscard]] bool
announceConfigChange(const QDBusConnection &bus, const QString &configFilePath,
                     const QHash<QString, QByteArrayList> &changedKeys);

} // namespace QindaQt::Apps::SettingsInput
