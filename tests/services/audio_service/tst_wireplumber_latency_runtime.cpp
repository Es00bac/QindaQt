// SPDX-License-Identifier: GPL-3.0-or-later

// Latency offsets against a private PipeWire and WirePlumber (ADR-0288). An
// ALSA "null" PCM sink publishes latencyOffsetNsec with ALSA's 0..2 s range,
// as a hardware sink does; the production backend reads the offset and range,
// applies a declared offset, answers one external reset, and leaves the node
// alone once nothing is declared. A null-audio-sink device stays absent. The
// user's session bus and audio graph are never contacted.

#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <optional>

using namespace QindaQt::Audio;

namespace
{

constexpr qint64 kMs = 1'000'000;
const QString kAlsaSink = QStringLiteral("qindaqt.test.alsa");
const QString kNullSink = QStringLiteral("qindaqt.test.null");

class ProcessGuard final
{
public:
    ~ProcessGuard() { stop(); }
    bool start(const QString &program, const QStringList &arguments,
               const QProcessEnvironment &environment)
    {
        process.setProcessEnvironment(environment);
        process.setProgram(program);
        process.setArguments(arguments);
        process.setProcessChannelMode(QProcess::MergedChannels);
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

bool pwCli(const QStringList &arguments, const QProcessEnvironment &environment,
           QString *output = nullptr)
{
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE), arguments);
    const bool finished = process.waitForStarted(5000) && process.waitForFinished(5000);
    if (!finished) {
        process.kill();
        process.waitForFinished();
    }
    if (output != nullptr) {
        *output = QString::fromUtf8(process.readAll());
    }
    return finished && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

std::optional<Device> newestDevice(const QSignalSpy &snapshots, const QString &nodeName)
{
    if (snapshots.isEmpty()) {
        return std::nullopt;
    }
    const auto snapshot = snapshots.constLast().at(1).value<Snapshot>();
    for (const Device &device : snapshot.outputs) {
        if (device.nodeName == nodeName) {
            return device;
        }
    }
    return std::nullopt;
}

} // namespace

class WirePlumberLatencyRuntimeTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesDeclaredOffsetsToAPrivateGraph();
};

void WirePlumberLatencyRuntimeTests::appliesDeclaredOffsetsToAPrivateGraph()
{
    registerDBusTypes();
    qRegisterMetaType<BackendOperationOutcome>();
    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-audio-latency-XXXXXX"));
    QVERIFY(root.isValid());
    const QString runtime = root.filePath(QStringLiteral("runtime"));
    const QString state = root.filePath(QStringLiteral("state"));
    const QString config = root.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(runtime));
    QVERIFY(QDir().mkpath(state));
    QVERIFY(QDir().mkpath(config));
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
    environment.insert(QStringLiteral("PIPEWIRE_RUNTIME_DIR"), runtime);
    environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                       QStringLiteral("unix:path=%1/no-session-bus").arg(root.path()));
    environment.remove(QStringLiteral("PIPEWIRE_REMOTE"));
    // The adapter reads these on its GLib worker; QtTest's one-test process
    // lifetime keeps them from reaching any other test.
    for (const char *name : {"XDG_RUNTIME_DIR", "PIPEWIRE_RUNTIME_DIR", "XDG_STATE_HOME",
                             "XDG_CONFIG_HOME", "DBUS_SESSION_BUS_ADDRESS"}) {
        qputenv(name, environment.value(QString::fromLatin1(name)).toUtf8());
    }
    qunsetenv("PIPEWIRE_REMOTE");

    ProcessGuard pipewire;
    QVERIFY(pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                           {QStringLiteral("-c"), QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                           environment));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")), 5000);
    ProcessGuard wireplumber;
    QVERIFY(wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                              {QStringLiteral("-p"), QStringLiteral("policy")}, environment));
    QString output;
    QVERIFY2(pwCli({QStringLiteral("create-node"), QStringLiteral("adapter"),
                    QStringLiteral("{ factory.name = api.alsa.pcm.sink node.name = %1 "
                                   "node.description = \"QindaQt Test ALSA\" "
                                   "api.alsa.path = \"null\" media.class = Audio/Sink "
                                   "object.linger = true audio.channels = 2 "
                                   "audio.position = [ FL FR ] }")
                        .arg(kAlsaSink)},
                   environment, &output),
             qPrintable(output));
    QVERIFY2(pwCli({QStringLiteral("create-node"), QStringLiteral("adapter"),
                    QStringLiteral("{ factory.name = support.null-audio-sink node.name = %1 "
                                   "media.class = Audio/Sink object.linger = true "
                                   "audio.position = [ FL FR ] }")
                        .arg(kNullSink)},
                   environment, &output),
             qPrintable(output));

    WirePlumberAudioBackend backend;
    QSignalSpy snapshots(&backend, &AudioBackend::snapshotReady);
    QVERIFY(backend.start() != 0);
    // The ALSA sink becomes settable once its PropInfo range has been read.
    QTRY_VERIFY_WITH_TIMEOUT(newestDevice(snapshots, kAlsaSink).has_value()
                                 && newestDevice(snapshots, kAlsaSink)->canSetLatencyOffset,
                             10000);
    Device alsa = *newestDevice(snapshots, kAlsaSink);
    QVERIFY(alsa.latencyOffsetKnown);
    QCOMPARE(alsa.latencyOffsetNs, qint64(0));
    QCOMPARE(alsa.latencyOffsetMinNs, qint64(0));
    QCOMPARE(alsa.latencyOffsetMaxNs, kMaxLatencyOffsetNs);
    QTRY_VERIFY_WITH_TIMEOUT(newestDevice(snapshots, kNullSink).has_value(), 5000);
    const Device null = *newestDevice(snapshots, kNullSink);
    QVERIFY(!null.latencyOffsetKnown);
    QVERIFY(!null.canSetLatencyOffset);
    QVERIFY(snapshots.constLast().at(1).value<Snapshot>().capabilities.testFlag(
        Capability::SetLatencyOffset));

    // A declaration reaches the node and comes back as graph truth.
    backend.applyLatencyOffsets({{.nodeName = kAlsaSink, .offsetNs = 120 * kMs},
                                 {.nodeName = kNullSink, .offsetNs = 50 * kMs}});
    QTRY_COMPARE_WITH_TIMEOUT(newestDevice(snapshots, kAlsaSink)->latencyOffsetNs,
                              120 * kMs, 5000);
    QVERIFY(!newestDevice(snapshots, kNullSink)->latencyOffsetKnown);

    // Something else resets the node, as a Bluetooth route does on connect:
    // the declared offset is written once more.
    QVERIFY2(pwCli({QStringLiteral("set-param"), kAlsaSink, QStringLiteral("Props"),
                    QStringLiteral("{ latencyOffsetNsec = 0 }")},
                   environment, &output),
             qPrintable(output));
    QTRY_COMPARE_WITH_TIMEOUT(newestDevice(snapshots, kAlsaSink)->latencyOffsetNs,
                              120 * kMs, 5000);

    // With nothing declared the node's own value stands.
    backend.applyLatencyOffsets({});
    QVERIFY2(pwCli({QStringLiteral("set-param"), kAlsaSink, QStringLiteral("Props"),
                    QStringLiteral("{ latencyOffsetNsec = 30000000 }")},
                   environment, &output),
             qPrintable(output));
    QTRY_COMPARE_WITH_TIMEOUT(newestDevice(snapshots, kAlsaSink)->latencyOffsetNs,
                              30 * kMs, 5000);
    QTest::qWait(500);
    QCOMPARE(newestDevice(snapshots, kAlsaSink)->latencyOffsetNs, 30 * kMs);

    backend.stop();
    wireplumber.stop();
    pipewire.stop();
}

QTEST_GUILESS_MAIN(WirePlumberLatencyRuntimeTests)
#include "tst_wireplumber_latency_runtime.moc"
