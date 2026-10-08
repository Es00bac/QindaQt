// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QtCore/QObject>
#include <QtCore/QByteArray>
namespace QindaQt::Power {
// Single Qt thread, borrowed by PeripheralClient. bind/cancel invalidate any
// outstanding receipt; request never publishes from a method reply. Caller
// supplies the already-discovered unique Power1 owner (no activation/watcher).
class PeripheralTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void bind(const QString &owner) = 0;
    virtual void request(quint64 token) = 0;
    virtual void cancel() = 0;
Q_SIGNALS:
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void receipt(const QString &owner, quint64 token, const QByteArray &payload);
};
}
