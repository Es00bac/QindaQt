// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_types.h"

#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QLocale>

namespace QindaQt::Apps::RemovableMedia {
namespace {
const QString Block = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString Drive = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString Filesystem = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString Encrypted = QStringLiteral("org.freedesktop.UDisks2.Encrypted");
const QString Partition = QStringLiteral("org.freedesktop.UDisks2.Partition");

QString bytePath(const QVariant &value)
{
    QByteArray bytes = qdbus_cast<QByteArray>(value);
    if (bytes.endsWith('\0')) bytes.chop(1);
    return QString::fromUtf8(bytes);
}

bool configurationPresent(const QVariant &value)
{
    if (value.metaType() != QMetaType::fromType<QDBusArgument>())
        return !value.toList().isEmpty();
    const auto argument = qvariant_cast<QDBusArgument>(value);
    argument.beginArray();
    const bool present = !argument.atEnd();
    argument.endArray();
    return present;
}
} // namespace

void registerMediaDBusTypes()
{
    qDBusRegisterMetaType<Interfaces>();
    qDBusRegisterMetaType<ManagedObjects>();
    qDBusRegisterMetaType<QList<QByteArray>>();
}

QString objectPath(const QVariant &value)
{
    return qdbus_cast<QDBusObjectPath>(value).path();
}

QString physicalMediaIdentity(const QString &path, const QVariantMap &drive)
{
    return path + QLatin1Char('|') + drive.value(QStringLiteral("Id")).toString()
        + QLatin1Char('|') + QString::number(drive.value(QStringLiteral("TimeDetected")).toULongLong())
        + QLatin1Char('|') + QString::number(drive.value(QStringLiteral("TimeMediaDetected")).toULongLong());
}

QVector<Volume> projectVolumes(const ManagedObjects &objects)
{
    QVector<Volume> result;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const auto &interfaces = it.value();
        if (!interfaces.contains(Block)) continue;
        const QVariantMap block = interfaces.value(Block);
        if (block.value(QStringLiteral("HintIgnore")).toBool()
            || block.value(QStringLiteral("HintSystem")).toBool()) continue;
        QString drivePath = objectPath(block.value(QStringLiteral("Drive")));
        const QString backingPath = objectPath(block.value(QStringLiteral("CryptoBackingDevice")));
        if (drivePath.isEmpty() || drivePath == QStringLiteral("/")) {
            drivePath = objectPath(objects.value(QDBusObjectPath(backingPath))
                                       .value(Block).value(QStringLiteral("Drive")));
        }
        const auto drive = objects.value(QDBusObjectPath(drivePath)).value(Drive);
        const QString bus = drive.value(QStringLiteral("ConnectionBus")).toString();
        const bool removable = drive.value(QStringLiteral("Removable")).toBool()
            || drive.value(QStringLiteral("MediaRemovable")).toBool();
        const bool optical = drive.value(QStringLiteral("Optical")).toBool();
        if (drive.isEmpty() || (!removable && !optical && bus != QStringLiteral("usb")
            && bus != QStringLiteral("firewire") && bus != QStringLiteral("sdio")
            && bus != QStringLiteral("mmc") && bus != QStringLiteral("thunderbolt"))) continue;
        if (!drive.value(QStringLiteral("MediaAvailable")).toBool()) continue;
        const bool encrypted = interfaces.contains(Encrypted);
        bool hasChildren = false;
        bool unlocked = false;
        for (auto child = objects.cbegin(); child != objects.cend(); ++child) {
            if (objectPath(child.value().value(Partition).value(QStringLiteral("Table"))) == it.key().path())
                hasChildren = true;
            if (objectPath(child.value().value(Block).value(QStringLiteral("CryptoBackingDevice"))) == it.key().path())
                unlocked = true;
        }
        // AGENT-GUARD: never offer a partition-table disk as a format target.
        // UDisks Format can overwrite nested data even without tear-down.
        if (hasChildren || interfaces.contains(QStringLiteral("org.freedesktop.UDisks2.PartitionTable"))
            || (encrypted && unlocked)) continue;
        const QString usage = block.value(QStringLiteral("IdUsage")).toString();
        if (usage != QStringLiteral("filesystem") && !usage.isEmpty() && !encrypted) continue;

        Volume volume;
        volume.path = it.key().path();
        volume.drive = drivePath;
        volume.driveIdentity = physicalMediaIdentity(drivePath, drive);
        volume.cryptoBackingDevice = backingPath;
        volume.device = bytePath(block.value(QStringLiteral("PreferredDevice")));
        if (volume.device.isEmpty()) volume.device = bytePath(block.value(QStringLiteral("Device")));
        volume.size = block.value(QStringLiteral("Size")).toULongLong();
        volume.optical = optical;
        volume.readOnly = block.value(QStringLiteral("ReadOnly")).toBool() || optical;
        volume.encrypted = encrypted;
        volume.mountable = interfaces.contains(Filesystem);
        volume.canMountReadOnly = volume.mountable
            && !configurationPresent(block.value(QStringLiteral("Configuration")));
        volume.canEject = drive.value(QStringLiteral("Ejectable")).toBool();
        volume.canPowerOff = drive.value(QStringLiteral("CanPowerOff")).toBool();
        volume.canFormat = !volume.readOnly && !encrypted && !unlocked && !configurationPresent(block.value(QStringLiteral("Configuration")));
        const auto mounts = qdbus_cast<QList<QByteArray>>(interfaces.value(Filesystem)
                                                           .value(QStringLiteral("MountPoints")));
        if (!mounts.isEmpty()) volume.mountPath = bytePath(QVariant::fromValue(mounts.constFirst()));
        QString driveName = (drive.value(QStringLiteral("Vendor")).toString() + QLatin1Char(' ')
                             + drive.value(QStringLiteral("Model")).toString()).trimmed();
        volume.label = block.value(QStringLiteral("IdLabel")).toString();
        if (volume.label.isEmpty()) volume.label = block.value(QStringLiteral("HintName")).toString();
        if (volume.label.isEmpty()) volume.label = driveName.isEmpty() ? volume.device : driveName;
        if (volume.label.size() > 200) volume.label = volume.label.left(200);
        volume.kind = optical ? QStringLiteral("Optical disc")
            : encrypted ? QStringLiteral("Encrypted volume")
            : usage.isEmpty() ? QStringLiteral("Unformatted media")
            : block.value(QStringLiteral("IdType")).toString().toUpper();
        const QString uuid = block.value(QStringLiteral("IdUUID")).toString();
        const QString driveId = drive.value(QStringLiteral("Id")).toString();
        volume.identity = volume.path + QLatin1Char('|') + volume.driveIdentity + QLatin1Char('|') + uuid
            + QLatin1Char('|') + QString::number(volume.size) + QLatin1Char('|') + backingPath;
        // Remember only a filesystem on a named physical device. Device-node
        // reuse must never transfer an automatic choice to an unrelated disk.
        if (!uuid.isEmpty() && !driveId.isEmpty()) {
            volume.preferenceKey = QString::fromLatin1(QCryptographicHash::hash(
                (driveId + QLatin1Char('|') + uuid).toUtf8(), QCryptographicHash::Sha256).toHex());
        }
        result.append(volume);
    }
    return result;
}

QVariantMap volumeMap(const Volume &v, const QString &preference)
{
    return {{QStringLiteral("token"), v.token}, {QStringLiteral("device"), v.device},
        {QStringLiteral("label"), v.label}, {QStringLiteral("kind"), v.kind},
        {QStringLiteral("sizeText"), QLocale().formattedDataSize(static_cast<qint64>(v.size))},
        {QStringLiteral("mountPath"), v.mountPath}, {QStringLiteral("mounted"), !v.mountPath.isEmpty()},
        {QStringLiteral("mountable"), v.mountable}, {QStringLiteral("readOnly"), v.readOnly},
        {QStringLiteral("optical"), v.optical}, {QStringLiteral("encrypted"), v.encrypted},
        {QStringLiteral("canFormat"), v.canFormat}, {QStringLiteral("canEject"), v.canEject},
        {QStringLiteral("canPowerOff"), v.canPowerOff}, {QStringLiteral("canMountReadOnly"), v.canMountReadOnly},
        {QStringLiteral("canRemember"), !v.preferenceKey.isEmpty()}, {QStringLiteral("preference"), preference}};
}
} // namespace QindaQt::Apps::RemovableMedia
