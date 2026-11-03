// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Link {

// service.json in the injected config directory. Today it holds only
// `lanEnabled` (default false, ADR-0434 item 6) and `lanInterfaces`, the optional interface-name allowlist
// (empty = every private-range interface; ADR-0441).
//
// AGENT-GUARD: writes are atomic (QSaveFile) and keep unknown keys, so a newer
// service's settings survive an older one rewriting the file. A missing or
// corrupt file reads as defaults; the next save repairs it. Never reads a
// default location: production passes $XDG_CONFIG_HOME/qindalink.
class LanSettings {
public:
    explicit LanSettings(QString directory) : m_directory(std::move(directory)) {}
    [[nodiscard]] bool lanEnabled() const;
    [[nodiscard]] bool setLanEnabled(bool enabled, QString *error = nullptr) const;
    [[nodiscard]] QStringList lanInterfaces() const;
    [[nodiscard]] bool setLanInterfaces(const QStringList &names, QString *error = nullptr) const;
    [[nodiscard]] QString path() const;

private:
    [[nodiscard]] bool setKey(const QString &key, const QJsonValue &value, QString *error) const;
    QString m_directory;
};

} // namespace QindaQt::Link
