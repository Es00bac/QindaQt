// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_adb/tunnel_manager.h>
#include <qindaqt/services/link_service/phone_setup.h>
#include <qindaqt/services/link_transport/caller_authority.h>

#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Link {

// org.qindaqt.Link1.Setup on the /org/qindaqt/Link1 object (docs/wiki/reference/link1-v1.md,
// "Setup"). The service owns ADB, so the app asks for the phone grants here instead of running adb.
//
// Replies are delayed: the adb steps finish later. Results are compact JSON text.
// A null runner (the service started with --no-adb) answers every method with an
// org.qindaqt.Link1.Error.NotConnected error.
// AGENT-GUARD (ADR-0438): every member changes or reveals phone state, so each asks the shared
// CallerAuthority first and fails with Error.Denied for an untrusted caller. A null authority denies.
class SetupAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Setup")
public:
    // `runner` and `tunnels` may be null and are borrowed; `exported` is the object the adaptor is added to.
    SetupAdaptor(AdbRunner *runner, TunnelManager *tunnels, QDBusConnection bus, QObject *exported,
                 CallerAuthority *authority);

public Q_SLOTS:
    QString AdbDevices(const QDBusMessage &msg);
    QString SetupPhone(const QString &serial, const QDBusMessage &msg);
    QString SetupStatus(const QString &serial, const QDBusMessage &msg);

private:
    void reply(const QDBusMessage &msg, const QString &json);
    void fail(const QDBusMessage &msg, const QString &code);

    PhoneSetup *m_setup = nullptr;  // null without a runner
    [[nodiscard]] bool allowed(const QDBusMessage &msg);

    TunnelManager *m_tunnels;
    CallerAuthority *m_authority;
    QDBusConnection m_bus;
};

} // namespace QindaQt::Link
