// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QSet>
#include <QtCore/QString>

namespace QindaQt::Link {

// Per-app mute list and the apps seen so far, persisted atomically in
// $XDG_CONFIG_HOME/qindalink/notifications.json:
//   {"version":1,"muted":["pkg"],"apps":{"pkg":"Label"}}
// Holds app packages and labels only, never notification content.
//
// Error behavior: an unreadable or malformed file loads as empty (the next
// successful save replaces it); save() returns false when the write fails
// and the in-memory state still applies for this run.
class AppSettings {
public:
    static constexpr qsizetype kMaxApps = 512;
    explicit AppSettings(QString path) : m_path(std::move(path)) { load(); }
    [[nodiscard]] bool isMuted(const QString &pkg) const { return m_muted.contains(pkg); }
    // Records an app the first time it is seen or when its label changed.
    // Returns true when something changed (and was saved).
    bool noteApp(const QString &pkg, const QString &label);
    bool setMuted(const QString &pkg, bool muted);
    // [{"pkg":..,"label":..,"muted":..}] sorted by label.
    [[nodiscard]] QJsonArray apps() const;

private:
    void load();
    bool save() const;

    QString m_path;
    QHash<QString, QString> m_apps;
    QSet<QString> m_muted;
};

} // namespace QindaQt::Link
