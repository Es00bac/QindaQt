// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_audio_churn.h"

#include <qindaqt/services/audio_protocol/audio_dbus.h>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtTest>

#ifdef __GLIBC__
#include <malloc.h>
#endif

#include <functional>
#include <mutex>

namespace QindaQt::Audio::Test
{
namespace
{

int clientStreams(const Snapshot &snapshot)
{
    int count = 0;
    for (const Stream &stream : snapshot.streams) {
        if (stream.applicationName.contains(QLatin1String("pw-cat"))) {
            ++count;
        }
    }
    return count;
}

struct Tally {
    std::mutex mutex;
    QtMessageHandler previous = nullptr;
    QHash<QString, int> counts;
};

Tally &tally()
{
    static Tally instance;
    return instance;
}

void countMessage(const QtMsgType type, const QMessageLogContext &context,
                  const QString &message)
{
    Tally &state = tally();
    QtMessageHandler previous = nullptr;
    {
        std::lock_guard lock(state.mutex);
        for (auto it = state.counts.begin(); it != state.counts.end(); ++it) {
            if (message.contains(it.key())) {
                ++it.value();
            }
        }
        previous = state.previous;
    }
    if (previous != nullptr) {
        previous(type, context, message);
    }
}

} // namespace

qint64 heapInUse()
{
#ifdef __GLIBC__
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

bool hasInput(const Snapshot &snapshot, const QString &nodeName)
{
    for (const Device &device : snapshot.inputs) {
        if (device.nodeName == nodeName) {
            return true;
        }
    }
    return false;
}

bool ChildProcess::start(const QString &program, const QStringList &arguments,
                         const QProcessEnvironment &environment, const QString &logPath)
{
    m_process.setProcessEnvironment(environment);
    m_process.setProgram(program);
    m_process.setArguments(arguments);
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_process.setStandardOutputFile(logPath.isEmpty() ? QProcess::nullDevice() : logPath);
    m_process.start();
    return m_process.waitForStarted(5000);
}

void ChildProcess::stop()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(3000)) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
}

PrivateAudioChurn::PrivateAudioChurn()
    : m_root(QStringLiteral("/tmp/qindaqt-audio-churn-XXXXXX"))
{
    registerDBusTypes();
    qRegisterMetaType<BackendOperationOutcome>();
}

PrivateAudioChurn::~PrivateAudioChurn()
{
    m_coordinator.reset();
    m_backend.reset();
    m_wireplumber.stop();
    m_pipewire.stop();
}

bool PrivateAudioChurn::start(QString *failure)
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
    // A row's backend-only overrides never reach the daemons or the clients.
    m_environment.remove(QStringLiteral("PIPEWIRE_MODULE_DIR"));
    m_environment.remove(QStringLiteral("LADSPA_PATH"));
    // The backend reads PipeWire's environment on its GLib worker. These
    // name only this rig's private socket; QtTest runs one process.
    for (const char *key : {"XDG_RUNTIME_DIR", "PIPEWIRE_RUNTIME_DIR", "XDG_STATE_HOME",
                            "XDG_CONFIG_HOME", "DBUS_SESSION_BUS_ADDRESS"}) {
        qputenv(key, m_environment.value(QString::fromLatin1(key)).toUtf8());
    }
    qunsetenv("PIPEWIRE_REMOTE");
    qunsetenv("PIPEWIRE_CORE");

    if (!m_pipewire.start(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                          {QStringLiteral("-c"), QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)},
                          m_environment, m_root.filePath(QStringLiteral("pipewire.log")))
        || !waitFor([&] { return QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")); },
                    5000)
        || !m_wireplumber.start(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                                {QStringLiteral("-p"), QStringLiteral("policy")}, m_environment,
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
    // Only the latest value is kept: a growing record of snapshots would be
    // heap growth of the test's own making.
    QObject::connect(m_coordinator.get(), &AudioOperationCoordinator::snapshotChanged,
                     m_coordinator.get(), [this](const Snapshot &snapshot) {
                         ++m_publications;
                         if (snapshot.availability == Availability::Ready
                             || snapshot.availability == Availability::Degraded) {
                             m_clientStreams = clientStreams(snapshot);
                         }
                     });
    m_coordinator->start();
    // Bound means the fixture input is in the graph and the first virtual
    // strip's own sink (a raw-proxy null sink, never a module) exists.
    if (!waitFor([&] {
            const Snapshot &now = m_coordinator->snapshot();
            bool virtualStrip = false;
            for (const Device &device : now.outputs) {
                virtualStrip = virtualStrip
                    || device.nodeName == QLatin1String("qindaqt.console.strip.virtual.1");
            }
            return m_clientStreams == 0 && virtualStrip
                && hasInput(now, QStringLiteral("qindaqt.test.churn.input"));
        }, 15000)) {
        *failure = QStringLiteral("console never bound to the private graph");
        return false;
    }
    settle();
    return true;
}

bool PrivateAudioChurn::apply(const OperationRequest &request)
{
    return m_coordinator->submit(request).immediateResult.status
        == OperationStatus::Succeeded;
}

bool PrivateAudioChurn::runTool(const QString &program, const QStringList &arguments)
{
    QProcess tool;
    tool.setProcessEnvironment(m_environment);
    tool.setStandardOutputFile(QProcess::nullDevice());
    tool.setStandardErrorFile(QProcess::nullDevice());
    tool.start(program, arguments);
    return tool.waitForFinished(5000) && tool.exitStatus() == QProcess::NormalExit
        && tool.exitCode() == 0;
}

bool PrivateAudioChurn::cycle(const QString &nodeName, const bool capture)
{
    ChildProcess client;
    QStringList arguments{capture ? QStringLiteral("--record") : QStringLiteral("--playback"),
                          QStringLiteral("--raw"), QStringLiteral("--format=s16"),
                          QStringLiteral("--rate=48000"), QStringLiteral("--channels=2"),
                          QStringLiteral("--volume=0")};
    if (!nodeName.isEmpty()) {
        arguments.append(QStringLiteral("--properties={ node.name = \"%1\" }").arg(nodeName));
    }
    arguments.append(capture ? QStringLiteral("/dev/null") : QStringLiteral("/dev/zero"));
    if (!client.start(QStringLiteral(QINDAQT_PW_CAT_EXECUTABLE),
                      arguments,
                      m_environment)
        || !waitFor([&] { return m_clientStreams == 1; }, 5000)) {
        return false;
    }
    client.stop();
    return waitFor([&] { return m_clientStreams == 0; }, 5000);
}

void PrivateAudioChurn::settle()
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

std::optional<qint64> PrivateAudioChurn::churnGrowth(const int warmup, const int measured)
{
    for (int index = 0; index < warmup; ++index) {
        if (!cycle()) {
            return std::nullopt;
        }
    }
    settle();
    const qint64 before = heapInUse();
    for (int index = 0; index < measured; ++index) {
        if (!cycle()) {
            return std::nullopt;
        }
    }
    settle();
    return heapInUse() - before;
}

const Snapshot &PrivateAudioChurn::snapshot() const
{
    return m_coordinator->snapshot();
}

WarningTally::WarningTally(QStringList fragments)
{
    Tally &state = tally();
    std::lock_guard lock(state.mutex);
    state.counts.clear();
    for (const QString &fragment : fragments) {
        state.counts.insert(fragment, 0);
    }
    state.previous = qInstallMessageHandler(countMessage);
}

WarningTally::~WarningTally()
{
    Tally &state = tally();
    std::lock_guard lock(state.mutex);
    qInstallMessageHandler(state.previous);
    state.previous = nullptr;
    state.counts.clear();
}

int WarningTally::count(const QString &fragment) const
{
    Tally &state = tally();
    std::lock_guard lock(state.mutex);
    return state.counts.value(fragment, 0);
}

} // namespace QindaQt::Audio::Test
