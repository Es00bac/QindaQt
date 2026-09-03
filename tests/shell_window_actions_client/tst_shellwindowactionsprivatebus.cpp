// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

#include <optional>
#include <utility>

using namespace QindaQt;

namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/CompositorShell";
const QString WindowId = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

class FakeCompositor final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.CompositorShell1")

public:
    FakeCompositor(QDBusConnection connection, bool delayReplies)
        : m_connection(std::move(connection))
        , m_delayReplies(delayReplies)
    {
    }

    bool replyPending()
    {
        if (!m_pendingMessage) return false;
        const bool sent = m_connection.send(m_pendingMessage->createReply(
            QVariantList{QVariant::fromValue(m_pendingReply)}));
        m_pendingMessage.reset();
        m_pendingReply.clear();
        return sent;
    }

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray ActivateWindow(const QString &windowId,
                                           const QString &epoch,
                                           const QString &revision)
    {
        ++calls;
        const QByteArray result = Compositor::encodeShellWindowActionResult({
            Compositor::ShellWindowActionStatus::Admitted,
            Compositor::ShellWindowAction::Activate, windowId,
            {epoch, revision.toULongLong()}, {}, {}});
        if (m_delayReplies) {
            setDelayedReply(true);
            m_pendingMessage = message();
            m_pendingReply = result;
            return {};
        }
        return result;
    }

public:
    int calls = 0;

private:
    QDBusConnection m_connection;
    std::optional<QDBusMessage> m_pendingMessage;
    QByteArray m_pendingReply;
    bool m_delayReplies = false;
};

bool registerCompositor(QDBusConnection &connection, FakeCompositor &compositor)
{
    return connection.registerService(QString::fromLatin1(ServiceName))
        && connection.registerObject(QString::fromLatin1(ObjectPath), &compositor,
                                     QDBusConnection::ExportScriptableSlots);
}

void unregisterCompositor(QDBusConnection &connection)
{
    connection.unregisterObject(QString::fromLatin1(ObjectPath));
    connection.unregisterService(QString::fromLatin1(ServiceName));
}

} // namespace

class ShellWindowActionsPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void callsFakeCompositorThroughExactOwner();
    void ownerReplacementWithdrawsTruthAndRejectsOldReply();
    void requestTimeoutOnRealBusIsUncertainWithoutReplay();
};

void ShellWindowActionsPrivateBusTest::callsFakeCompositorThroughExactOwner()
{
    auto serviceBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-happy-service"));
    auto clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-happy-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeCompositor compositor(serviceBus, false);
    QVERIFY(registerCompositor(serviceBus, compositor));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(clientBus);
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    QTRY_COMPARE_WITH_TIMEOUT(client.uniqueOwner(), serviceBus.baseService(), 500);
    const Compositor::ShellWindowGeneration generation{
        QStringLiteral("epoch-private"), 3};
    QSignalSpy finished(&client,
                        &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished);
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, generation));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 500);
    QCOMPARE(compositor.calls, 1);
    QVERIFY(client.lastResult()->serverResult->admitted());
    QCOMPARE(client.lastResult()->generation, generation);

    unregisterCompositor(serviceBus);
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-happy-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-happy-service"));
}

void ShellWindowActionsPrivateBusTest::
ownerReplacementWithdrawsTruthAndRejectsOldReply()
{
    auto oldBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-old-service"));
    auto replacementBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-new-service"));
    auto clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-replace-client"));
    QVERIFY(oldBus.isConnected());
    QVERIFY(replacementBus.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeCompositor oldCompositor(oldBus, true);
    FakeCompositor replacementCompositor(replacementBus, false);
    QVERIFY(registerCompositor(oldBus, oldCompositor));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(clientBus);
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    QTRY_COMPARE_WITH_TIMEOUT(client.uniqueOwner(), oldBus.baseService(), 500);
    QSignalSpy finished(&client,
                        &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished);
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, {QStringLiteral("epoch-old"), 4}));
    QTRY_COMPARE_WITH_TIMEOUT(oldCompositor.calls, 1, 500);
    QVERIFY(client.requestInFlight());

    QVERIFY(oldBus.unregisterService(QString::fromLatin1(ServiceName)));
    QVERIFY(registerCompositor(replacementBus, replacementCompositor));
    QTRY_COMPARE_WITH_TIMEOUT(client.uniqueOwner(), replacementBus.baseService(), 500);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 500);
    QVERIFY(client.available());
    QVERIFY(!client.requestInFlight());
    QVERIFY(client.lastResult()->uncertain);
    QVERIFY(!client.lastResult()->serverResult);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("owner-changed"));
    QCOMPARE(replacementCompositor.calls, 0);

    QVERIFY(oldCompositor.replyPending());
    QTest::qWait(50);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("owner-changed"));
    QCOMPARE(replacementCompositor.calls, 0);

    unregisterCompositor(replacementBus);
    oldBus.unregisterObject(QString::fromLatin1(ObjectPath));
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-replace-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-new-service"));
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-old-service"));
}

void ShellWindowActionsPrivateBusTest::
requestTimeoutOnRealBusIsUncertainWithoutReplay()
{
    auto serviceBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-timeout-service"));
    auto clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("actions-timeout-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    FakeCompositor compositor(serviceBus, true);
    QVERIFY(registerCompositor(serviceBus, compositor));

    ShellWindowActionsClient::QtShellWindowActionsTransport transport(clientBus);
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 20);
    QVERIFY(client.start());
    QTRY_COMPARE_WITH_TIMEOUT(client.uniqueOwner(), serviceBus.baseService(), 500);
    QSignalSpy finished(&client,
                        &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished);
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, {QStringLiteral("epoch-timeout"), 5}));
    QTRY_COMPARE_WITH_TIMEOUT(compositor.calls, 1, 500);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 500);
    QVERIFY(client.lastResult()->uncertain);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("request-timeout"));
    QCOMPARE(compositor.calls, 1);

    QVERIFY(compositor.replyPending());
    QTest::qWait(50);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(compositor.calls, 1);

    unregisterCompositor(serviceBus);
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-timeout-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("actions-timeout-service"));
}

QTEST_GUILESS_MAIN(ShellWindowActionsPrivateBusTest)
#include "tst_shellwindowactionsprivatebus.moc"
