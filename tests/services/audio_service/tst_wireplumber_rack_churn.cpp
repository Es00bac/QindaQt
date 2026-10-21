// SPDX-License-Identifier: GPL-3.0-or-later

// Strip racks under client-stream churn (audio-service wiki, 2026-10-21).
// The worker re-applies every declaration on each graph event. A rack that
// PipeWire refused was loaded again on every one of them, and PipeWire 1.6.8
// leaks the filter-graph nodes it had already loaded on each refusal: every
// short playback client grew the service heap by hundreds of kilobytes. Each
// row churns pw-cat clients through a private PipeWire/WirePlumber with the
// real backend and coordinator in this process, and bounds the heap growth.

#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include <functional>
#include <memory>
#include <optional>

using namespace QindaQt::Audio;

namespace
{

constexpr int kWarmupCycles = 10;
constexpr int kMeasuredCycles = 40;
// The leak was 48-110 kB per refused load, about three loads per stream per
// racked strip: over 600 kB per stream here. A correct worker measured a few
// hundred bytes per stream. The bound sits far from both.
constexpr qint64 kMaxGrowthPerCycle = 8 * 1024;

// AGENT-NOTE: glibc's in-use bytes over every arena plus mmapped chunks. The
// growth this guards was malloc'd heap with no new mapping or descriptor.
qint64 heapInUse()
{
#if defined(__GLIBC__)
    const struct mallinfo2 info = mallinfo2();
    return static_cast<qint64>(info.uordblks + info.hblkhd);
#else
    return 0;
#endif
}

bool waitFor(const std::function<bool()> &condition, const int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QTest::qWait(5);
    }
    return true;
}

// AGENT-GUARD: a child's output never accumulates in this process. QProcess
// keeps unread output on the heap, and the private daemons log on graph
// changes; buffered here, that log alone measured 16 kB per client stream.
class ProcessGuard final
{
public:
    ~ProcessGuard() { stop(); }

    bool start(const QString &program, const QStringList &arguments,
               const QProcessEnvironment &environment, const QString &logPath = {})
    {
        process.setProcessEnvironment(environment);
        process.setProgram(program);
        process.setArguments(arguments);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setStandardOutputFile(logPath.isEmpty() ? QProcess::nullDevice() : logPath);
        process.start();
        return process.waitForStarted(5000);
    }

    void stop()
    {
        if (process.state() == QProcess::NotRunning) {
            return;
        }
        process.terminate();
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(3000);
        }
    }

    QProcess process;
};

int clientStreams(const Snapshot &snapshot)
{
    int count = 0;
    for (const Stream &stream : snapshot.streams) {
        if (stream.direction == StreamDirection::Playback
            && stream.applicationName.contains(QLatin1String("pw-cat"))) {
            ++count;
        }
    }
    return count;
}

bool hasInput(const Snapshot &snapshot, const QString &nodeName)
{
    for (const Device &device : snapshot.inputs) {
        if (device.nodeName == nodeName) {
            return true;
        }
    }
    return false;
}

// One private graph, one console bound to it. Its PipeWire, WirePlumber and
// stores live under a temporary root; the session-bus address is unreachable.
class ChurnRig final
{
public:
    bool start(QString *failure)
    {
        const QString runtime = m_root.filePath(QStringLiteral("runtime"));
        const QString state = m_root.filePath(QStringLiteral("state"));
        const QString config = m_root.filePath(QStringLiteral("config"));
        if (!m_root.isValid() || !QDir().mkpath(runtime) || !QDir().mkpath(state)
            || !QDir().mkpath(config)) {
            *failure = QStringLiteral("private roots");
            return false;
        }
        m_environment = QProcessEnvironment::systemEnvironment();
        m_environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
        m_environment.insert(QStringLiteral("PIPEWIRE_RUNTIME_DIR"), runtime);
        m_environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
        m_environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        m_environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                             QStringLiteral("unix:path=%1/no-session-bus").arg(m_root.path()));
        m_environment.remove(QStringLiteral("PIPEWIRE_REMOTE"));
        m_environment.remove(QStringLiteral("PIPEWIRE_CORE"));
        // The backend reads PipeWire's environment on its GLib worker. These
        // name only this row's private socket; QtTest runs one process.
        for (const char *key : {"XDG_RUNTIME_DIR", "PIPEWIRE_RUNTIME_DIR", "XDG_STATE_HOME",
                                "XDG_CONFIG_HOME", "DBUS_SESSION_BUS_ADDRESS"}) {
            qputenv(key, m_environment.value(QString::fromLatin1(key)).toUtf8());
        }
        qunsetenv("PIPEWIRE_REMOTE");
        qunsetenv("PIPEWIRE_CORE");

        if (!m_pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                              {QStringLiteral("-c"),
                               QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                              m_environment, m_root.filePath(QStringLiteral("pipewire.log")))
            || !waitFor([&] { return QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")); },
                        5000)
            || !m_wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                                    {QStringLiteral("-p"), QStringLiteral("policy")},
                                    m_environment,
                                    m_root.filePath(QStringLiteral("wireplumber.log")))) {
            *failure = QStringLiteral("private PipeWire/WirePlumber");
            return false;
        }
        for (const QString &properties :
             {QStringLiteral("{ factory.name = support.null-audio-sink "
                             "node.name = qindaqt.test.churn.output "
                             "node.description = \"Churn Output\" media.class = Audio/Sink "
                             "object.linger = true audio.position = [ FL FR ] }"),
              QStringLiteral("{ factory.name = support.null-audio-sink "
                             "node.name = qindaqt.test.churn.input "
                             "node.description = \"Churn Input\" media.class = Audio/Source "
                             "object.linger = true audio.position = [ FL FR ] }")}) {
            QProcess create;
            create.setProcessEnvironment(m_environment);
            create.setStandardOutputFile(QProcess::nullDevice());
            create.setStandardErrorFile(QProcess::nullDevice());
            create.start(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                         {QStringLiteral("create-node"), QStringLiteral("adapter"), properties});
            if (!create.waitForFinished(5000) || create.exitCode() != 0) {
                *failure = QStringLiteral("fixture node");
                return false;
            }
        }

        m_backend = std::make_unique<WirePlumberAudioBackend>();
        m_coordinator = std::make_unique<AudioOperationCoordinator>(
            m_backend.get(), nullptr, m_root.filePath(QStringLiteral("presets")),
            m_root.filePath(QStringLiteral("macros.json")),
            m_root.filePath(QStringLiteral("vban.json")),
            m_root.filePath(QStringLiteral("latency.json")));
        // Only the latest value is kept: a growing record of snapshots would
        // be heap growth of the test's own making.
        QObject::connect(m_coordinator.get(), &AudioOperationCoordinator::snapshotChanged,
                         m_coordinator.get(), [this](const Snapshot &snapshot) {
                             ++m_publications;
                             if (snapshot.availability == Availability::Ready
                                 || snapshot.availability == Availability::Degraded) {
                                 m_clientStreams = clientStreams(snapshot);
                             }
                         });
        m_coordinator->start();
        const QString virtualSink = QStringLiteral("qindaqt.console.strip.virtual.1");
        if (!waitFor([&] {
                const Snapshot &now = m_coordinator->snapshot();
                bool virtualBound = false;
                for (const Device &device : now.outputs) {
                    virtualBound = virtualBound || device.nodeName == virtualSink;
                }
                return m_clientStreams == 0 && virtualBound
                    && hasInput(now, QStringLiteral("qindaqt.test.churn.input"));
            }, 15000)) {
            *failure = QStringLiteral("console never bound to the private graph");
            return false;
        }
        settle();
        return true;
    }

    // Both racked strips get the same rack: the hardware strip reads the
    // fixture input, the virtual strip its own sink's monitor.
    [[nodiscard]] bool declareRacks(const StripProcessing &rack)
    {
        for (const QString &strip :
             {QStringLiteral("strip.hw.1"), QStringLiteral("strip.virtual.1")}) {
            OperationRequest request;
            request.kind = OperationKind::SetStripProcessing;
            request.consoleId = strip;
            request.processing = rack;
            if (m_coordinator->submit(request).immediateResult.status
                != OperationStatus::Succeeded) {
                return false;
            }
        }
        return true;
    }

    // A pw-cat client appears in the graph and then leaves it.
    bool cycle()
    {
        ProcessGuard client;
        if (!client.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                          {QStringLiteral("--playback"), QStringLiteral("--raw"),
                           QStringLiteral("--format=s16"), QStringLiteral("--rate=48000"),
                           QStringLiteral("--channels=2"), QStringLiteral("--volume=0"),
                           QStringLiteral("/dev/zero")},
                          m_environment)
            || !waitFor([&] { return m_clientStreams == 1; }, 5000)) {
            return false;
        }
        client.stop();
        return waitFor([&] { return m_clientStreams == 0; }, 5000);
    }

    // Returns once no publication has arrived for half a second.
    void settle()
    {
        QElapsedTimer quiet;
        quiet.start();
        QElapsedTimer total;
        total.start();
        int seen = m_publications;
        while (quiet.elapsed() < 500 && total.elapsed() < 5000) {
            QTest::qWait(20);
            if (m_publications != seen) {
                seen = m_publications;
                quiet.restart();
            }
        }
    }

    [[nodiscard]] const Snapshot &snapshot() const { return m_coordinator->snapshot(); }

    ~ChurnRig()
    {
        m_coordinator.reset();
        m_backend.reset();
        m_wireplumber.stop();
        m_pipewire.stop();
    }

private:
    QTemporaryDir m_root{QStringLiteral("/tmp/qindaqt-audio-rack-churn-XXXXXX")};
    QProcessEnvironment m_environment;
    ProcessGuard m_pipewire;
    ProcessGuard m_wireplumber;
    std::unique_ptr<WirePlumberAudioBackend> m_backend;
    std::unique_ptr<AudioOperationCoordinator> m_coordinator;
    int m_publications = 0;
    int m_clientStreams = -1;
};

// Churns warm-up then measured cycles; returns the measured heap growth.
std::optional<qint64> churnGrowth(ChurnRig &rig)
{
    for (int index = 0; index < kWarmupCycles; ++index) {
        if (!rig.cycle()) {
            return std::nullopt;
        }
    }
    rig.settle();
    const qint64 before = heapInUse();
    for (int index = 0; index < kMeasuredCycles; ++index) {
        if (!rig.cycle()) {
            return std::nullopt;
        }
    }
    rig.settle();
    return heapInUse() - before;
}

QString growthMessage(const qint64 growth)
{
    return QStringLiteral("heap grew %1 bytes over %2 client streams (%3 per stream, bound %4)")
        .arg(growth)
        .arg(kMeasuredCycles)
        .arg(growth / kMeasuredCycles)
        .arg(kMaxGrowthPerCycle);
}

} // namespace

class WirePlumberRackChurnTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void anEqualizerRackLoadsAndStreamChurnKeepsTheHeapBounded();
    void aRefusedRackIsNotRetriedOnEveryGraphEvent();
};

void WirePlumberRackChurnTests::initTestCase()
{
#if !defined(__GLIBC__)
    QSKIP("the bound is measured with glibc's mallinfo2");
#endif
    registerDBusTypes();
    qRegisterMetaType<BackendOperationOutcome>();
}

// The equalizer is PipeWire's builtin biquads, so this rack needs no plugin
// package. It was refused on every graph event ("unknown input port
// eq_low:Input") and leaked the loaded biquads each time.
void WirePlumberRackChurnTests::anEqualizerRackLoadsAndStreamChurnKeepsTheHeapBounded()
{
    ChurnRig rig;
    QString failure;
    QVERIFY2(rig.start(&failure), qPrintable(failure));
    StripProcessing rack;
    rack.equalizer.enabled = true;
    QVERIFY(rig.declareRacks(rack));
    // Heap first: on the old worker the refusal storm is the failure under
    // test, and it shows before the absent processed sources do.
    rig.settle();
    const std::optional<qint64> growth = churnGrowth(rig);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    qInfo().noquote() << growthMessage(*growth);
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    QVERIFY(hasInput(rig.snapshot(), QStringLiteral("qindaqt.console.strip.hw.1.processed")));
    QVERIFY(hasInput(rig.snapshot(), QStringLiteral("qindaqt.console.strip.virtual.1.processed")));
}

// A rack PipeWire must refuse whatever the plugin set: an empty LADSPA path
// removes the limiter. The builtin equalizer ahead of it is loaded before
// the refusal, which is the leaking case; the worker must try it once.
void WirePlumberRackChurnTests::aRefusedRackIsNotRetriedOnEveryGraphEvent()
{
    // In-process filter-graph reads LADSPA_PATH at each plugin load. It is
    // set before the backend thread exists and restored after the rig (and
    // that thread) is gone: setenv must not race the worker's getenv.
    QTemporaryDir noPlugins(QStringLiteral("/tmp/qindaqt-audio-no-ladspa-XXXXXX"));
    QVERIFY(noPlugins.isValid());
    const QByteArray previousPath = qgetenv("LADSPA_PATH");
    const bool hadPath = qEnvironmentVariableIsSet("LADSPA_PATH");
    qputenv("LADSPA_PATH", noPlugins.path().toUtf8());
    const auto restorePath = qScopeGuard([&] {
        if (hadPath) {
            qputenv("LADSPA_PATH", previousPath);
        } else {
            qunsetenv("LADSPA_PATH");
        }
    });
    ChurnRig rig;
    QString failure;
    QVERIFY2(rig.start(&failure), qPrintable(failure));
    StripProcessing rack;
    rack.equalizer.enabled = true;
    rack.limiter.enabled = true;
    QVERIFY(rig.declareRacks(rack));
    rig.settle();
    const std::optional<qint64> growth = churnGrowth(rig);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    qInfo().noquote() << growthMessage(*growth);
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    // The scenario really was a refusal: neither strip gained a processed source.
    QVERIFY(!hasInput(rig.snapshot(), QStringLiteral("qindaqt.console.strip.hw.1.processed")));
    QVERIFY(!hasInput(rig.snapshot(),
                      QStringLiteral("qindaqt.console.strip.virtual.1.processed")));
}

QTEST_GUILESS_MAIN(WirePlumberRackChurnTests)
#include "tst_wireplumber_rack_churn.moc"
