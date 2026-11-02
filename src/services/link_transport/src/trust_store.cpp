// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/trust_store.h>

#include <qindaqt/services/link_transport/private_dir.h>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace QindaQt::Link {

bool TrustStore::load()
{
    m_devices.clear();
    QFile f(QDir(m_directory).filePath(QStringLiteral("devices.json")));
    if (!f.exists()) {
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return false;
    }
    for (const QJsonValue &v : doc.object().value(QStringLiteral("devices")).toArray()) {
        const QJsonObject o = v.toObject();
        PairedDevice d;
        d.id = o.value(QStringLiteral("id")).toString();
        if (d.id.size() != 64) {
            continue;
        }
        d.name = o.value(QStringLiteral("name")).toString();
        d.model = o.value(QStringLiteral("model")).toString();
        d.pairedAt = qint64(o.value(QStringLiteral("pairedAt")).toDouble());
        d.lastSeen = qint64(o.value(QStringLiteral("lastSeen")).toDouble());
        m_devices.append(d);
    }
    return true;
}

std::optional<PairedDevice> TrustStore::find(const QString &id) const
{
    for (const PairedDevice &d : m_devices) {
        if (d.id == id) {
            return d;
        }
    }
    return std::nullopt;
}

bool TrustStore::upsert(const PairedDevice &device)
{
    // AGENT-GUARD: roll back the in-memory list when the write fails, so the
    // hub never trusts a key that is not on disk (protocol page, "Persisting the pin").
    const QList<PairedDevice> before = m_devices;
    bool replaced = false;
    for (PairedDevice &d : m_devices) {
        if (d.id == device.id) {
            d = device;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_devices.append(device);
    }
    if (!save()) {
        m_devices = before;
        return false;
    }
    return true;
}

bool TrustStore::remove(const QString &id)
{
    for (qsizetype i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == id) {
            const QList<PairedDevice> before = m_devices;
            m_devices.removeAt(i);
            if (!save()) {
                m_devices = before;
                return false;
            }
            return true;
        }
    }
    return true;
}

bool TrustStore::save() const
{
    QJsonArray array;
    for (const PairedDevice &d : m_devices) {
        array.append(QJsonObject{{QStringLiteral("id"), d.id},
                                 {QStringLiteral("name"), d.name},
                                 {QStringLiteral("model"), d.model},
                                 {QStringLiteral("pairedAt"), double(d.pairedAt)},
                                 {QStringLiteral("lastSeen"), double(d.lastSeen)}});
    }
    if (!ensurePrivateDirectory(m_directory)) {
        return false;
    }
    QSaveFile f(QDir(m_directory).filePath(QStringLiteral("devices.json")));
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                      {QStringLiteral("devices"), array}}).toJson());
    return f.commit();
}

} // namespace QindaQt::Link
