// SPDX-License-Identifier: GPL-3.0-or-later
#include "power_service_object_p.h"
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_dbus.h>
#include <QtCore/QRegularExpression>
namespace QindaQt::Power {
bool PowerServiceObject::RequestPeripheralSnapshotWithReceipt(const QString &nonce)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    if (!calledFromDBus() || !pattern.match(nonce).hasMatch()) return false;
    QByteArray payload;
    if (!encodePeripheralSnapshot(m_coordinator->peripheralSnapshot(),payload)) return false;
    // Only this targeted SignalMessage carries authority; method result is dispatch status.
    auto receipt=QDBusMessage::createTargetedSignal(message().service(),
        QString::fromLatin1(kObjectPath),QString::fromLatin1(kInterfaceName),
        QStringLiteral("PeripheralSnapshotReceipt"));
    receipt.setArguments({nonce,payload});
    return m_connection.send(receipt);
}
}
