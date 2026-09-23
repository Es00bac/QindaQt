// SPDX-License-Identifier: GPL-3.0-or-later

// Manual two-host private-runtime smoke for ADR-0246. It never touches the
// desktop's running PipeWire graph or default output.
#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>
#include <qindaqt/services/audio_protocol/audio_dbus.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtCore/QDebug>

#include <functional>

using namespace QindaQt::Audio;

namespace {

struct ProcessGuard {
    QProcess process;
    ~ProcessGuard() {
        if (process.state() == QProcess::NotRunning) return;
        process.terminate();
        if (!process.waitForFinished(2000)) {
            process.kill();
            process.waitForFinished(2000);
        }
    }
    bool start(const QString &program, const QStringList &args,
               const QProcessEnvironment &env) {
        process.setProcessEnvironment(env);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(program, args);
        return process.waitForStarted(5000);
    }
};

bool command(const QString &program, const QStringList &args,
             const QProcessEnvironment &env, QString *output = nullptr) {
    QProcess process;
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted(5000) || !process.waitForFinished(5000)) return false;
    if (output) *output = QString::fromUtf8(process.readAll());
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool waitUntil(const std::function<bool()> &ready, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (ready()) return true;
        QThread::msleep(20);
    }
    return false;
}

Handle findOutput(const Snapshot &snapshot, const QString &name) {
    for (const Device &device : snapshot.outputs)
        if (device.nodeName == name) return device.handle;
    return {};
}

bool toneFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    // An obvious, bounded nonzero stereo signal. The test validates samples,
    // not speaker audibility or a real user's media.
    QByteArray block;
    block.resize(48000 * 2 * 2);
    for (int frame = 0; frame < 48000; ++frame) {
        const qint16 sample = frame % 480 < 240 ? 12000 : -12000;
        for (int channel = 0; channel < 2; ++channel) {
            const int offset = (frame * 2 + channel) * 2;
            block[offset] = static_cast<char>(sample & 0xff);
            block[offset + 1] = static_cast<char>((sample >> 8) & 0xff);
        }
    }
    for (int second = 0; second < 10; ++second)
        if (file.write(block) != block.size()) return false;
    return true;
}

bool nonzeroPcm(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray pcm = file.readAll();
    if (pcm.size() < 48000 * 2 * 2) return false;
    qsizetype loud = 0;
    for (qsizetype i = 0; i + 1 < pcm.size(); i += 2) {
        const auto low = static_cast<quint8>(pcm.at(i));
        const auto high = static_cast<quint8>(pcm.at(i + 1));
        const auto sample = static_cast<qint16>(low | (high << 8));
        if (sample > 1000 || sample < -1000) ++loud;
    }
    qInfo() << "captured bytes" << pcm.size() << "nonzero samples" << loud;
    return loud > 48000;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    registerDBusTypes();
    if (argc != 4) {
        qCritical() << "usage: vban-peer-crosshost <send|receive> <peer-ipv4> <udp-port>";
        return 2;
    }
    const QString role = QString::fromLocal8Bit(argv[1]);
    const QString peer = QString::fromLocal8Bit(argv[2]);
    bool validPort = false;
    const int port = QString::fromLocal8Bit(argv[3]).toInt(&validPort);
    if ((role != QStringLiteral("send") && role != QStringLiteral("receive"))
        || !validPort || port < 1024 || port > 65535) return 2;

    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-vban-crosshost-XXXXXX"));
    if (!root.isValid()) return 3;
    const QString runtime = root.filePath(QStringLiteral("runtime"));
    const QString state = root.filePath(QStringLiteral("state"));
    const QString config = root.filePath(QStringLiteral("config"));
    if (!QDir().mkpath(runtime) || !QDir().mkpath(state) || !QDir().mkpath(config)) return 3;
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
    if (!pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                        {QStringLiteral("-c"), QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                        env)) return 4;
    if (!waitUntil([&] { return QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")); },
                   5000)) return 4;
    ProcessGuard policy;
    if (!policy.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                      {QStringLiteral("-p"), QStringLiteral("policy")}, env)) return 4;
    constexpr auto sinkName = "qindaqt.test.crosshost.output";
    QString diagnostics;
    if (!command(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                 {QStringLiteral("create-node"), QStringLiteral("adapter"),
                  QStringLiteral("{ factory.name = support.null-audio-sink "
                                 "node.name = %1 node.description = \"%1\" "
                                 "media.class = Audio/Sink object.linger = true "
                                 "audio.position = [ FL FR ] }")
                      .arg(QLatin1String(sinkName))}, env, &diagnostics)) {
        qCritical().noquote() << diagnostics;
        return 5;
    }
    WirePlumberAudioBackend backend;
    Snapshot snapshot;
    QStringList running;
    QObject::connect(&backend, &AudioBackend::snapshotReady, &app,
                     [&](quint64, const Snapshot &value) { snapshot = value; });
    QObject::connect(&backend, &AudioBackend::vbanRunningChanged, &app,
                     [&](quint64, const QStringList &value) { running = value; });
    if (backend.start() == 0) return 6;
    if (!waitUntil([&] { return snapshot.availability == Availability::Ready
                              && findOutput(snapshot, QLatin1String(sinkName)).isValid(); },
                   10000)) {
        qCritical() << "private output unavailable" << int(snapshot.availability)
                    << snapshot.reasonCode;
        return 6;
    }
    const Handle target = findOutput(snapshot, QLatin1String(sinkName));
    ProcessGuard playback;
    if (role == QStringLiteral("send")) {
        const QString path = root.filePath(QStringLiteral("tone.raw"));
        if (!toneFile(path)) return 8;
        if (!playback.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                            {QStringLiteral("--playback"), QStringLiteral("--raw"),
                             QStringLiteral("--format=s16"), QStringLiteral("--rate=48000"),
                             QStringLiteral("--channels=2"),
                             QStringLiteral("--target=") + QLatin1String(sinkName), path}, env))
            return 8;
    }
    backend.applyVban({BackendVbanStream{.name = QStringLiteral("Peer"),
                                        .outgoing = role == QStringLiteral("send"),
                                        .target = target,
                                        .host = peer,
                                        .port = static_cast<quint32>(port)}});
    if (!waitUntil([&] { return running.contains(QStringLiteral("Peer")); }, 10000)) {
        qCritical() << "local route did not activate";
        return 7;
    }
    QString graph;
    const bool routeReady = waitUntil([&] {
        if (!command(QStringLiteral(QINDAQT_PW_LINK_EXECUTABLE),
                     {QStringLiteral("-l")}, env, &graph)) return false;
        return role == QStringLiteral("receive")
            ? graph.contains(QStringLiteral("qindaqt.test.crosshost.output:playback_FL\n"
                                            "  |<- qindaqt.vban.route.Peer:output_FL"))
            : graph.contains(QStringLiteral("qindaqt.test.crosshost.output:monitor_FL\n"
                                            "  |-> qindaqt.vban.send.Peer:input_FL"));
    }, 5000);
    if (!routeReady) {
        qCritical().noquote() << "route mismatch" << graph;
        return 7;
    }
    if (role == QStringLiteral("receive")) {
        qInfo().noquote() << "READY receive" << peer << port;
        ProcessGuard capture;
        const QString capturePath = root.filePath(QStringLiteral("captured.raw"));
        if (!capture.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                           {QStringLiteral("--record"), QStringLiteral("--raw"),
                            QStringLiteral("--format=s16"), QStringLiteral("--rate=48000"),
                            QStringLiteral("--channels=2"),
                            QStringLiteral("--target=") + QLatin1String(sinkName),
                            QStringLiteral("--properties={ stream.capture.sink = true "
                                           "node.dont-move = true node.dont-reconnect = true "
                                           "node.dont-fallback = true }"),
                            QStringLiteral("--sample-count=240000"), capturePath}, env))
            return 8;
        if (!capture.process.waitForFinished(10000)) {
            qCritical().noquote() << capture.process.readAll();
            return 8;
        }
        // pw-cat may return 1 at its sample-count limit on some versions;
        // the bounded file's actual PCM is the acceptance evidence.
        if (!nonzeroPcm(capturePath)) {
            qCritical().noquote() << "no cross-host PCM"
                                  << "capture exit" << capture.process.exitCode()
                                  << "bytes" << QFileInfo(capturePath).size()
                                  << capture.process.readAll();
            return 8;
        }
        qInfo() << "CROSSHOST_PCM_PASS receive";
    } else {
        if (!playback.process.waitForFinished(15000) || playback.process.exitCode() != 0) {
            qCritical().noquote() << playback.process.readAll();
            return 8;
        }
        qInfo() << "CROSSHOST_SENT send";
    }
    backend.stop();
    return 0;
}
