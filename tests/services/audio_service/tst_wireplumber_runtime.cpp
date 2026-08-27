// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_dbus.h>
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

class ProcessGuard final
{
public:
    ~ProcessGuard()
    {
        stop();
    }

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

bool runCommand(const QString &program, const QStringList &arguments,
                const QProcessEnvironment &environment, QString *output = nullptr)
{
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    if (output != nullptr) {
        *output = QString::fromUtf8(process.readAll());
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

qsizetype openFileDescriptorCount()
{
    return QDir(QStringLiteral("/proc/self/fd"))
        .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
        .size();
}

std::optional<Snapshot> newestSnapshot(const QSignalSpy &spy)
{
    for (qsizetype index = spy.size(); index > 0; --index) {
        const Snapshot snapshot = spy.at(index - 1).at(1).value<Snapshot>();
        if (snapshot.availability == Availability::Ready
            || snapshot.availability == Availability::Degraded) {
            return snapshot;
        }
    }
    return std::nullopt;
}

std::optional<Snapshot> latestSnapshot(const QSignalSpy &spy)
{
    if (spy.isEmpty()) {
        return std::nullopt;
    }
    return spy.constLast().at(1).value<Snapshot>();
}

const Device *findDevice(const Snapshot &snapshot, const QString &name)
{
    for (const Device &device : snapshot.outputs) {
        if (device.name == name || device.description == name) {
            return &device;
        }
    }
    for (const Device &device : snapshot.inputs) {
        if (device.name == name || device.description == name) {
            return &device;
        }
    }
    return nullptr;
}

void stopWithDiagnostic(ProcessGuard &process, const char *label)
{
    const QByteArray output = process.process.readAll();
    process.stop();
    if (!output.isEmpty()) {
        qInfo().noquote() << label << QString::fromUtf8(output).trimmed();
    }
}

} // namespace

class WirePlumberRuntimeTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void runGenerationFencesStopAndRestart();
    void isolatedGraphOperationsAndAuthorityRestart();

private:
    void exerciseReconnectStress(WirePlumberAudioBackend &backend,
                                 QSignalSpy &snapshots, QSignalSpy &outcomes,
                                 ProcessGuard &wireplumber,
                                 const QProcessEnvironment &environment);
};

void WirePlumberRuntimeTests::runGenerationFencesStopAndRestart()
{
    registerDBusTypes();
    qRegisterMetaType<BackendOperationOutcome>();
    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-audio-generation-XXXXXX"));
    QVERIFY(root.isValid());
    const QString runtime = root.filePath(QStringLiteral("runtime"));
    const QString state = root.filePath(QStringLiteral("state"));
    const QString config = root.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(runtime));
    QVERIFY(QDir().mkpath(state));
    QVERIFY(QDir().mkpath(config));
    qputenv("XDG_RUNTIME_DIR", runtime.toUtf8());
    qputenv("PIPEWIRE_RUNTIME_DIR", runtime.toUtf8());
    qputenv("XDG_STATE_HOME", state.toUtf8());
    qputenv("XDG_CONFIG_HOME", config.toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS",
            QStringLiteral("unix:path=%1/no-session-bus").arg(root.path()).toUtf8());
    qunsetenv("PIPEWIRE_REMOTE");

    WirePlumberAudioBackend backend;
    QSignalSpy snapshots(&backend, &AudioBackend::snapshotReady);
    const qsizetype descriptorsBefore = openFileDescriptorCount();
    for (int iteration = 0; iteration < 250; ++iteration) {
        QVERIFY(backend.start() != 0);
        backend.stop();
    }
    const qsizetype descriptorsAfter = openFileDescriptorCount();
    QVERIFY2(descriptorsAfter <= descriptorsBefore + 5,
             qPrintable(QStringLiteral("FD growth after 250 cycles: %1 -> %2")
                            .arg(descriptorsBefore)
                            .arg(descriptorsAfter)));
    QCoreApplication::processEvents();
    QCOMPARE(snapshots.size(), 0);

    const quint64 establishedGeneration = backend.start();
    QTRY_VERIFY_WITH_TIMEOUT(!snapshots.isEmpty(), 5000);
    const quint64 establishedEpoch = latestSnapshot(snapshots)->epoch;
    QCOMPARE(snapshots.constLast().at(0).toULongLong(), establishedGeneration);
    backend.stop();
    const qsizetype stoppedCount = snapshots.size();
    QTest::qWait(5);
    QCOMPARE(snapshots.size(), stoppedCount);

    const quint64 supersededGeneration = backend.start();
    backend.stop();
    const quint64 currentGeneration = backend.start();
    QVERIFY(currentGeneration != supersededGeneration);
    const qsizetype restartStart = snapshots.size();
    QTRY_VERIFY_WITH_TIMEOUT(snapshots.size() > restartStart, 5000);
    for (qsizetype index = restartStart; index < snapshots.size(); ++index) {
        QCOMPARE(snapshots.at(index).at(0).toULongLong(), currentGeneration);
    }
    QVERIFY(latestSnapshot(snapshots)->epoch != establishedEpoch);
    backend.stop();
    const qsizetype finalCount = snapshots.size();
    QTest::qWait(5);
    QCOMPARE(snapshots.size(), finalCount);
}

void WirePlumberRuntimeTests::exerciseReconnectStress(
    WirePlumberAudioBackend &backend, QSignalSpy &snapshots, QSignalSpy &outcomes,
    ProcessGuard &wireplumber, const QProcessEnvironment &environment)
{
    quint64 stressEpoch = latestSnapshot(snapshots)->epoch;
    for (quint64 iteration = 0; iteration < 8; ++iteration) {
        const Snapshot beforeRestart = *latestSnapshot(snapshots);
        QVERIFY(!beforeRestart.outputs.isEmpty());
        const qsizetype priorOutcomes = outcomes.size();
        backend.submit(100 + iteration,
                       {.kind = OperationKind::SetMute,
                        .primary = beforeRestart.outputs.constFirst().handle,
                        .secondary = {},
                        .volume = 0.0,
                        .muted = (iteration % 2) == 0});
        wireplumber.stop();
        QTRY_VERIFY_WITH_TIMEOUT(outcomes.size() > priorOutcomes, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(latestSnapshot(snapshots).has_value()
                                     && latestSnapshot(snapshots)->epoch != stressEpoch,
                                 5000);
        stressEpoch = latestSnapshot(snapshots)->epoch;
        QVERIFY(wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                                  {QStringLiteral("-p"), QStringLiteral("policy")},
                                  environment));
        QTRY_VERIFY_WITH_TIMEOUT(
            latestSnapshot(snapshots).has_value()
                && latestSnapshot(snapshots)->epoch == stressEpoch
                && (latestSnapshot(snapshots)->availability == Availability::Ready
                    || latestSnapshot(snapshots)->availability
                        == Availability::Degraded)
                && !latestSnapshot(snapshots)->outputs.isEmpty(),
            10000);
    }
}

void WirePlumberRuntimeTests::isolatedGraphOperationsAndAuthorityRestart()
{
    registerDBusTypes();
    qRegisterMetaType<BackendOperationOutcome>();
    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-audio-runtime-XXXXXX"));
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

    // The adapter reads PipeWire's process environment on its GLib worker.
    // These values point only at this test's private socket and are restored by
    // QtTest's one-test-per-process lifetime before any other test can run.
    qputenv("XDG_RUNTIME_DIR", runtime.toUtf8());
    qputenv("PIPEWIRE_RUNTIME_DIR", runtime.toUtf8());
    qputenv("XDG_STATE_HOME", state.toUtf8());
    qputenv("XDG_CONFIG_HOME", config.toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS",
            environment.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).toUtf8());
    qunsetenv("PIPEWIRE_REMOTE");

    ProcessGuard pipewire;
    QVERIFY2(pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                            {QStringLiteral("-c"),
                             QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                            environment),
             qPrintable(QString::fromUtf8(pipewire.process.readAll())));
    QTRY_VERIFY_WITH_TIMEOUT(
        QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")), 5000);

    ProcessGuard wireplumber;
    QVERIFY2(wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                               {QStringLiteral("-p"), QStringLiteral("policy")},
                               environment),
             qPrintable(QString::fromUtf8(wireplumber.process.readAll())));

    const auto createNode = [&](const QString &properties) {
        QString output;
        const bool succeeded = runCommand(
            QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
            {QStringLiteral("create-node"), QStringLiteral("adapter"), properties},
            environment, &output);
        QVERIFY2(succeeded, qPrintable(output));
    };
    createNode(QStringLiteral(
        "{ factory.name = support.null-audio-sink node.name = qindaqt.test.output.1 "
        "node.description = \"QindaQt Test Output 1\" media.class = Audio/Sink "
        "object.linger = true audio.position = [ FL FR ] }"));
    createNode(QStringLiteral(
        "{ factory.name = support.null-audio-sink node.name = qindaqt.test.output.2 "
        "node.description = \"QindaQt Test Output 2\" media.class = Audio/Sink "
        "object.linger = true audio.position = [ FL FR ] }"));
    createNode(QStringLiteral(
        "{ factory.name = support.null-audio-sink node.name = qindaqt.test.input.1 "
        "node.description = \"QindaQt Test Input 1\" media.class = Audio/Source "
        "object.linger = true audio.position = [ FL FR ] }"));

    WirePlumberAudioBackend backend;
    QSignalSpy snapshots(&backend, &AudioBackend::snapshotReady);
    QSignalSpy outcomes(&backend, &AudioBackend::operationFinished);
    const quint64 backendGeneration = backend.start();
    QVERIFY(backendGeneration != 0);
    QTRY_VERIFY_WITH_TIMEOUT(newestSnapshot(snapshots).has_value(), 10000);
    Snapshot snapshot = *newestSnapshot(snapshots);
    QTRY_VERIFY_WITH_TIMEOUT(newestSnapshot(snapshots).has_value()
                                 && newestSnapshot(snapshots)->outputs.size() >= 2
                                 && !newestSnapshot(snapshots)->inputs.isEmpty(),
                             10000);
    snapshot = *newestSnapshot(snapshots);
    const Device *first = findDevice(snapshot, QStringLiteral("QindaQt Test Output 1"));
    const Device *second = findDevice(snapshot, QStringLiteral("QindaQt Test Output 2"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    const Handle firstHandle = first->handle;
    const Handle secondHandle = second->handle;
    const quint64 originalEpoch = snapshot.epoch;

    backend.submit(1, {.kind = OperationKind::SetVolume,
                       .primary = firstHandle,
                       .secondary = {},
                       .volume = 0.25});
    QTRY_COMPARE_WITH_TIMEOUT(outcomes.size(), 1, 5000);
    QCOMPARE(outcomes.at(0).at(2).value<BackendOperationOutcome>().status,
             BackendOperationStatus::Succeeded);
    backend.submit(2, {.kind = OperationKind::SetMute,
                       .primary = firstHandle,
                       .secondary = {},
                       .volume = 0.0,
                       .muted = true});
    QTRY_COMPARE_WITH_TIMEOUT(outcomes.size(), 2, 5000);
    QCOMPARE(outcomes.at(1).at(2).value<BackendOperationOutcome>().status,
             BackendOperationStatus::Succeeded);
    backend.submit(3, {.kind = OperationKind::SetDefault,
                       .primary = secondHandle,
                       .secondary = {},
                       .volume = 0.0,
                       .muted = false});
    QTRY_COMPARE_WITH_TIMEOUT(outcomes.size(), 3, 5000);
    QCOMPARE(outcomes.at(2).at(2).value<BackendOperationOutcome>().status,
             BackendOperationStatus::Succeeded);

    ProcessGuard playback;
    QVERIFY2(playback.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                            {QStringLiteral("--playback"), QStringLiteral("--raw"),
                             QStringLiteral("--rate=48000"),
                             QStringLiteral("--channels=2"),
                             QStringLiteral("/dev/zero")},
                            environment),
             qPrintable(QString::fromUtf8(playback.process.readAll())));
    QTRY_VERIFY_WITH_TIMEOUT(newestSnapshot(snapshots).has_value()
                                 && !newestSnapshot(snapshots)->streams.isEmpty(),
                             10000);
    snapshot = *newestSnapshot(snapshots);
    const Stream stream = snapshot.streams.constFirst();
    backend.submit(4, {.kind = OperationKind::MoveStream,
                       .primary = stream.handle,
                       .secondary = firstHandle,
                       .volume = 0.0,
                       .muted = false});
    QTRY_COMPARE_WITH_TIMEOUT(outcomes.size(), 4, 5000);
    QCOMPARE(outcomes.at(3).at(2).value<BackendOperationOutcome>().status,
             BackendOperationStatus::Succeeded);
    playback.stop();

    wireplumber.stop();
    QTRY_VERIFY_WITH_TIMEOUT(
        latestSnapshot(snapshots).has_value()
            && latestSnapshot(snapshots)->epoch != originalEpoch,
        10000);
    QVERIFY(wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                              {QStringLiteral("-p"), QStringLiteral("policy")},
                              environment));
    QTRY_VERIFY_WITH_TIMEOUT(latestSnapshot(snapshots).has_value()
                                 && latestSnapshot(snapshots)->epoch != originalEpoch
                                 && (latestSnapshot(snapshots)->availability
                                         == Availability::Ready
                                     || latestSnapshot(snapshots)->availability
                                         == Availability::Degraded),
                             10000);

    backend.submit(5, {.kind = OperationKind::SetMute,
                       .primary = firstHandle,
                       .secondary = {},
                       .volume = 0.0,
                       .muted = false});
    QTRY_COMPARE_WITH_TIMEOUT(outcomes.size(), 5, 5000);
    QCOMPARE(outcomes.at(4).at(2).value<BackendOperationOutcome>().status,
             BackendOperationStatus::Failed);
    QCOMPARE(outcomes.at(4).at(2).value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("stale-handle"));

    exerciseReconnectStress(backend, snapshots, outcomes, wireplumber, environment);

    backend.stop();
    stopWithDiagnostic(wireplumber, "wireplumber:");
    stopWithDiagnostic(pipewire, "pipewire:");
}

QTEST_GUILESS_MAIN(WirePlumberRuntimeTests)
#include "tst_wireplumber_runtime.moc"
