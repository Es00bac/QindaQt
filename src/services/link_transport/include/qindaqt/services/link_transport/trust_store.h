// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::Link {

struct PairedDevice {
    QString id;     // 64 lowercase hex SPKI digest
    QString name;
    QString model;
    qint64 pairedAt = 0;  // ms since epoch
    qint64 lastSeen = 0;  // ms since epoch, 0 = never
};

// Paired-device pins in `<directory>/devices.json`. Single-threaded; the
// directory is injected (never a default location) and the file is written
// atomically with QSaveFile at 0600.
class TrustStore {
public:
    explicit TrustStore(QString directory) : m_directory(std::move(directory)) {}

    // Reads devices.json. A missing file is an empty store; a corrupt file
    // returns false and leaves the store empty.
    bool load();
    [[nodiscard]] bool contains(const QString &id) const { return find(id).has_value(); }
    [[nodiscard]] std::optional<PairedDevice> find(const QString &id) const;
    [[nodiscard]] const QList<PairedDevice> &devices() const { return m_devices; }
    bool upsert(const PairedDevice &device);
    bool remove(const QString &id);

private:
    bool save() const;
    QString m_directory;
    QList<PairedDevice> m_devices;
};

} // namespace QindaQt::Link
