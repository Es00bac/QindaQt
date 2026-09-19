// SPDX-License-Identifier: LGPL-3.0-or-later
#include "screensaver_launcher.h"

#include <KIdleTime>

#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr auto kScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto kScreenSaverPath = "/ScreenSaver";

QStringList defaultArguments(const QString &program)
{
    const QString name = program.section(QLatin1Char('/'), -1);
    if (name == QLatin1String("qinda-patrol")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")};
    }
    if (name == QLatin1String("circuit-reef")) {
        return {QStringLiteral("--all-screens"), QStringLiteral("--private")};
    }
    return {};
}

} // namespace

ScreensaverLauncher::ScreensaverLauncher(QDBusConnection sessionBus, QObject *parent)
    : QObject(parent), m_bus(std::move(sessionBus))
{
    m_configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/qindaqt/screensaver.json");
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(2000);
    m_relaunchTimer.setSingleShot(true);
    m_relaunchTimer.setInterval(1000);

    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
        }
    });
    connect(&m_relaunchTimer, &QTimer::timeout, this, [this] {
        // The saver exits on an output topology change; bring it back while
        // the session is still idle and unlocked.
        if (m_idle && !m_locked && m_enabled) {
            launch();
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        m_killTimer.stop();
        if (m_idle && !m_locked && m_enabled) {
            m_relaunchTimer.start();
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            QTextStream(stderr) << "qindaqt-desktop-controls: screensaver failed to start: "
                                << m_program << '\n';
            m_idle = false;
        }
    });

    auto *const idleTime = KIdleTime::instance();
    connect(idleTime, &KIdleTime::timeoutReached, this, [this](int identifier, int) {
        if (identifier != m_idleToken) {
            return;
        }
        m_idle = true;
        KIdleTime::instance()->catchNextResumeEvent();
        if (!m_locked && !sessionLocked()) {
            launch();
        }
    });
    connect(idleTime, &KIdleTime::resumingFromIdle, this, [this] {
        m_idle = false;
        m_relaunchTimer.stop();
        stop();
    });

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        loadConfiguration();
        armIdleTimeout();
        if (!m_watcher.files().contains(m_configPath) && QFile::exists(m_configPath)) {
            m_watcher.addPath(m_configPath);
        }
    });
}

ScreensaverLauncher::~ScreensaverLauncher()
{
    disarmIdleTimeout();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

void ScreensaverLauncher::start()
{
    m_bus.connect(QString::fromLatin1(kScreenSaverService), QString::fromLatin1(kScreenSaverPath),
                  QString::fromLatin1(kScreenSaverService), QStringLiteral("ActiveChanged"), this,
                  SLOT(lockChanged(bool)));
    loadConfiguration();
    if (QFile::exists(m_configPath)) {
        m_watcher.addPath(m_configPath);
    } else {
        m_watcher.addPath(QFileInfo(m_configPath).absolutePath());
    }
    armIdleTimeout();
}

void ScreensaverLauncher::loadConfiguration()
{
    m_enabled = true;
    m_program = QStringLiteral("qinda-patrol");
    m_arguments.clear();
    m_timeoutMinutes = 5;
    QFile file(m_configPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        m_enabled = object.value(QStringLiteral("enabled")).toBool(true);
        const QString saver = object.value(QStringLiteral("saver")).toString();
        if (!saver.isEmpty()) {
            m_program = saver;
        }
        m_timeoutMinutes = qBound(1, object.value(QStringLiteral("timeoutMinutes")).toInt(5), 240);
        for (const auto &value : object.value(QStringLiteral("arguments")).toArray()) {
            m_arguments.append(value.toString());
        }
    }
    if (m_arguments.isEmpty()) {
        m_arguments = defaultArguments(m_program);
    }
    if (m_program == QLatin1String("none")) {
        m_enabled = false;
    }
}

void ScreensaverLauncher::armIdleTimeout()
{
    disarmIdleTimeout();
    if (!m_enabled) {
        stop();
        return;
    }
    m_idleToken = KIdleTime::instance()->addIdleTimeout(m_timeoutMinutes * 60 * 1000);
}

void ScreensaverLauncher::disarmIdleTimeout()
{
    if (m_idleToken >= 0) {
        KIdleTime::instance()->removeIdleTimeout(m_idleToken);
        m_idleToken = -1;
    }
}

bool ScreensaverLauncher::sessionLocked() const
{
    QDBusInterface locker(QString::fromLatin1(kScreenSaverService),
                          QString::fromLatin1(kScreenSaverPath),
                          QString::fromLatin1(kScreenSaverService), m_bus);
    const QDBusReply<bool> reply = locker.call(QStringLiteral("GetActive"));
    return reply.isValid() && reply.value();
}

void ScreensaverLauncher::launch()
{
    if (m_process.state() != QProcess::NotRunning) {
        return;
    }
    m_killTimer.stop();
    m_process.start(m_program, m_arguments);
}

void ScreensaverLauncher::stop()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    m_process.terminate();
    m_killTimer.start();
}

} // namespace QindaQt::Session::DesktopControls

// The Q_SLOT the D-Bus signal connects to: KScreenLocker's ActiveChanged.
void QindaQt::Session::DesktopControls::ScreensaverLauncher::lockChanged(bool active)
{
    m_locked = active;
    if (active) {
        m_relaunchTimer.stop();
        stop();
    }
}
