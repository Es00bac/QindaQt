// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_public_projection.h"
#include <QCryptographicHash>

namespace QindaQt::Apps::RemovableMedia {
namespace Public = QindaQt::RemovableMedia;
namespace {
QString id(const QString &epoch, const QString &kind, const QString &identity)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        (epoch + QLatin1Char('|') + kind + QLatin1Char('|') + identity).toUtf8(),
        QCryptographicHash::Sha256).toHex());
}
QString displayText(QString text)
{
    text.remove(QChar::Null);
    while (text.toUtf8().size() > Public::kMaxLabelUtf8Bytes) {
        // Remove a complete supplementary scalar, never leave half a surrogate.
        text.chop(text.size() > 1 && text.at(text.size() - 1).isLowSurrogate()
            && text.at(text.size() - 2).isHighSurrogate() ? 2 : 1);
    }
    return text;
}
}
Public::VolumeRow publicVolume(const Volume &v, const QString &epoch, bool busy)
{
    Public::VolumeRow row;
    row.driveDisplayId = id(epoch, QStringLiteral("drive"), v.driveIdentity.isEmpty() ? v.drive : v.driveIdentity);
    row.volumeDisplayId = id(epoch, QStringLiteral("volume"), v.identity.isEmpty() ? v.token : v.identity);
    row.attachment = {id(epoch, QStringLiteral("attachment"), v.token), 1};
    row.displayName = displayText(v.label);
    row.kind = displayText(v.kind);
    row.partitionNumber = v.partitionNumber;
    row.sizeBytes = v.size;
    row.mountRoots = v.mountRoots;
    row.preferredRoot = v.mountPath;
    row.mountState = v.mountPath.isEmpty() ? Public::MountState::Unmounted : Public::MountState::Mounted;
    row.readOnly = v.readOnlyKnown ? (v.readOnly ? Public::ReadOnlyState::ReadOnly : Public::ReadOnlyState::Writable)
                                  : Public::ReadOnlyState::Unknown;
    row.encrypted = v.encrypted || (!v.cryptoBackingDevice.isEmpty() && v.cryptoBackingDevice != QStringLiteral("/"));
    row.locked = v.encrypted;
    row.optical = v.optical;
    row.actions.open = !busy && !v.mountPath.isEmpty()
        ? Public::ActionAvailability{true, Public::DisabledReason::None}
        : Public::ActionAvailability{false, busy ? Public::DisabledReason::Busy : Public::DisabledReason::NotMounted};
    // Inventory slice exports no action method. Later owner admission enables
    // these only when the operation/readback contract is implemented.
    return row;
}
}
