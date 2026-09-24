// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_session_apply_status_client.h"

#include <QDBusConnection>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

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

class ApplyStateObject final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.WindowManagement1")
public:
    QVariantMap state = appliedState();
    int retryCalls = 0;

public Q_SLOTS:
    QVariantMap GetState() const { return state; }
    void RetryApply()
    {
        ++retryCalls;
        state = appliedState(QStringLiteral("click"), QStringLiteral("applying"));
        Q_EMIT StateChanged(state);
    }

Q_SIGNALS:
    void StateChanged(const QVariantMap &state);
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

    ApplyStateObject first;
    QVERIFY(firstBus.registerObject(QString::fromLatin1(ObjectPath), &first,
                                    QDBusConnection::ExportAllSlots
                                        | QDBusConnection::ExportAllSignals));
    QVERIFY(firstBus.registerService(QString::fromLatin1(ServiceName)));

    WindowsSessionApplyStatusClient client(clientBus);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.status().serviceAvailable
                                 || !client.status().message.isEmpty(),
                             2'000);
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

    firstBus.unregisterService(QString::fromLatin1(ServiceName));
    QTRY_VERIFY_WITH_TIMEOUT(!client.status().serviceAvailable, 2'000);
    QVERIFY(client.status().message.contains(QStringLiteral("Start or restart")));

    ApplyStateObject replacement;
    replacement.state = appliedState(QStringLiteral("focus-follows-mouse"));
    QVERIFY(replacementBus.registerObject(QString::fromLatin1(ObjectPath), &replacement,
                                          QDBusConnection::ExportAllSlots
                                              | QDBusConnection::ExportAllSignals));
    QVERIFY(replacementBus.registerService(QString::fromLatin1(ServiceName)));
    QTRY_VERIFY_WITH_TIMEOUT(client.status().serviceAvailable, 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(client.status().preferences->focusPolicy,
                              QStringLiteral("focus-follows-mouse"), 2'000);

    replacementBus.unregisterService(QString::fromLatin1(ServiceName));
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
