// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio_service_object_p.h"
#include <QtCore/QScopedValueRollback>
#include <QtDBus/QDBusMessage>

namespace QindaQt::BluetoothRadio {
RadioServiceObject::RadioServiceObject(RadioOperation &operation) : m_operation(operation) {}
QString RadioServiceObject::introspect(const QString &) const {
    return QStringLiteral(R"(<interface name="org.qindaqt.BluetoothRadio1">
<method name="ObserveAndUnblock"><arg type="(ssssst)" direction="in"/>
<arg type="(sus)" direction="out"/></method></interface>)");
}
bool RadioServiceObject::handleMessage(const QDBusMessage &message,
    const QDBusConnection &connection) {
    if (message.interface() != QLatin1String(kInterface)
        || message.member() != QLatin1String("ObserveAndUnblock")) return false;
    if (message.type() != QDBusMessage::MethodCallMessage
        || message.signature() != QLatin1String("(ssssst)")
        || message.arguments().size() != 1) {
        connection.send(message.createErrorReply(
            QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
            QStringLiteral("One bounded selected-adapter request is required")));
        return true;
    }
    const auto request = qdbus_cast<Request>(message.arguments().constFirst());
    Result result{request.nonce, Disposition::Refused, QStringLiteral("radio-busy")};
    if (!m_busy) {
        QScopedValueRollback<bool> active(m_busy, true);
        result = m_operation.execute(message.service(), request);
    }
    connection.send(message.createReply(QVariantList{QVariant::fromValue(result)}));
    return true;
}
}
