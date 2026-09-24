// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_session_apply_status_client.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>
#include <utility>

using namespace QindaQt::Apps::SettingsWindows;

namespace {
constexpr auto ServiceName = "org.qindaqt.WindowManagement1";
constexpr auto ObjectPath = "/org/qindaqt/WindowManagement1";

QVariantMap appliedState(const QString &focusPolicy = QStringLiteral("click"),
                         const QString &phase = QStringLiteral("applied"))
{
    return {{QStringLiteral("wireVersion"), quint32(1)},
            {QStringLiteral("phase"), phase},
            {QStringLiteral("settingsOwner"), QStringLiteral(":1.2")},
            {QStringLiteral("settingsEpoch"), QStringLiteral("epoch-a")},
            {QStringLiteral("settingsRevision"), QVariant::fromValue(quint64(3))},
            {QStringLiteral("kwinOwner"), QStringLiteral(":1.9")},
            {QStringLiteral("preferences"),
             QVariantMap{{QStringLiteral("windowManagement.focusPolicy"), focusPolicy},
                         {QStringLiteral("windowManagement.dockingModifier"),
                          QStringLiteral("super")},
                         {QStringLiteral("windowManagement.snapDistance"), 12},
                         {QStringLiteral("windowManagement.closeContainerPolicy"),
                          QStringLiteral("ask")}}},
            {QStringLiteral("message"), QString{}}};
}

class ApplyStateObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.WindowManagement1")
public:
    explicit ApplyStateObject(QDBusConnection bus)
        : m_bus(std::move(bus))
    {
    }

    QVariantMap state = appliedState();
    int retryCalls = 0;
    int getStateCalls = 0;
    bool holdGetState = false;

    [[nodiscard]] qsizetype pendingGetStateCount() const noexcept
    {
        return m_pendingGetState.has_value() ? 1 : 0;
    }

    bool replyPendingGetState()
    {
        if (!m_pendingGetState) {
            return false;
        }
        const QDBusMessage request = std::exchange(m_pendingGetState, std::nullopt).value();
        return m_bus.send(request.createReply(QVariant::fromValue(state)));
    }

public Q_SLOTS:
    QVariantMap GetState()
    {
        ++getStateCalls;
        if (holdGetState) {
            // AGENT-GUARD: Retain the request, not QDBusContext; it expires
            // when this exported method returns.
            m_pendingGetState = message();
            setDelayedReply(true);
            return {};
        }
        return state;
    }
    void RetryApply()
    {
        ++retryCalls;
        state = appliedState(QStringLiteral("click"), QStringLiteral("applying"));
        Q_EMIT StateChanged(state);
    }

Q_SIGNALS:
    void StateChanged(const QVariantMap &state);

private:
    QDBusConnection m_bus;
    std::optional<QDBusMessage> m_pendingGetState;
};

QString startPrivateBus(QProcess *daemon)
{
    daemon->start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                  {QStringLiteral("--session"), QStringLiteral("--nofork"),
                   QStringLiteral("--print-address=1")});
    if (!daemon->waitForStarted() || !daemon->waitForReadyRead()) {
        return {};
    }
    return QString::fromUtf8(daemon->readLine()).trimmed();
}

uint requestName(const QDBusConnection &bus, const QString &name, uint flags)
{
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("RequestName"));
    request << name << flags;
    const QDBusMessage reply = bus.call(request);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return 0;
    }
    return reply.arguments().constFirst().toUInt();
}

uint releaseName(const QDBusConnection &bus, const QString &name)
{
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ReleaseName"));
    request << name;
    const QDBusMessage reply = bus.call(request);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return 0;
    }
    return reply.arguments().constFirst().toUInt();
}

} // namespace

class WindowsSessionApplyStatusClientTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exactOwnerReadbackRetryLossAndReplacement();
};

void WindowsSessionApplyStatusClientTest::exactOwnerReadbackRetryLossAndReplacement()
{
    QProcess daemon;
    const auto stopDaemon = qScopeGuard([&daemon] {
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
            daemon.waitForFinished();
        }
    });
    const QString address = startPrivateBus(&daemon);
    QVERIFY2(!address.isEmpty(), qPrintable(daemon.errorString()));
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString clientName = QStringLiteral("wm-status-client-") + suffix;
    const QString firstName = QStringLiteral("wm-status-owner-a-") + suffix;
    const QString replacementName = QStringLiteral("wm-status-owner-b-") + suffix;
    auto clientBus = QDBusConnection::connectToBus(address, clientName);
    auto firstBus = QDBusConnection::connectToBus(address, firstName);
    auto replacementBus = QDBusConnection::connectToBus(address, replacementName);
    QVERIFY(clientBus.isConnected() && firstBus.isConnected()
            && replacementBus.isConnected());

    ApplyStateObject first(firstBus);
    QVERIFY(firstBus.registerObject(QString::fromLatin1(ObjectPath), &first,
                                    QDBusConnection::ExportAllSlots
                                        | QDBusConnection::ExportAllSignals));
    QDBusServiceWatcher ownerWatcher(QString::fromLatin1(ServiceName), clientBus,
                                     QDBusServiceWatcher::WatchForOwnerChange);
    QSignalSpy ownerChanges(&ownerWatcher, &QDBusServiceWatcher::serviceOwnerChanged);
    // D-Bus flags: A allows replacement; B later requests replacement directly,
    // producing one A-to-B NameOwnerChanged event without an empty-owner gap.
    QCOMPARE(requestName(firstBus, QString::fromLatin1(ServiceName), 1U), 1U);
    QTRY_COMPARE_WITH_TIMEOUT(ownerChanges.size(), 1, 2'000);
    QCOMPARE(ownerChanges.constLast().at(1).toString(), QString{});
    QCOMPARE(ownerChanges.constLast().at(2).toString(), firstBus.baseService());

    WindowsSessionApplyStatusClient client(clientBus);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.status().serviceAvailable, 2'000);
    QVERIFY2(client.status().serviceAvailable, qPrintable(client.status().message));
    QCOMPARE(client.status().phase, SessionApplyPhase::Applied);
    QVERIFY(client.status().preferences.has_value());
    QCOMPARE(client.status().preferences->focusPolicy, QStringLiteral("click"));

    first.state = appliedState(QStringLiteral("click"), QStringLiteral("failed"));
    first.state[QStringLiteral("message")] = QStringLiteral("kwinrc could not be written");
    Q_EMIT first.StateChanged(first.state);
    QTRY_COMPARE_WITH_TIMEOUT(client.status().phase, SessionApplyPhase::Failed, 2'000);
    QVERIFY(client.status().message.contains(QStringLiteral("could not be written")));

    client.retryApply();
    QTRY_COMPARE_WITH_TIMEOUT(first.retryCalls, 1, 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(client.status().phase, SessionApplyPhase::Applying, 2'000);

    first.holdGetState = true;
    first.state = appliedState(QStringLiteral("focus-under-mouse"));
    Q_EMIT first.StateChanged(first.state);
    QTRY_COMPARE_WITH_TIMEOUT(first.pendingGetStateCount(), qsizetype(1), 2'000);

    ApplyStateObject replacement(replacementBus);
    replacement.state = appliedState(QStringLiteral("focus-follows-mouse"));
    QVERIFY(replacementBus.registerObject(QString::fromLatin1(ObjectPath), &replacement,
                                          QDBusConnection::ExportAllSlots
                                              | QDBusConnection::ExportAllSignals));
    replacement.holdGetState = true;
    QCOMPARE(requestName(replacementBus, QString::fromLatin1(ServiceName), 6U), 1U);
    QTRY_COMPARE_WITH_TIMEOUT(replacement.pendingGetStateCount(), qsizetype(1), 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(ownerChanges.size(), 2, 2'000);
    QCOMPARE(ownerChanges.constLast().at(1).toString(), firstBus.baseService());
    QCOMPARE(ownerChanges.constLast().at(2).toString(), replacementBus.baseService());
    QVERIFY(!client.status().serviceAvailable);
    QCOMPARE(client.status().phase, SessionApplyPhase::Unavailable);
    QVERIFY(client.status().message.contains(QStringLiteral("owner changed")));
    // A displaced but still-live connection can be queued by D-Bus. Remove its
    // queued name claim so releasing B below exercises the empty-owner path.
    releaseName(firstBus, QString::fromLatin1(ServiceName));
    // A late A reply cannot revive its acknowledgement during B's held read.
    QVERIFY(first.replyPendingGetState());
    QTest::qWait(50);
    QVERIFY(!client.status().serviceAvailable);
    QCOMPARE(client.status().phase, SessionApplyPhase::Unavailable);

    QVERIFY(replacement.replyPendingGetState());
    QTRY_VERIFY_WITH_TIMEOUT(client.status().serviceAvailable, 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(client.status().preferences->focusPolicy,
                              QStringLiteral("focus-follows-mouse"), 2'000);

    QCOMPARE(releaseName(replacementBus, QString::fromLatin1(ServiceName)), 1U);
    QTRY_VERIFY_WITH_TIMEOUT(!client.status().serviceAvailable, 2'000);
    QVERIFY(client.status().message.contains(QStringLiteral("Start or restart")));
    firstBus.unregisterObject(QString::fromLatin1(ObjectPath));
    replacementBus.unregisterObject(QString::fromLatin1(ObjectPath));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(firstName);
    QDBusConnection::disconnectFromBus(replacementName);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(WindowsSessionApplyStatusClientTest)
#include "tst_windows_session_apply_status_client.moc"
