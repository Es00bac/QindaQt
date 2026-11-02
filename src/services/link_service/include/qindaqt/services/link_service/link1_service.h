// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_adb/tunnel_manager.h>
#include <qindaqt/services/link_messages/messages_adaptor.h>
#include <qindaqt/services/link_service/pairing_presenter.h>
#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>

#include <memory>

namespace QindaQt::Link {

struct ServiceConfig {
    QString dataDir;            // injected; production passes $XDG_DATA_HOME/qindalink
    quint16 port = 47160;       // 0 = ephemeral (tests)
    QString version;
};

// Exports org.qindaqt.Link1 at /org/qindaqt/Link1 (docs/wiki/reference/link1-v1.md)
// and owns identity, trust store, hub and, when given, the ADB tunnel manager.
//
// Ownership: the service owns everything except the injected presenter and
// runner, which the caller keeps alive. Threading: one thread (the bus
// connection's). Error behavior: start() returns false with `error` set when
// the identity, listener or bus name cannot be established.
// AGENT-CONTRACT: apps and the applet rely on DevicesChanged for every
// snapshot-visible change and on PairingRequested carrying the raw digits.
class Link1Service final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1")
    Q_CLASSINFO("D-Bus Introspection",
                "<interface name=\"org.qindaqt.Link1\">"
                "<method name=\"Devices\"><arg name=\"json\" type=\"s\" direction=\"out\"/></method>"
                "<method name=\"ConfirmPairing\"><arg name=\"id\" type=\"s\" direction=\"in\"/>"
                "<arg name=\"accept\" type=\"b\" direction=\"in\"/></method>"
                "<method name=\"Unpair\"><arg name=\"id\" type=\"s\" direction=\"in\"/></method>"
                "<method name=\"Ring\"><arg name=\"id\" type=\"s\" direction=\"in\"/></method>"
                "<method name=\"ListenPort\"><arg name=\"port\" type=\"q\" direction=\"out\"/></method>"
                "<signal name=\"DevicesChanged\"><arg name=\"json\" type=\"s\"/></signal>"
                "<signal name=\"PairingRequested\"><arg name=\"id\" type=\"s\"/><arg name=\"name\" type=\"s\"/>"
                "<arg name=\"model\" type=\"s\"/><arg name=\"code\" type=\"s\"/></signal>"
                "</interface>")
public:
    Link1Service(ServiceConfig config, QDBusConnection bus, PairingPresenter *presenter,
                 AdbRunner *adb, QObject *parent = nullptr);
    ~Link1Service() override;
    [[nodiscard]] bool start(QString *error);
    [[nodiscard]] Hub *hub() { return m_hub.get(); }

public Q_SLOTS:
    Q_SCRIPTABLE QString Devices() const;
    Q_SCRIPTABLE void ConfirmPairing(const QString &id, bool accept);
    Q_SCRIPTABLE void Unpair(const QString &id);
    Q_SCRIPTABLE void Ring(const QString &id);
    Q_SCRIPTABLE quint16 ListenPort() const;

Q_SIGNALS:
    Q_SCRIPTABLE void DevicesChanged(const QString &json);
    Q_SCRIPTABLE void PairingRequested(const QString &id, const QString &name, const QString &model,
                                       const QString &code);

private:
    void onDevicesChanged();

    ServiceConfig m_config;
    QDBusConnection m_bus;
    PairingPresenter *m_presenter;
    AdbRunner *m_adb;
    std::unique_ptr<TrustStore> m_store;
    // AGENT-GUARD: declared before m_hub so it is destroyed after the hub, whose sessions call
    // sessionClosed() on every registered feature while they shut down.
    std::unique_ptr<MessagesFeature> m_messages;
    std::unique_ptr<Hub> m_hub;
    std::unique_ptr<TunnelManager> m_tunnels;
    QStringList m_presented;
};

} // namespace QindaQt::Link
