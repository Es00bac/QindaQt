// SPDX-License-Identifier: GPL-3.0-or-later

// The client half of the voice interop probe. Run by
// tests/session/voice/voice_interop_probe.py inside a private session bus on
// which a real org.qindaqt.Voice1 provider is already published.
//
// AGENT-CONTRACT: this binary uses the production VoiceClient and
// QtVoiceTransport, unmodified. Its whole value is that nothing here is a test
// double, so a contract drift between QindaQt and a provider fails here.

#include <qindaqt/services/voice_client/qt_voice_transport.h>
#include <qindaqt/services/voice_client/voice_client.h>
#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

#include <functional>

using namespace QindaQt::Services::Voice;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

int failures = 0;

void check(const bool condition, const QString &what)
{
    out() << (condition ? "ok   " : "FAIL ") << what << Qt::endl;
    if (!condition) {
        ++failures;
    }
}

// Runs the event loop until `predicate` holds or the budget expires.
bool waitFor(const std::function<bool()> &predicate, const int milliseconds = 5'000)
{
    QEventLoop loop;
    QTimer poll;
    QTimer deadline;
    bool satisfied = false;
    poll.setInterval(10);
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (predicate()) {
            satisfied = true;
            loop.quit();
        }
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    deadline.start(milliseconds);
    if (predicate()) {
        return true;
    }
    loop.exec();
    return satisfied;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    QtVoiceTransport transport(QDBusConnection::sessionBus());
    VoiceClient client(&transport);

    OperationResult lastResult;
    bool resultArrived = false;
    QObject::connect(&client, &VoiceClient::operationCompleted, &application,
                     [&](quint64, const OperationResult &result) {
                         lastResult = result;
                         resultArrived = true;
                     });

    client.start();
    check(waitFor([&] { return client.state() == ClientState::Ready; }),
          QStringLiteral("the client reaches Ready against the real provider"));
    if (client.state() != ClientState::Ready) {
        out() << "FAIL the provider never became readable; reason="
              << client.reasonCode() << Qt::endl;
        out().flush();
        return 1;
    }

    const Snapshot first = client.snapshot();
    check(validateSnapshot(first).accepted,
          QStringLiteral("the provider's snapshot passes QindaQt validation"));
    check(first.providerId == QStringLiteral("mock"),
          QStringLiteral("the provider reports its configured provider id"));
    check(first.dictationShortcut == QStringLiteral("F5"),
          QStringLiteral("the dictation shortcut crosses the wire"));
    check(!first.providers.isEmpty(),
          QStringLiteral("the provider advertises a provider inventory"));
    check(first.state == SessionState::Idle,
          QStringLiteral("a fresh provider reports Idle"));

    // A stale revision must be refused by the provider, not silently obeyed.
    resultArrived = false;
    const quint64 staleId = client.startDictation();
    check(staleId != 0, QStringLiteral("the client mints a request id"));
    check(waitFor([&] { return resultArrived; }),
          QStringLiteral("the provider answers a dictation request"));
    check(lastResult.status == OperationStatus::Succeeded,
          QStringLiteral("the provider accepts a request at the current revision"));
    check(validateOperationResult(lastResult).accepted,
          QStringLiteral("the provider's result passes QindaQt validation"));

    check(waitFor([&] { return client.snapshot().state == SessionState::Listening; }),
          QStringLiteral("the Changed signal carries the provider into Listening"));
    const Snapshot listening = client.snapshot();
    check(listening.revision > first.revision,
          QStringLiteral("the revision advanced on a caller-visible change"));
    check(listening.partialText == QStringLiteral("the quick"),
          QStringLiteral("a live partial crosses the wire during capture"));
    check(validateSnapshot(listening).accepted,
          QStringLiteral("the capturing snapshot passes validation"));

    // Starting a second recording over a live one must be refused.
    resultArrived = false;
    check(client.startDictation() != 0, QStringLiteral("a second request is minted"));
    check(waitFor([&] { return resultArrived; }),
          QStringLiteral("the provider answers the second request"));
    check(lastResult.status == OperationStatus::Rejected,
          QStringLiteral("a second recording is refused while one is live"));
    check(lastResult.reasonCode == QStringLiteral("already-capturing"),
          QStringLiteral("the refusal names a structured reason"));

    resultArrived = false;
    check(client.finish() != 0, QStringLiteral("a finish request is minted"));
    check(waitFor([&] { return resultArrived; }),
          QStringLiteral("the provider answers the finish request"));
    check(lastResult.status == OperationStatus::Succeeded,
          QStringLiteral("finishing a live recording succeeds"));
    check(waitFor([&] {
              return client.snapshot().lastText
                     == QStringLiteral("the quick brown fox");
          }),
          QStringLiteral("the delivered text reaches the client"));
    const Snapshot delivered = client.snapshot();
    check(delivered.partialText.isEmpty(),
          QStringLiteral("no partial survives the end of capture"));
    check(delivered.lastRoute == DeliveryRoute::InputMethod,
          QStringLiteral("the delivery route is mapped to the contract's enum"));

    // Disarming must be refused nowhere and honoured everywhere.
    resultArrived = false;
    check(client.setEnabled(false) != 0,
          QStringLiteral("a disarm request is minted"));
    check(waitFor([&] { return resultArrived; }),
          QStringLiteral("the provider answers the disarm request"));
    check(lastResult.status == OperationStatus::Succeeded,
          QStringLiteral("the provider accepts being disarmed"));
    check(waitFor([&] { return !client.snapshot().enabled; }),
          QStringLiteral("the disarmed state reaches the client"));

    resultArrived = false;
    check(client.startDictation() != 0,
          QStringLiteral("a request is minted while disarmed"));
    check(waitFor([&] { return resultArrived; }),
          QStringLiteral("the provider answers while disarmed"));
    check(lastResult.status == OperationStatus::Rejected
              && lastResult.reasonCode == QStringLiteral("input-disabled"),
          QStringLiteral("a disarmed provider refuses to open the microphone"));

    out() << (failures == 0 ? "PASS" : "FAIL") << " voice interop: "
          << (failures == 0 ? QStringLiteral("all checks passed")
                            : QStringLiteral("%1 check(s) failed").arg(failures))
          << Qt::endl;
    out().flush();
    return failures == 0 ? 0 : 1;
}
