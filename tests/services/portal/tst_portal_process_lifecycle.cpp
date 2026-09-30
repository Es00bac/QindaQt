// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/resident_portal_service.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <csignal>
#include <optional>
#include <map>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>

using namespace QindaQt::Services::Portal;

namespace {

constexpr auto SettingsServiceName = "org.qindaqt.Settings1";

struct ActivationObservation final {
    QString portalOwner;
    QString settingsOwner;
    qint64 portalProcessId = 0;
    qint64 settingsProcessId = 0;
};

class PrivateBusDaemon final {
public:
    ~PrivateBusDaemon() { stop(); }

    bool start(const QProcessEnvironment &environment, QString *error)
    {
        m_process.setProcessEnvironment(environment);
        m_process.start(QStringLiteral(QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000)
            || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString() + QStringLiteral(": ")
                     + QString::fromUtf8(m_process.readAllStandardError());
            stop();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private daemon returned no address: ")
                     + QString::fromUtf8(m_process.readAllStandardError());
            stop();
            return false;
        }
        return true;
    }

    void stop() noexcept
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(5'000)) {
            m_process.kill();
            m_process.waitForFinished(5'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class ActivatedProcessCleanup final {
public:
    ~ActivatedProcessCleanup()
    {
        for (const auto &[processId, fd] : m_processes) {
            if (isAlive(processId)) {
                // PIDFD targets only the retained fixture process, never a
                // recycled PID. Production dumpability must stay disabled.
                ::syscall(SYS_pidfd_send_signal, fd, SIGTERM, nullptr, 0);
                for (int attempt=0; attempt<50 && isAlive(processId); ++attempt) QTest::qSleep(20);
                if (isAlive(processId)) ::syscall(SYS_pidfd_send_signal, fd, SIGKILL, nullptr, 0);
            }
            ::close(fd);
        }
    }
    bool track(qint64 processId)
    {
        // AGENT-CONTRACT: called only for unique owners just observed on this
        // test's private daemon, activated from its exact staged descriptors.
        // /proc/exe is intentionally unreadable for the secret-bearing backend.
        const int fd=static_cast<int>(::syscall(SYS_pidfd_open, processId, 0));
        if(fd<0) return false;
        if(!m_processes.emplace(processId,fd).second) {::close(fd);return false;}
        return isAlive(processId);
    }
    bool isAlive(qint64 processId) const
    {
        const auto found=m_processes.find(processId);if(found==m_processes.end()) return false;
        pollfd descriptor{found->second,POLLIN,0};
        return ::poll(&descriptor,1,0)==0;
    }
private:
    std::map<qint64,int> m_processes;
};

bool writeDescriptor(const QString &directory, const QByteArray &name,
                     const QString &executable, QString *error)
{
    if (!QDir().mkpath(directory)) {
        *error = QStringLiteral("cannot create activation service directory");
        return false;
    }
    QFile file(QDir(directory).filePath(QString::fromLatin1(name) +
                                        QStringLiteral(".service")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = file.errorString();
        return false;
    }
    const QByteArray contents = QByteArrayLiteral("[D-BUS Service]\nName=")
                                + name + QByteArrayLiteral("\nExec=")
                                + executable.toUtf8() + QByteArrayLiteral("\n");
    if (file.write(contents) != contents.size()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool writeActivationDescriptors(const QString &dataRoot,
                                const QString &portalExecutable,
                                const QString &settingsExecutable,
                                QString *error)
{
    const QString serviceDirectory =
        QDir(dataRoot).filePath(QStringLiteral("dbus-1/services"));
    return writeDescriptor(serviceDirectory, kPortalServiceName,
                           portalExecutable, error)
           && writeDescriptor(serviceDirectory, SettingsServiceName,
                              settingsExecutable, error);
}

std::optional<ActivationObservation> activateAndObserve(
    const QString &address, const QString &connectionName, QString *error)
{
    const QDBusConnection connection =
        QDBusConnection::connectToBus(address, connectionName);
    if (!connection.isConnected()) {
        *error = connection.lastError().message();
        return std::nullopt;
    }

    QElapsedTimer timeout;
    timeout.start();
    bool ready = false;
    while (timeout.elapsed() < 8'000) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QString::fromLatin1(kPortalServiceName),
            QString::fromLatin1(kPortalObjectPath),
            QString::fromLatin1(kPortalSettingsInterface),
            QStringLiteral("Read"));
        message << QString::fromLatin1(kAppearanceNamespace)
                << QString::fromLatin1(kColorSchemeKey);
        const QDBusReply<QDBusVariant> reply(
            connection.call(message, QDBus::Block, 2'000));
        if (reply.isValid() && reply.value().variant().toUInt() == quint32(0)) {
            ready = true;
            break;
        }
        QTest::qWait(50);
    }
    if (!ready) {
        *error = QStringLiteral("activated portal never published Settings1 truth");
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }

    const auto observe = [&](const QString &service,
                             QString *owner, qint64 *processId) {
        const QDBusReply<QString> ownerReply =
            connection.interface()->serviceOwner(service);
        const QDBusReply<uint> processReply =
            connection.interface()->servicePid(service);
        if (!ownerReply.isValid() || ownerReply.value().isEmpty()
            || !processReply.isValid() || processReply.value() == 0) {
            return false;
        }
        *owner = ownerReply.value();
        *processId = static_cast<qint64>(processReply.value());
        return true;
    };

    ActivationObservation observation;
    if (!observe(QString::fromLatin1(kPortalServiceName),
                 &observation.portalOwner, &observation.portalProcessId)
        || !observe(QString::fromLatin1(SettingsServiceName),
                    &observation.settingsOwner,
                    &observation.settingsProcessId)) {
        *error = QStringLiteral("could not observe both activated process owners");
        QDBusConnection::disconnectFromBus(connectionName);
        return std::nullopt;
    }
    QDBusConnection::disconnectFromBus(connectionName);
    return observation;
}

} // namespace

class PortalProcessLifecycleTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void activatesBothProcessesAndExitsOnPrivateDaemonLoss();
};

void PortalProcessLifecycleTests::activatesBothProcessesAndExitsOnPrivateDaemonLoss()
{
    QTemporaryDir directory;
    QVERIFY2(directory.isValid(), "could not create isolated process-test directory");
    const QString dataRoot =
        QDir(directory.path()).filePath(QStringLiteral("data"));
    const QString configRoot =
        QDir(directory.path()).filePath(QStringLiteral("config"));
    const QString portalExecutable = qEnvironmentVariable(
        "QINDAQT_TEST_PORTAL_EXECUTABLE",
        QStringLiteral(QINDAQT_PORTAL_SERVICE_EXECUTABLE));
    const QString settingsExecutable = qEnvironmentVariable(
        "QINDAQT_TEST_SETTINGS_EXECUTABLE",
        QStringLiteral(QINDAQT_SETTINGS_SERVICE_EXECUTABLE));
    const QString schemaDirectory = qEnvironmentVariable(
        "QINDAQT_TEST_SETTINGS_SCHEMA_DIR",
        QStringLiteral(QINDAQT_PORTAL_SOURCE_DIR "/data/settings"));
    const QString themeDirectory = qEnvironmentVariable(
        "QINDAQT_TEST_PORTAL_THEME_DIR",
        QStringLiteral(QINDAQT_PORTAL_SOURCE_DIR "/data/themes"));
    QString error;
    QVERIFY2(writeActivationDescriptors(dataRoot, portalExecutable,
                                        settingsExecutable, &error),
             qPrintable(error));
    QVERIFY(QFileInfo::exists(portalExecutable));
    QVERIFY(QFileInfo::exists(settingsExecutable));
    QVERIFY(QFileInfo::exists(
        QDir(schemaDirectory).filePath(QStringLiteral("schema-v2.json"))));
    QVERIFY(QFileInfo::exists(
        QDir(themeDirectory).filePath(QStringLiteral("qinda-dark.json"))));
    QVERIFY(QDir().mkpath(configRoot));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_DATA_DIRS"), dataRoot);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), dataRoot);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), directory.path()+QStringLiteral("/cache"));
    environment.remove(QStringLiteral("DISPLAY"));
    environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configRoot);
    environment.insert(QStringLiteral("QINDAQT_SETTINGS_SCHEMA_DIR"),
                       schemaDirectory);
    environment.insert(QStringLiteral("QINDAQT_PORTAL_THEME_DIRS"),
                       themeDirectory);
    environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
    environment.remove(QStringLiteral("DBUS_STARTER_ADDRESS"));
    environment.remove(QStringLiteral("DBUS_STARTER_BUS_TYPE"));

    ActivatedProcessCleanup cleanup;
    PrivateBusDaemon firstDaemon;
    QVERIFY2(firstDaemon.start(environment, &error), qPrintable(error));
    const auto first = activateAndObserve(
        firstDaemon.address(), QStringLiteral("portal-process-first"), &error);
    QVERIFY2(first.has_value(), qPrintable(error));
    QVERIFY(cleanup.track(first->portalProcessId));
    QVERIFY(cleanup.track(first->settingsProcessId));
    QVERIFY(cleanup.isAlive(first->portalProcessId));
    QVERIFY(cleanup.isAlive(first->settingsProcessId));
    QVERIFY(!QFileInfo::exists(
        QDir(configRoot).filePath(QStringLiteral("qindaqt/settings-v2.json"))));

    firstDaemon.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.isAlive(first->portalProcessId),
                             5'000);
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.isAlive(first->settingsProcessId),
                             5'000);

    PrivateBusDaemon secondDaemon;
    QVERIFY2(secondDaemon.start(environment, &error), qPrintable(error));
    const QString placeholderOneName = QStringLiteral("portal-placeholder-one");
    const QString placeholderTwoName = QStringLiteral("portal-placeholder-two");
    const auto placeholderOne = QDBusConnection::connectToBus(
        secondDaemon.address(), placeholderOneName);
    const auto placeholderTwo = QDBusConnection::connectToBus(
        secondDaemon.address(), placeholderTwoName);
    QVERIFY(placeholderOne.isConnected());
    QVERIFY(placeholderTwo.isConnected());
    const auto second = activateAndObserve(
        secondDaemon.address(), QStringLiteral("portal-process-second"), &error);
    QVERIFY2(second.has_value(), qPrintable(error));
    QVERIFY(cleanup.track(second->portalProcessId));
    QVERIFY(cleanup.track(second->settingsProcessId));
    QVERIFY(second->portalProcessId != first->portalProcessId);
    QVERIFY(second->settingsProcessId != first->settingsProcessId);
    QVERIFY(second->portalOwner != first->portalOwner);
    QVERIFY(second->settingsOwner != first->settingsOwner);
    QVERIFY(!QFileInfo::exists(
        QDir(configRoot).filePath(QStringLiteral("qindaqt/settings-v2.json"))));

    secondDaemon.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.isAlive(second->portalProcessId),
                             5'000);
    QTRY_VERIFY_WITH_TIMEOUT(!cleanup.isAlive(second->settingsProcessId),
                             5'000);
    QDBusConnection::disconnectFromBus(placeholderOneName);
    QDBusConnection::disconnectFromBus(placeholderTwoName);
}

QTEST_GUILESS_MAIN(PortalProcessLifecycleTests)
#include "tst_portal_process_lifecycle.moc"
