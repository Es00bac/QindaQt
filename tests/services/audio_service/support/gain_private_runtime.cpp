// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_private_runtime.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

namespace QindaQt::Audio::Test
{
PrivateGainRuntime::PrivateGainRuntime()
    : m_root(QStringLiteral("/tmp/qindaqt-console-gain-XXXXXX"))
{
}
PrivateGainRuntime::~PrivateGainRuntime()
{
    stopProcess(m_policy);
    stopProcess(m_pipewire);
    for (auto it = m_previous.cbegin(); it != m_previous.cend(); ++it) {
        if (it.value()) qputenv(it.key().constData(), *it.value());
        else qunsetenv(it.key().constData());
    }
}
bool PrivateGainRuntime::startProcess(QProcess &process, const QString &program,
                                      const QStringList &arguments)
{
    QProcess::UnixProcessParameters parameters;
    parameters.flags = QProcess::UnixProcessFlag::CloseFileDescriptors;
    parameters.lowestFileDescriptorToClose = 3;
    process.setUnixProcessParameters(parameters);
    process.setProcessEnvironment(m_environment);
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    return process.waitForStarted(5000);
}
void PrivateGainRuntime::stopProcess(QProcess &process)
{
    if (process.state() == QProcess::NotRunning) return;
    process.terminate();
    if (!process.waitForFinished(2000)) {
        process.kill();
        (void)process.waitForFinished(2000);
    }
}
bool PrivateGainRuntime::start(const bool savedLoudState)
{
    if (m_started) return ready();
    // Artifact and dynamic-loader proof precede even private daemon startup.
    // The receipt is a configure-time pin; no upstream version-string fallback.
    if (!m_image.preflight(&m_failure)) return false;
    if (!m_root.isValid()) { m_failure = QStringLiteral("private-root"); return false; }
    const QStringList directories = {QStringLiteral("runtime"), QStringLiteral("config"),
        QStringLiteral("state"), QStringLiteral("cache"), QStringLiteral("data"),
        QStringLiteral("pw-config"), QStringLiteral("wp-config")};
    for (const QString &directory : directories) {
        const QString path = m_root.filePath(directory);
        if (!QDir().mkpath(path) || !QFile::setPermissions(path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
            m_failure = QStringLiteral("private-directory"); return false;
        }
    }
    if (!QFile::copy(QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG),
                      m_root.filePath(QStringLiteral("pw-config/pipewire.conf")))
        || !QFile::copy(QStringLiteral(QINDAQT_PIPEWIRE_CLIENT_TEST_CONFIG),
                       m_root.filePath(QStringLiteral("pw-config/client.conf")))
        || !QFile::copy(QStringLiteral(QINDAQT_WIREPLUMBER_TEST_CONFIG),
                       m_root.filePath(QStringLiteral("wp-config/wireplumber.conf")))) {
        m_failure = QStringLiteral("private-config-copy"); return false;
    }
    QMap<QByteArray, QByteArray> values = {
        {"XDG_RUNTIME_DIR", m_root.filePath(QStringLiteral("runtime")).toUtf8()},
        {"PIPEWIRE_RUNTIME_DIR", m_root.filePath(QStringLiteral("runtime")).toUtf8()},
        {"PIPEWIRE_REMOTE", "pipewire-0"},
        {"PIPEWIRE_CORE", "pipewire-0"},
        {"PIPEWIRE_CONFIG_DIR", m_root.filePath(QStringLiteral("pw-config")).toUtf8()},
        {"WIREPLUMBER_CONFIG_DIR", m_root.filePath(QStringLiteral("wp-config")).toUtf8()},
        {"XDG_CONFIG_HOME", m_root.filePath(QStringLiteral("config")).toUtf8()},
        {"XDG_CONFIG_DIRS", m_root.filePath(QStringLiteral("config")).toUtf8()},
        {"XDG_STATE_HOME", m_root.filePath(QStringLiteral("state")).toUtf8()},
        {"XDG_CACHE_HOME", m_root.filePath(QStringLiteral("cache")).toUtf8()},
        {"XDG_DATA_HOME", m_root.filePath(QStringLiteral("data")).toUtf8()},
        {"XDG_DATA_DIRS", "/usr/share"},
        {"DBUS_SESSION_BUS_ADDRESS", ("unix:path=" + m_root.path() + "/absent-session").toUtf8()},
        {"DBUS_SYSTEM_BUS_ADDRESS", ("unix:path=" + m_root.path() + "/absent-system").toUtf8()}};
    const auto pinnedLoader = m_image.bootstrapValues();

    // Child environments are explicit. In particular PIPEWIRE_CORE can be
    // an absolute listening-socket path even when XDG_RUNTIME_DIR is private.
    m_environment = QProcessEnvironment{};
    m_environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
    m_environment.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    m_environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    // Native library composition also runs in this process. Scrub every
    // ambient transport/config/plugin/daemon override, not a guessed subset;
    // restore the exact previous state only after graphs/processes have died.
    const auto ambient = QProcessEnvironment::systemEnvironment();
    for (const QString &name : ambient.keys()) {
        const bool nativeOverride = name.startsWith(QLatin1String("PIPEWIRE_"))
            || name.startsWith(QLatin1String("WIREPLUMBER_"))
            || name.startsWith(QLatin1String("SPA_"))
            || name.startsWith(QLatin1String("DBUS_"))
            || name.startsWith(QLatin1String("LISTEN_"))
            || name == QLatin1String("NOTIFY_SOCKET")
            || name == QLatin1String("WATCHDOG_PID")
            || name == QLatin1String("WATCHDOG_USEC")
            || name == QLatin1String("PULSE_SERVER");
        if (!nativeOverride || pinnedLoader.contains(name.toLatin1())) continue;
        const QByteArray key = name.toLatin1();
        m_previous.insert(key, qgetenv(key.constData()));
        qunsetenv(key.constData());
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!m_previous.contains(it.key())) {
            m_previous.insert(it.key(), qEnvironmentVariableIsSet(it.key().constData())
                ? std::optional<QByteArray>{qgetenv(it.key().constData())} : std::nullopt);
        }
        qputenv(it.key().constData(), it.value());
        m_environment.insert(QString::fromLatin1(it.key()), QString::fromUtf8(it.value()));
    }
    // Loader variables already pinned before exec are never rewritten here.
    for (auto it = pinnedLoader.cbegin(); it != pinnedLoader.cend(); ++it)
        m_environment.insert(QString::fromLatin1(it.key()), QString::fromUtf8(it.value()));
    if (savedLoudState) {
        // A fresh helper process avoids GLib's cached XDG state directory from
        // a preceding test. This uses public WpState in the private roots and
        // proves policy restoration with a real legacy positive control.
        QProcess seed;
        if (!startProcess(seed, QStringLiteral(QINDAQT_GAIN_STATE_SEED_EXECUTABLE),
                          {QStringLiteral("--seed-private-state")})
            || !seed.waitForFinished(5000) || seed.exitStatus() != QProcess::NormalExit
            || seed.exitCode() != 0) {
            stopProcess(seed); m_failure = QStringLiteral("private-state-seed"); return false;
        }
    }
    if (!startProcess(m_pipewire, QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE),
                       {QStringLiteral("-c"), QStringLiteral("pipewire.conf")})) {
        m_failure = QStringLiteral("private-daemon-start"); return false;
    }
    QElapsedTimer timer; timer.start();
    while (!QFileInfo::exists(m_root.filePath(QStringLiteral("runtime/pipewire-0")))
           && timer.elapsed() < 5000 && m_pipewire.state() != QProcess::NotRunning) {
        QThread::msleep(1);
    }
    // The socket file is startup readiness only. NativeGraph still requires a
    // real connected Core, installed manager, producer serial and API plugins.
    if (!QFileInfo::exists(m_root.filePath(QStringLiteral("runtime/pipewire-0")))
        || !startProcess(m_policy, QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE),
                         {QStringLiteral("-p"), QStringLiteral("policy")})) {
        m_failure = QStringLiteral("private-policy-start"); return false;
    }
    QElapsedTimer mapsTimer; mapsTimer.start();
    while (mapsTimer.elapsed() < 5000
           && (!m_image.privateChild(m_pipewire.processId())
               || !m_image.privateChild(m_policy.processId()))) {
        if (m_pipewire.state() == QProcess::NotRunning
            || m_policy.state() == QProcess::NotRunning) break;
        QThread::msleep(1);
    }
    if (!m_image.privateChild(m_pipewire.processId())
        || !m_image.privateChild(m_policy.processId())) {
        m_failure = QStringLiteral("qualified-child-image");
        return false;
    }
    m_started = true;
    return ready();
}
bool PrivateGainRuntime::ready() const
{
    return m_started && m_image.bootstrapStillPinned() && m_pipewire.state() != QProcess::NotRunning
        && m_policy.state() != QProcess::NotRunning;
}
QString PrivateGainRuntime::statePath() const
{
    return m_root.filePath(QStringLiteral("state"));
}
void PrivateGainRuntime::stopPolicy() { stopProcess(m_policy); }
} // namespace QindaQt::Audio::Test
