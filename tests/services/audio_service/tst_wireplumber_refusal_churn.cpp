// SPDX-License-Identifier: GPL-3.0-or-later

// Refused console loopbacks and VBAN streams under client-stream churn
// (audio-service wiki, 2026-10-21). The worker re-applies every declaration
// on each graph event. A send, virtual-bus or VBAN route loopback PipeWire
// refused, or a VBAN stream that could not start, used to be tried again on
// every one of them; a VBAN failure logged a warning each time. Each refusal
// must now be attempted, and reported, once per declaration.

#include "support/private_audio_churn.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace
{

constexpr int kWarmupCycles = 5;
constexpr int kMeasuredCycles = 30;
constexpr qint64 kMaxGrowthPerCycle = 8 * 1024;
const QString kBusRefused = QStringLiteral("PipeWire refused console bus");
const QString kSendRefused = QStringLiteral("PipeWire refused console send");
const QString kRouteRefused = QStringLiteral("loopback route failed to load");
const QString kReceiverFailed = QStringLiteral("VBAN: incoming stream 'churn-in' failed");

// A UDP socket bound to a kernel-chosen port, without SO_REUSEADDR: while it
// is open the receiver's own bind of that port fails.
class HeldPort final
{
public:
    HeldPort()
        : m_socket(::socket(AF_INET, SOCK_DGRAM, 0))
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        socklen_t length = sizeof(address);
        if (m_socket >= 0
            && ::bind(m_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0
            && ::getsockname(m_socket, reinterpret_cast<sockaddr *>(&address), &length) == 0) {
            m_port = ntohs(address.sin_port);
        }
    }
    ~HeldPort()
    {
        if (m_socket >= 0) {
            ::close(m_socket);
        }
    }
    HeldPort(const HeldPort &) = delete;
    HeldPort &operator=(const HeldPort &) = delete;
    [[nodiscard]] quint16 port() const { return m_port; }

private:
    int m_socket = -1;
    quint16 m_port = 0;
};

// Sets a backend-only variable before the rig exists and restores it after:
// setenv must not race the worker thread's getenv.
class ScopedVariable final
{
public:
    ScopedVariable(const char *name, const QByteArray &value)
        : m_name(name)
        , m_had(qEnvironmentVariableIsSet(name))
        , m_previous(qgetenv(name))
    {
        qputenv(name, value);
    }
    ~ScopedVariable()
    {
        if (m_had) {
            qputenv(m_name, m_previous);
        } else {
            qunsetenv(m_name);
        }
    }
    ScopedVariable(const ScopedVariable &) = delete;
    ScopedVariable &operator=(const ScopedVariable &) = delete;

private:
    const char *m_name;
    bool m_had;
    QByteArray m_previous;
};

// The installed PipeWire modules except the loopback, so every console
// loopback this process asks for is refused while the core itself loads.
bool linkModulesWithoutLoopback(const QString &directory)
{
    const QDir system(QStringLiteral(QINDAQT_PIPEWIRE_MODULE_DIR));
    bool linked = false;
    for (const QString &entry : system.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (entry == QLatin1String("libpipewire-module-loopback.so")) {
            continue;
        }
        linked = QFile::link(system.filePath(entry), directory + QLatin1Char('/') + entry)
            || linked;
    }
    return linked;
}

bool declareIncomingVban(PrivateAudioChurn &churn, const quint16 port)
{
    OperationRequest upsert;
    upsert.kind = OperationKind::UpsertVbanStream;
    upsert.vbanDefinition.name = QStringLiteral("churn-in");
    upsert.vbanDefinition.outgoing = false;
    upsert.vbanDefinition.host = QStringLiteral("127.0.0.1");
    upsert.vbanDefinition.port = port;
    upsert.vbanDefinition.outputNodeName = QStringLiteral("qindaqt.test.churn.output");
    return churn.apply(upsert);
}

bool enableVban(PrivateAudioChurn &churn, const bool enabled)
{
    OperationRequest request;
    request.kind = OperationKind::SetVbanEnabled;
    request.displayName = QStringLiteral("churn-in");
    request.enabled = enabled;
    return churn.apply(request);
}

QString growthMessage(const qint64 growth)
{
    return QStringLiteral("heap grew %1 bytes over %2 client streams (bound %3 per stream)")
        .arg(growth)
        .arg(kMeasuredCycles)
        .arg(kMaxGrowthPerCycle);
}

} // namespace

class WirePlumberRefusalChurnTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void refusedConsoleLoopbacksAreTriedOncePerDeclaration();
    void aVbanStreamThatCannotStartIsTriedOncePerDeclaration();
};

void WirePlumberRefusalChurnTests::initTestCase()
{
#if !defined(__GLIBC__)
    QSKIP("the bound is measured with glibc's mallinfo2");
#endif
}

void WirePlumberRefusalChurnTests::refusedConsoleLoopbacksAreTriedOncePerDeclaration()
{
    QTemporaryDir modules(QStringLiteral("/tmp/qindaqt-audio-modules-XXXXXX"));
    QVERIFY(modules.isValid());
    QVERIFY(linkModulesWithoutLoopback(modules.path()));
    const ScopedVariable moduleDirectory("PIPEWIRE_MODULE_DIR", modules.path().toUtf8());
    // Counting from before the first rebuild: the default console declares
    // its three virtual-bus loopbacks as soon as the backend runs.
    WarningTally warnings({kBusRefused, kSendRefused, kRouteRefused});
    PrivateAudioChurn churn;
    QString failure;
    QVERIFY2(churn.start(&failure), qPrintable(failure));
    // A send from the bound hardware strip into the bound physical bus A1;
    // the default console's three virtual buses are loopbacks already.
    OperationRequest send;
    send.kind = OperationKind::SetStripSend;
    send.consoleId = QStringLiteral("strip.hw.1");
    send.busIndex = 0;
    send.enabled = true;
    send.gainDb = 0.0;
    QVERIFY(churn.apply(send));
    const quint16 freePort = HeldPort().port();
    QVERIFY(freePort != 0);
    QVERIFY(declareIncomingVban(churn, freePort));
    QVERIFY(enableVban(churn, true));
    churn.settle();

    const std::optional<qint64> growth = churn.churnGrowth(kWarmupCycles, kMeasuredCycles);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    // Each refused declaration was attempted, and reported, exactly once. The
    // VBAN route used to be retried, and logged, on every graph event.
    QCOMPARE(warnings.count(kRouteRefused), 1);
    QCOMPARE(warnings.count(kSendRefused), 1);
    QCOMPARE(warnings.count(kBusRefused), 3);
}

void WirePlumberRefusalChurnTests::aVbanStreamThatCannotStartIsTriedOncePerDeclaration()
{
    const HeldPort busy;
    QVERIFY(busy.port() != 0);
    PrivateAudioChurn churn;
    QString failure;
    QVERIFY2(churn.start(&failure), qPrintable(failure));
    WarningTally warnings({kReceiverFailed});
    QVERIFY(declareIncomingVban(churn, busy.port()));
    QVERIFY(enableVban(churn, true));
    churn.settle();
    const std::optional<qint64> growth = churn.churnGrowth(kWarmupCycles, kMeasuredCycles);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    QCOMPARE(warnings.count(kReceiverFailed), 1);

    // Switching the stream off and on again is a new declaration (a fresh
    // activation token): it earns exactly one more attempt.
    QVERIFY(enableVban(churn, false));
    churn.settle();
    QVERIFY(enableVban(churn, true));
    churn.settle();
    for (int index = 0; index < kWarmupCycles; ++index) {
        QVERIFY(churn.cycle());
    }
    churn.settle();
    QCOMPARE(warnings.count(kReceiverFailed), 2);
}

QTEST_GUILESS_MAIN(WirePlumberRefusalChurnTests)
#include "tst_wireplumber_refusal_churn.moc"
