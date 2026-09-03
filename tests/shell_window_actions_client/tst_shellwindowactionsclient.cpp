// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_transport.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt;

namespace {

const QString WindowId = QStringLiteral("11111111-2222-4333-8444-555555555555");
const Compositor::ShellWindowGeneration Generation{QStringLiteral("epoch-a"), 7};

class FakeTransport final
    : public ShellWindowActionsClient::ShellWindowActionsTransport
{
public:
    struct Sent final
    {
        quint64 token;
        QString owner;
        Compositor::ShellWindowAction action;
        QString windowId;
        Compositor::ShellWindowGeneration generation;
    };

    bool start(QString *error) override
    {
        started = startSucceeds;
        if (!started && error) *error = QStringLiteral("start failed");
        return started;
    }
    void stop() override { started = false; }
    void request(quint64 token, const QString &owner,
                 Compositor::ShellWindowAction action, const QString &windowId,
                 const Compositor::ShellWindowGeneration &generation) override
    {
        sent.append({token, owner, action, windowId, generation});
    }

    void owner(const QString &value) { Q_EMIT serviceOwnerChanged(value); }
    void reply(const Compositor::ShellWindowActionResult &result)
    {
        const auto request = sent.constLast();
        Q_EMIT replyReceived(request.token, request.owner,
                             Compositor::encodeShellWindowActionResult(result));
    }
    void failLast()
    {
        const auto request = sent.constLast();
        Q_EMIT requestFailed(request.token, request.owner,
                             QStringLiteral("wire failed"));
    }

    QVector<Sent> sent;
    bool started = false;
    bool startSucceeds = true;
};

} // namespace

class ShellWindowActionsClientTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bindsExactOwnerAndCompletesMatchingReply();
    void serializesAndNeverReplaysFailure();
    void ownerChangeMakesPendingOutcomeUncertain();
    void ownerChangeBindsBeforeCompletionSignal();
    void malformedReplyIsUncertain();
    void timeoutIsUncertainWithoutReplay();
};

void ShellWindowActionsClientTest::bindsExactOwnerAndCompletesMatchingReply()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 100);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.23"));
    QVERIFY(client.available());
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, Generation));
    QCOMPARE(transport.sent.size(), 1);
    QCOMPARE(transport.sent[0].owner, QStringLiteral(":1.23"));
    transport.reply({Compositor::ShellWindowActionStatus::Admitted,
                     Compositor::ShellWindowAction::Activate,
                     WindowId, Generation, {}, {}});
    QVERIFY(!client.requestInFlight());
    QVERIFY(client.lastResult());
    QVERIFY(client.lastResult()->serverResult->admitted());
    QVERIFY(!client.lastResult()->uncertain);
}

void ShellWindowActionsClientTest::serializesAndNeverReplaysFailure()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 100);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.8"));
    QVERIFY(client.request(Compositor::ShellWindowAction::Minimize,
                           WindowId, Generation));
    QString error;
    QVERIFY(!client.request(Compositor::ShellWindowAction::Raise,
                            WindowId, Generation, &error));
    QVERIFY(!error.isEmpty());
    transport.failLast();
    QTest::qWait(10);
    QCOMPARE(transport.sent.size(), 1);
    QVERIFY(client.lastResult()->uncertain);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("transport-failed"));
}

void ShellWindowActionsClientTest::ownerChangeMakesPendingOutcomeUncertain()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 100);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.1"));
    QVERIFY(client.request(Compositor::ShellWindowAction::Close,
                           WindowId, Generation));
    transport.owner(QStringLiteral(":1.2"));
    QVERIFY(client.lastResult()->uncertain);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("owner-changed"));
    QCOMPARE(client.uniqueOwner(), QStringLiteral(":1.2"));
    const auto request = transport.sent.constLast();
    Q_EMIT transport.replyReceived(
        request.token, request.owner,
        Compositor::encodeShellWindowActionResult({
            Compositor::ShellWindowActionStatus::Admitted,
            Compositor::ShellWindowAction::Close, WindowId, Generation, {}, {}}));
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("owner-changed"));
}

void ShellWindowActionsClientTest::ownerChangeBindsBeforeCompletionSignal()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 100);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.1"));
    QVERIFY(client.request(Compositor::ShellWindowAction::Activate,
                           WindowId, Generation));
    QMetaObject::Connection completion;
    completion = connect(
            &client, &ShellWindowActionsClient::ShellWindowActionsClient::actionFinished,
            &client, [&] {
                disconnect(completion);
                QVERIFY(client.request(Compositor::ShellWindowAction::Raise,
                                       WindowId, Generation));
            });
    transport.owner(QStringLiteral(":1.2"));
    QCOMPARE(transport.sent.size(), 2);
    QCOMPARE(transport.sent.constLast().owner, QStringLiteral(":1.2"));
}

void ShellWindowActionsClientTest::malformedReplyIsUncertain()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 100);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.5"));
    QVERIFY(client.request(Compositor::ShellWindowAction::Raise,
                           WindowId, Generation));
    const auto request = transport.sent.constLast();
    Q_EMIT transport.replyReceived(request.token, request.owner, QByteArray("{}"));
    QVERIFY(client.lastResult()->uncertain);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("malformed-reply"));
}

void ShellWindowActionsClientTest::timeoutIsUncertainWithoutReplay()
{
    FakeTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 5);
    QVERIFY(client.start());
    transport.owner(QStringLiteral(":1.6"));
    QVERIFY(client.request(Compositor::ShellWindowAction::Unminimize,
                           WindowId, Generation));
    QTRY_VERIFY_WITH_TIMEOUT(client.lastResult().has_value(), 100);
    QVERIFY(client.lastResult()->uncertain);
    QCOMPARE(client.lastResult()->failureCode, QStringLiteral("request-timeout"));
    QCOMPARE(transport.sent.size(), 1);
}

QTEST_GUILESS_MAIN(ShellWindowActionsClientTest)
#include "tst_shellwindowactionsclient.moc"
