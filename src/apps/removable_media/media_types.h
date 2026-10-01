// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Apps::RemovableMedia {

using Interfaces = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, Interfaces>;

struct Volume final {
    QString token, path, drive, device, label, kind, identity, driveIdentity, preferenceKey;
    QString mountPath, cryptoBackingDevice;
    quint64 size = 0;
    bool mountable = false;
    bool readOnly = false;
    bool optical = false;
    bool encrypted = false;
    bool canFormat = false;
    bool canEject = false;
    bool canPowerOff = false;
    bool canMountReadOnly = false;
    bool operator==(const Volume &) const = default;
};

enum class Operation { Mount, MountReadOnly, Unmount, Remove, Format, Unlock };
struct Request final {
    QString token;
    Operation operation = Operation::Mount;
    QString filesystem, label, passphrase;
};

// Pure UDisks inventory projection. Tokens are assigned by the owning backend
// per observed attachment, never derived from reusable /dev names.
[[nodiscard]] QVector<Volume> projectVolumes(const ManagedObjects &objects);
[[nodiscard]] QVariantMap volumeMap(const Volume &volume, const QString &preference);
[[nodiscard]] QString objectPath(const QVariant &value);
[[nodiscard]] QString physicalMediaIdentity(const QString &path, const QVariantMap &drive);
void registerMediaDBusTypes();

} // namespace QindaQt::Apps::RemovableMedia
Q_DECLARE_METATYPE(QindaQt::Apps::RemovableMedia::Interfaces)
Q_DECLARE_METATYPE(QindaQt::Apps::RemovableMedia::ManagedObjects)
