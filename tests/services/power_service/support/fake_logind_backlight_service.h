// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QSemaphore>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusVirtualObject>

#include <optional>

namespace QindaQt::Tests {

// Wire record of org.freedesktop.login1.Manager.ListSessions, `a(susso)`.
struct FakeLogindSessionWire {
    QString sessionId;
    quint32 uid = 0;
    QString userName;
    QString seatId;
    QDBusObjectPath sessionPath;
};

QDBusArgument &operator<<(QDBusArgument &argument,
                          const FakeLogindSessionWire &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                FakeLogindSessionWire &value);

// Wire record of org.freedesktop.login1.Session.Seat, `(so)`.
struct FakeLogindSeatWire {
    QString seatId;
    QDBusObjectPath seatPath;
};

QDBusArgument &operator<<(QDBusArgument &argument, const FakeLogindSeatWire &value);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                FakeLogindSeatWire &value);

// A fake logind whose *session* surface is wire-faithful: ListSessions answers
// a(susso), each session object answers Properties.Get for Seat ((so)) and
// Active (b), and Session.SetBrightness(ssu) records its exact arguments or
// fails with a chosen D-Bus error.
//
// AGENT-NOTE: kept separate from support/fake_logind_service.h on purpose.
// That fake owns the Manager action/inhibitor surface for the session
// collaborator tests; mixing the two would couple two unrelated failure
// models into one fixture.
//
// AGENT-GUARD: LogindBacklightWriter makes *blocking* system-bus calls, which
// is correct for a resident service talking to a separate daemon but means a
// same-thread fake can never answer: Qt dispatches nothing while
// dbus_connection_send_with_reply_and_block waits. Drive this fake through
// FakeLogindBacklightServiceThread so its connection lives on another thread,
// and keep every member guarded by m_mutex because the test thread configures
// it while that thread serves calls.
class FakeLogindBacklightService final : public QDBusVirtualObject
{
public:
    struct SessionSpec {
        QString sessionId;
        quint32 uid = 0;
        QString userName;
        QString seatId;
        QString path;
        bool active = false;
    };

    struct BrightnessCall {
        QString sessionPath;
        QString subsystem;
        QString deviceName;
        quint32 value = 0;
    };

    explicit FakeLogindBacklightService(const QDBusConnection &connection,
                                        QObject *parent = nullptr);
    ~FakeLogindBacklightService() override;

    bool registerService();
    void unregisterService();

    // `auto` is the caller's own session. An empty seat here is the real
    // "no seat" condition an ssh shell or a system-scope unit sees.
    void setAutoSession(const QString &seatId, bool active);
    void setSessions(const QList<SessionSpec> &sessions);
    void setListSessionsFails(bool fails);
    // When set, SetBrightness answers with this error name/text instead of an
    // empty reply. `org.freedesktop.DBus.Error.UnknownObject` exercises the
    // stale-session re-resolve path.
    void setBrightnessError(const QString &errorName, const QString &errorText);
    void clearBrightnessError();
    // Stops answering SetBrightness for the named path, as a dead session
    // object would.
    void setUnknownSessionPath(const QString &path);

    [[nodiscard]] QList<BrightnessCall> brightnessCalls() const;
    [[nodiscard]] int listSessionsCalls() const;
    void resetCounters();

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;

private:
    void sendError(const QDBusMessage &message, const QString &name,
                   const QString &text);
    [[nodiscard]] std::optional<SessionSpec> sessionForPath(const QString &path) const;

    mutable QMutex m_mutex;
    QDBusConnection m_connection;
    QList<SessionSpec> m_sessions;
    QList<BrightnessCall> m_brightnessCalls;
    SessionSpec m_autoSession;
    QString m_brightnessErrorName;
    QString m_brightnessErrorText;
    QString m_unknownSessionPath;
    int m_listSessionsCalls = 0;
    bool m_autoSessionExists = true;
    bool m_listSessionsFails = false;
};

// Owns one connection to the private bus on its own thread, plus the fake
// registered on it. Construction blocks until the service is registered, so a
// test can configure and call immediately afterwards.
class FakeLogindBacklightServiceThread final
{
public:
    explicit FakeLogindBacklightServiceThread(const QString &busAddress);
    ~FakeLogindBacklightServiceThread();

    [[nodiscard]] bool isReady() const { return m_ready; }
    [[nodiscard]] FakeLogindBacklightService *service() const { return m_service; }

private:
    class Worker final : public QThread
    {
    public:
        Worker(QString address, FakeLogindBacklightServiceThread *owner)
            : m_address(std::move(address))
            , m_owner(owner)
        {
        }
        void run() override;

    private:
        QString m_address;
        FakeLogindBacklightServiceThread *m_owner;
    };

    friend class Worker;

    Worker m_worker;
    QSemaphore m_started;
    FakeLogindBacklightService *m_service = nullptr;
    QString m_connectionName;
    bool m_ready = false;
};

} // namespace QindaQt::Tests

Q_DECLARE_METATYPE(QindaQt::Tests::FakeLogindSessionWire)
Q_DECLARE_METATYPE(QindaQt::Tests::FakeLogindSeatWire)
