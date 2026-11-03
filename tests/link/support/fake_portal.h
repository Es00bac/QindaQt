// SPDX-License-Identifier: GPL-3.0-or-later
// A fake org.freedesktop.portal.RemoteDesktop (+ Request/Session) for QindaLink
// tests. SAFETY: it registers under a caller-chosen UNIQUE service name and
// clients under test are pointed at that name through PortalOptions.service, so
// no test can ever reach the real portal or raise a real approval prompt, even
// if it is run outside dbus-run-session.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Link::Test {

class FakePortalSession;

class FakePortal : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.RemoteDesktop")
public:
    enum class Mode { Approve, DenyAtStart, HoldAtStart, FailCreate, RefuseSelect };

    static QDBusConnection connect()
    {
        return QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-portal"));
    }

    explicit FakePortal(QString serviceName) : m_service(std::move(serviceName)), m_bus(connect())
    {
        m_ok = m_bus.isConnected()
            && m_bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), this,
                                    QDBusConnection::ExportScriptableSlots)
            && m_bus.registerService(m_service);
    }
    ~FakePortal() override;
    [[nodiscard]] bool ok() const { return m_ok; }
    [[nodiscard]] const QString &service() const { return m_service; }
    Mode mode = Mode::Approve;
    QStringList calls;        // "Method:arg:arg" in arrival order
    QVariantMap lastSelect;   // options of the last SelectDevices
    int prompts = 0;          // Start calls that were not silently restored
    [[nodiscard]] bool sessionOpen() const { return m_open; }
    // Emits Session.Closed as the compositor would when the user revokes access.
    void revoke();

public Q_SLOTS:
    Q_SCRIPTABLE QDBusObjectPath CreateSession(const QVariantMap &options);
    Q_SCRIPTABLE QDBusObjectPath SelectDevices(const QDBusObjectPath &session, const QVariantMap &options);
    Q_SCRIPTABLE QDBusObjectPath Start(const QDBusObjectPath &session, const QString &parent, const QVariantMap &options);
    Q_SCRIPTABLE void NotifyPointerMotion(const QDBusObjectPath &s, const QVariantMap &o, double dx, double dy);
    Q_SCRIPTABLE void NotifyPointerButton(const QDBusObjectPath &s, const QVariantMap &o, int button, uint state);
    Q_SCRIPTABLE void NotifyPointerAxis(const QDBusObjectPath &s, const QVariantMap &o, double dx, double dy);
    Q_SCRIPTABLE void NotifyPointerAxisDiscrete(const QDBusObjectPath &s, const QVariantMap &o, uint axis, int steps);
    Q_SCRIPTABLE void NotifyKeyboardKeysym(const QDBusObjectPath &s, const QVariantMap &o, int keysym, uint state);

public:
    void sessionClosedByClient();

private:
    QDBusObjectPath requestPath(const QVariantMap &options) const;
    void respond(const QDBusObjectPath &request, uint response, const QVariantMap &results);

    QString m_service;
    QDBusConnection m_bus;
    bool m_ok = false;
    bool m_open = false;
    QString m_sessionPath;
    FakePortalSession *m_session = nullptr;
};

// org.freedesktop.portal.Session at the session handle path.
class FakePortalSession : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
public:
    explicit FakePortalSession(FakePortal &portal) : m_portal(portal) {}
public Q_SLOTS:
    Q_SCRIPTABLE void Close() { m_portal.sessionClosedByClient(); }
private:
    FakePortal &m_portal;
};

inline FakePortal::~FakePortal()
{
    // The named connection outlives this object; leave nothing registered on it.
    m_bus.unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"), QDBusConnection::UnregisterTree);
    m_bus.unregisterService(m_service);
    delete m_session;
    QDBusConnection::disconnectFromBus(QStringLiteral("fake-portal"));
}

inline QDBusObjectPath FakePortal::requestPath(const QVariantMap &options) const
{
    QString sender = message().service();
    sender.remove(QLatin1Char(':')).replace(QLatin1Char('.'), QLatin1Char('_'));
    return QDBusObjectPath(QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
                               .arg(sender, options.value(QStringLiteral("handle_token")).toString()));
}

inline void FakePortal::respond(const QDBusObjectPath &request, uint response, const QVariantMap &results)
{
    // Sent before the method reply on purpose: a client that subscribes late misses it.
    QDBusMessage sig = QDBusMessage::createSignal(request.path(), QStringLiteral("org.freedesktop.portal.Request"),
                                                  QStringLiteral("Response"));
    sig << response << results;
    m_bus.send(sig);
}

inline QDBusObjectPath FakePortal::CreateSession(const QVariantMap &options)
{
    calls << QStringLiteral("CreateSession");
    const QDBusObjectPath req = requestPath(options);
    if (mode == Mode::FailCreate) {
        respond(req, 2, {});
        return req;
    }
    QString sender = message().service();
    sender.remove(QLatin1Char(':')).replace(QLatin1Char('.'), QLatin1Char('_'));
    m_sessionPath = QStringLiteral("/org/freedesktop/portal/desktop/session/%1/%2")
                        .arg(sender, options.value(QStringLiteral("session_handle_token")).toString());
    if (m_session != nullptr) m_bus.unregisterObject(m_sessionPath);
    delete m_session;
    m_session = new FakePortalSession(*this);
    m_bus.registerObject(m_sessionPath, m_session, QDBusConnection::ExportScriptableSlots);
    m_open = true;
    respond(req, 0, {{QStringLiteral("session_handle"), m_sessionPath}});
    return req;
}

inline QDBusObjectPath FakePortal::SelectDevices(const QDBusObjectPath &, const QVariantMap &options)
{
    calls << QStringLiteral("SelectDevices:types=%1:persist=%2")
                 .arg(options.value(QStringLiteral("types")).toUInt())
                 .arg(options.value(QStringLiteral("persist_mode")).toUInt());
    lastSelect = options;
    const QDBusObjectPath req = requestPath(options);
    respond(req, mode == Mode::RefuseSelect ? 2u : 0u, {});
    return req;
}

inline QDBusObjectPath FakePortal::Start(const QDBusObjectPath &, const QString &, const QVariantMap &options)
{
    calls << QStringLiteral("Start");
    const QDBusObjectPath req = requestPath(options);
    if (!lastSelect.contains(QStringLiteral("restore_token"))) ++prompts;
    if (mode == Mode::HoldAtStart) return req;
    if (mode == Mode::DenyAtStart) {
        respond(req, 1, {});
        return req;
    }
    respond(req, 0, {{QStringLiteral("devices"), 3u}, {QStringLiteral("restore_token"), QStringLiteral("fake-token-1")}});
    return req;
}

inline void FakePortal::NotifyPointerMotion(const QDBusObjectPath &, const QVariantMap &, double dx, double dy)
{
    calls << QStringLiteral("NotifyPointerMotion:%1:%2").arg(dx).arg(dy);
}
inline void FakePortal::NotifyPointerButton(const QDBusObjectPath &, const QVariantMap &, int button, uint state)
{
    calls << QStringLiteral("NotifyPointerButton:%1:%2").arg(button).arg(state);
}
inline void FakePortal::NotifyPointerAxis(const QDBusObjectPath &, const QVariantMap &o, double dx, double dy)
{
    calls << QStringLiteral("NotifyPointerAxis:%1:%2:%3").arg(dx).arg(dy).arg(o.value(QStringLiteral("finish")).toBool());
}
inline void FakePortal::NotifyPointerAxisDiscrete(const QDBusObjectPath &, const QVariantMap &, uint axis, int steps)
{
    calls << QStringLiteral("NotifyPointerAxisDiscrete:%1:%2").arg(axis).arg(steps);
}
inline void FakePortal::NotifyKeyboardKeysym(const QDBusObjectPath &, const QVariantMap &, int keysym, uint state)
{
    calls << QStringLiteral("NotifyKeyboardKeysym:%1:%2").arg(keysym).arg(state);
}

inline void FakePortal::revoke()
{
    m_open = false;
    m_bus.send(QDBusMessage::createSignal(m_sessionPath, QStringLiteral("org.freedesktop.portal.Session"),
                                          QStringLiteral("Closed")) << QVariantMap{});
}

inline void FakePortal::sessionClosedByClient()
{
    m_open = false;
    calls << QStringLiteral("Close");
}

} // namespace QindaQt::Link::Test
