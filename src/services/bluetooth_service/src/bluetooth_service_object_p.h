// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Bluetooth
{

class BluetoothServiceObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Bluetooth2")
    // Delayed-reply slots return void at the C++ ABI, so the canonical D-Bus
    // outputs must be explicit rather than inferred from the meta-object.
    // AGENT-GUARD: This introspection, data/org.qindaqt.Bluetooth2.xml, and
    // the codecs in bluetooth_dbus.cpp are one canonical Bluetooth2 v2 ABI.
    // A change to any of the three requires changing all of them and the
    // signature tests in the same commit.
    Q_CLASSINFO(
        "D-Bus Introspection",
        "<interface name=\"org.qindaqt.Bluetooth2\">"
        "<method name=\"GetSnapshot\"><arg name=\"snapshot\" "
        "type=\"(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnbyb)(tu(tt)ssq))\" direction=\"out\"/></method>"
        "<method name=\"SetPowered\"><arg name=\"adapter\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"powered\" type=\"b\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"AcquireDiscovery\"><arg name=\"adapter\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"ReleaseDiscovery\"><arg name=\"adapter\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"Connect\"><arg name=\"device\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"Disconnect\"><arg name=\"device\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"Pair\"><arg name=\"device\" type=\"(tt)\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"CancelPairing\"><arg name=\"device\" type=\"(tt)\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"Remove\"><arg name=\"device\" type=\"(tt)\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetTrusted\"><arg name=\"device\" type=\"(tt)\" direction=\"in\"/>"
        "<arg name=\"trusted\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"ReplyConfirmation\"><arg name=\"promptId\" type=\"t\" direction=\"in\"/>"
        "<arg name=\"accept\" type=\"b\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"ReplyPasskey\"><arg name=\"promptId\" type=\"t\" direction=\"in\"/>"
        "<arg name=\"passkey\" type=\"u\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"ReplyPin\"><arg name=\"promptId\" type=\"t\" direction=\"in\"/>"
        "<arg name=\"pin\" type=\"s\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"CancelPrompt\"><arg name=\"promptId\" type=\"t\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
        "name=\"revision\" type=\"t\"/></signal></interface>")

public:
    explicit BluetoothServiceObject(BluetoothModel *model, const QDBusConnection &connection,
                                    QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE QindaQt::Bluetooth::Snapshot GetSnapshot() const;
    Q_SCRIPTABLE void SetPowered(const QindaQt::Bluetooth::Handle &adapter, bool powered);
    Q_SCRIPTABLE void AcquireDiscovery(const QindaQt::Bluetooth::Handle &adapter);
    Q_SCRIPTABLE void ReleaseDiscovery(const QindaQt::Bluetooth::Handle &adapter);
    Q_SCRIPTABLE void Connect(const QindaQt::Bluetooth::Handle &device);
    Q_SCRIPTABLE void Disconnect(const QindaQt::Bluetooth::Handle &device);
    Q_SCRIPTABLE void Pair(const QindaQt::Bluetooth::Handle &device);
    Q_SCRIPTABLE void CancelPairing(const QindaQt::Bluetooth::Handle &device);
    Q_SCRIPTABLE void Remove(const QindaQt::Bluetooth::Handle &device);
    Q_SCRIPTABLE void SetTrusted(const QindaQt::Bluetooth::Handle &device, bool trusted);
    Q_SCRIPTABLE void ReplyConfirmation(quint64 promptId, bool accept);
    Q_SCRIPTABLE void ReplyPasskey(quint64 promptId, quint32 passkey);
    Q_SCRIPTABLE void ReplyPin(quint64 promptId, const QString &pin);
    Q_SCRIPTABLE void CancelPrompt(quint64 promptId);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);

private:
    void beginOperation(const OperationRequest &request);
    [[nodiscard]] Handle promptDevice(quint64 promptId) const;
    void finishOperation(quint64 operationId, const OperationResult &result);

    BluetoothModel *m_model = nullptr;
    QDBusConnection m_connection;
    QHash<quint64, QDBusMessage> m_pendingReplies;
};

} // namespace QindaQt::Bluetooth
