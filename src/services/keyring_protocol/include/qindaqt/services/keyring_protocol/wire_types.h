// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QMap>
#include <QString>
namespace qindaqt::keyring::protocol {
using MetadataRows = QList<QVariantMap>;
using Paths = QList<QDBusObjectPath>;
using StringMap = QMap<QString, QString>;
void wipe(QByteArray &) noexcept;
struct WireSecret {
    QDBusObjectPath session;
    QByteArray parameters, value;
    QString contentType;
    // Own copies are scrubbed even on rejected decoding. Qt/D-Bus may retain
    // independent framework copies; this is not a locked-memory claim.
    ~WireSecret() { wipe(value); }
};
using SecretMap = QMap<QDBusObjectPath, WireSecret>;
QDBusArgument &operator<<(QDBusArgument &, const WireSecret &);
const QDBusArgument &operator>>(const QDBusArgument &, WireSecret &);
void registerWireTypes();
template<class T> T argument(const QVariant &value) {
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<T>(value.value<QDBusArgument>());
    return qvariant_cast<T>(value);
}
}
Q_DECLARE_METATYPE(qindaqt::keyring::protocol::MetadataRows)
Q_DECLARE_METATYPE(qindaqt::keyring::protocol::WireSecret)
Q_DECLARE_METATYPE(qindaqt::keyring::protocol::StringMap)
Q_DECLARE_METATYPE(qindaqt::keyring::protocol::SecretMap)
Q_DECLARE_METATYPE(qindaqt::keyring::protocol::Paths)
