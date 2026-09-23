// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>
#include <qindaqt/services/audio_protocol/audio_dbus.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <optional>

using namespace QindaQt::Audio;

namespace {

struct ProcessGuard {
    QProcess process;
    ~ProcessGuard() {
        if (process.state() == QProcess::NotRunning) return;
        process.terminate();
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(3000);
        }
    }
    bool start(const QString &program, const QStringList &arguments,
               const QProcessEnvironment &environment) {
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(program, arguments);
        return process.waitForStarted(5000);
    }
};

bool command(const QString &program, const QStringList &arguments,
             const QProcessEnvironment &environment, QString *output = nullptr) {
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(5000)) return false;
    if (output) *output = QString::fromUtf8(process.readAll());
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

std::optional<Snapshot> readySnapshot(const QSignalSpy &spy) {
    for (qsizetype i = spy.size(); i > 0; --i) {
        const Snapshot snapshot = spy.at(i - 1).at(1).value<Snapshot>();
        if (snapshot.availability == Availability::Ready
            || snapshot.availability == Availability::Degraded)
            return snapshot;
    }
    return std::nullopt;
}

Handle outputHandle(const Snapshot &snapshot, const QString &nodeName) {
    for (const Device &device : snapshot.outputs)
        if (device.nodeName == nodeName) return device.handle;
    return {};
}

bool hasRunning(const QSignalSpy &spy, const QString &name) {
    if (spy.isEmpty()) return false;
    return spy.constLast().at(1).toStringList().contains(name);
}

} // namespace

class VbanPeerRuntimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void twoPrivateInstancesRouteOnlyToSelectedOutput();
};

void VbanPeerRuntimeTest::twoPrivateInstancesRouteOnlyToSelectedOutput() {
    registerDBusTypes();
    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-vban-peer-XXXXXX"));
    QVERIFY(root.isValid());
    const QString runtime = root.filePath(QStringLiteral("runtime"));
    const QString state = root.filePath(QStringLiteral("state"));
    const QString config = root.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(runtime));
    QVERIFY(QDir().mkpath(state));
    QVERIFY(QDir().mkpath(config));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
    env.insert(QStringLiteral("PIPEWIRE_RUNTIME_DIR"), runtime);
    env.insert(QStringLiteral("XDG_STATE_HOME"), state);
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
               QStringLiteral("unix:path=%1/no-session-bus").arg(root.path()));
    env.remove(QStringLiteral("PIPEWIRE_REMOTE"));
    qputenv("XDG_RUNTIME_DIR", runtime.toUtf8());
    qputenv("PIPEWIRE_RUNTIME_DIR", runtime.toUtf8());
    qputenv("XDG_STATE_HOME", state.toUtf8());
    qputenv("XDG_CONFIG_HOME", config.toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS",
            env.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).toUtf8());
    qunsetenv("PIPEWIRE_REMOTE");

    ProcessGuard pipewire;
    QVERIFY(pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                           {QStringLiteral("-c"), QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                           env));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")), 5000);
    ProcessGuard policy;
    QVERIFY(policy.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                         {QStringLiteral("-p"), QStringLiteral("policy")}, env));

    const auto createSink = [&](const QString &name) {
        QString diagnostic;
        const bool ok = command(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                                {QStringLiteral("create-node"), QStringLiteral("adapter"),
                                 QStringLiteral("{ factory.name = support.null-audio-sink "
                                                "node.name = %1 node.description = \"%1\" "
                                                "media.class = Audio/Sink object.linger = true "
                                                "audio.position = [ FL FR ] }").arg(name)},
                                env, &diagnostic);
        QVERIFY2(ok, qPrintable(diagnostic));
    };
    createSink(QStringLiteral("qindaqt.test.peer.output.1"));
    createSink(QStringLiteral("qindaqt.test.peer.output.2"));

    WirePlumberAudioBackend sender;
    WirePlumberAudioBackend receiver;
    QSignalSpy senderSnapshots(&sender, &AudioBackend::snapshotReady);
    QSignalSpy receiverSnapshots(&receiver, &AudioBackend::snapshotReady);
    QSignalSpy senderRunning(&sender, &AudioBackend::vbanRunningChanged);
    QSignalSpy receiverRunning(&receiver, &AudioBackend::vbanRunningChanged);
    QVERIFY(sender.start() != 0);
    QTRY_VERIFY_WITH_TIMEOUT(readySnapshot(senderSnapshots).has_value()
        && outputHandle(*readySnapshot(senderSnapshots),
                        QStringLiteral("qindaqt.test.peer.output.1")).isValid(), 10000);
    QVERIFY(receiver.start() != 0);
    QTRY_VERIFY_WITH_TIMEOUT(readySnapshot(receiverSnapshots).has_value()
        && outputHandle(*readySnapshot(receiverSnapshots),
                        QStringLiteral("qindaqt.test.peer.output.2")).isValid(), 10000);
    const Handle sendTarget = outputHandle(*readySnapshot(senderSnapshots),
                                             QStringLiteral("qindaqt.test.peer.output.1"));
    const Handle receiveTarget = outputHandle(*readySnapshot(receiverSnapshots),
                                                QStringLiteral("qindaqt.test.peer.output.2"));
    ProcessGuard silentPlayback;
    QVERIFY(silentPlayback.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                                 {QStringLiteral("--playback"), QStringLiteral("--raw"),
                                  QStringLiteral("--rate=48000"), QStringLiteral("--channels=2"),
                                  QStringLiteral("--target=qindaqt.test.peer.output.1"),
                                  QStringLiteral("/dev/zero")}, env));
    const quint32 port = 42000 + static_cast<quint32>(QCoreApplication::applicationPid() % 10000);
    receiver.applyVban({BackendVbanStream{.name = QStringLiteral("Peer"),
                                          .outgoing = false,
                                          .target = receiveTarget,
                                          .host = QStringLiteral("127.0.0.1"),
                                          .port = port}});
    sender.applyVban({BackendVbanStream{.name = QStringLiteral("Peer"),
                                        .outgoing = true,
                                        .target = sendTarget,
                                        .host = QStringLiteral("127.0.0.1"),
                                        .port = port}});
    QTRY_VERIFY_WITH_TIMEOUT(hasRunning(receiverRunning, QStringLiteral("Peer")), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(hasRunning(senderRunning, QStringLiteral("Peer")), 10000);

    const auto links = [&]() {
        QString graph;
        const bool ok = command(QStringLiteral(QINDAQT_PW_LINK_EXECUTABLE),
                                {QStringLiteral("-l")}, env, &graph);
        return ok ? graph : QString{};
    };
    const auto onExactTargets = [&]() {
        const QString graph = links();
        return graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.1:monitor_FL\n"
                   "  |-> qindaqt.vban.send.Peer:input_FL"))
            && graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.1:monitor_FR\n"
                   "  |-> qindaqt.vban.send.Peer:input_FR"))
            && graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.2:playback_FL\n"
                   "  |<- qindaqt.vban.route.Peer:output_FL"))
            && graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.2:playback_FR\n"
                   "  |<- qindaqt.vban.route.Peer:output_FR"))
            && !graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.1:playback_FL\n"
                   "  |<- qindaqt.vban.route.Peer"))
            && !graph.contains(QStringLiteral(
                   "qindaqt.test.peer.output.2:monitor_FL\n"
                   "  |-> qindaqt.vban.send.Peer"));
    };
    QVERIFY(onExactTargets());

    // AGENT-GUARD: WirePlumber may rewrite target.node to -1 when an exact
    // target is also the default. Rescans and default changes must preserve
    // both pinned paths; a missing selected node must fail closed.
    QVERIFY(command(QStringLiteral(QINDAQT_PW_METADATA_EXECUTABLE),
                    {QStringLiteral("-n"), QStringLiteral("default"), QStringLiteral("0"),
                     QStringLiteral("default.audio.sink"),
                     QStringLiteral("{\"name\":\"qindaqt.test.peer.output.2\"}"),
                     QStringLiteral("Spa:String:JSON")}, env));
    QTest::qWait(500);
    QVERIFY(onExactTargets());
    QVERIFY(hasRunning(senderRunning, QStringLiteral("Peer")));
    QVERIFY(hasRunning(receiverRunning, QStringLiteral("Peer")));

    const auto nodeId = [&](const QString &name) -> QString {
        QString inventory;
        if (!command(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                     {QStringLiteral("ls"), QStringLiteral("Node")}, env, &inventory))
            return {};
        QString currentId;
        const QRegularExpression idPattern(QStringLiteral("^id (\\d+),"));
        for (const QString &line : inventory.split(QLatin1Char('\n'))) {
            const auto match = idPattern.match(line.trimmed());
            if (match.hasMatch()) currentId = match.captured(1);
            if (line.contains(QStringLiteral("node.name = \"%1\"").arg(name)))
                return currentId;
        }
        return {};
    };
    const QString receiveNodeId = nodeId(QStringLiteral("qindaqt.test.peer.output.2"));
    QVERIFY(!receiveNodeId.isEmpty());
    QVERIFY(command(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                    {QStringLiteral("destroy"), receiveNodeId}, env));
    QTRY_VERIFY_WITH_TIMEOUT(!hasRunning(receiverRunning, QStringLiteral("Peer")), 5000);
    QVERIFY(!links().contains(QStringLiteral(
        "qindaqt.test.peer.output.1:playback_FL\n"
        "  |<- qindaqt.vban.route.Peer")));

    const QString sendNodeId = nodeId(QStringLiteral("qindaqt.test.peer.output.1"));
    QVERIFY(!sendNodeId.isEmpty());
    QVERIFY(command(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                    {QStringLiteral("destroy"), sendNodeId}, env));
    QTRY_VERIFY_WITH_TIMEOUT(!hasRunning(senderRunning, QStringLiteral("Peer")), 5000);
    sender.stop();
    receiver.stop();
}

QTEST_MAIN(VbanPeerRuntimeTest)
#include "tst_vban_peer_runtime.moc"
