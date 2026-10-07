// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>

#include <csignal>

namespace QindaQt::Tests {

// AGENT-GUARD: Explicit, untyped bus configuration includes only our private
// activation descriptor. The helper connects by its pinned address, never by
// an ambient system/session bus, and cleanup signals only our exact executable.
class UpowerActivationBus final
{
public:
    bool start(const int delayMs, const bool fail = false)
    {
        if (!root.isValid()) return false;
        address = QStringLiteral("unix:abstract=qindaqt-upower-startup-%1")
                      .arg(QUuid::createUuid().toString(QUuid::Id128));
        const QString serviceDir = root.filePath(QStringLiteral("services"));
        if (!QDir().mkpath(serviceDir)) return false;
        const QString panel = root.filePath(QStringLiteral("backlight/panel"));
        if (!QDir().mkpath(panel)
            || !writeFile(panel + QStringLiteral("/type"), QByteArrayLiteral("firmware\n"))
            || !writeFile(panel + QStringLiteral("/max_brightness"), QByteArrayLiteral("255\n"))
            || !writeFile(panel + QStringLiteral("/brightness"), QByteArrayLiteral("100\n")))
            return false;
        QFile descriptor(serviceDir + QStringLiteral("/org.freedesktop.UPower.service"));
        if (!descriptor.open(QIODevice::WriteOnly)) return false;
        const QString command = execArgument(QCoreApplication::applicationFilePath())
            + QStringLiteral(" --activation-helper --delay-ms=%1 --fail=%2 ")
                  .arg(delayMs).arg(fail ? 1 : 0)
            + execArgument(QStringLiteral("--bus-address=") + address) + QLatin1Char(' ')
            + execArgument(QStringLiteral("--fixture-root=") + root.path());
        descriptor.write((QStringLiteral("[D-BUS Service]\nName=org.freedesktop.UPower\nExec=")
                          + command + QLatin1Char('\n')).toUtf8());
        descriptor.close();
        QFile config(root.filePath(QStringLiteral("bus.conf")));
        if (!config.open(QIODevice::WriteOnly)) return false;
        const QString contents = QStringLiteral(
            "<busconfig><listen>%1</listen><auth>EXTERNAL</auth>"
            "<servicedir>%2</servicedir><limit name=\"service_start_timeout\">10000</limit>"
            "<policy context=\"default\"><allow user=\"*\"/><allow own=\"*\"/>"
            "<allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
            "</policy></busconfig>").arg(address, serviceDir.toHtmlEscaped());
        config.write(contents.toUtf8());
        config.close();
        daemon.setProgram(QStringLiteral("dbus-daemon"));
        daemon.setArguments({QStringLiteral("--config-file=") + config.fileName(),
                             QStringLiteral("--nofork"), QStringLiteral("--nopidfile"),
                             QStringLiteral("--print-address=1")});
        daemon.start();
        if (!daemon.waitForStarted(5000) || !daemon.waitForReadyRead(5000)) return false;
        if (!QString::fromUtf8(daemon.readLine()).startsWith(address)) return false;
        name = QStringLiteral("upower-startup-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return connection.isConnected();
    }

    ~UpowerActivationBus()
    {
        QFile marker(root.filePath(QStringLiteral("helper-pid")));
        if (marker.open(QIODevice::ReadOnly)) {
            bool valid = false;
            const qint64 pid = marker.readAll().trimmed().toLongLong(&valid);
            const QString expected = QFileInfo(QCoreApplication::applicationFilePath())
                                         .canonicalFilePath();
            auto isHelper = [&] {
                return valid && pid > 0 && !expected.isEmpty()
                    && QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid))
                           .canonicalFilePath() == expected;
            };
            if (isHelper()) {
                ::kill(static_cast<pid_t>(pid), SIGTERM);
                for (int attempt = 0; attempt < 100 && isHelper(); ++attempt)
                    QThread::msleep(10);
                if (isHelper()) ::kill(static_cast<pid_t>(pid), SIGKILL);
            }
        }
        for (const QString &open : connections) QDBusConnection::disconnectFromBus(open);
        if (!name.isEmpty()) QDBusConnection::disconnectFromBus(name);
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) {
            daemon.kill();
            daemon.waitForFinished();
        }
    }

    QDBusConnection openConnection()
    {
        const QString openedName = name + QStringLiteral("-fake-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        connections.push_back(openedName);
        return QDBusConnection::connectToBus(address, openedName);
    }

    bool helperStarted() const
    {
        return QFileInfo::exists(root.filePath(QStringLiteral("helper-pid")));
    }

    bool helperFinished() const
    {
        return QFileInfo::exists(root.filePath(QStringLiteral("helper-done")));
    }

    QString owner() const
    {
        const QDBusReply<QString> reply = connection.interface()->serviceOwner(
            QStringLiteral("org.freedesktop.UPower"));
        return reply.isValid() ? reply.value() : QString();
    }

    QTemporaryDir root{QStringLiteral(QINDAQT_TEST_SCRATCH_DIR)
                       + QStringLiteral("/upower-startup-XXXXXX")};
    QString address;
    QString name;
    QProcess daemon;
    QDBusConnection connection{QStringLiteral("invalid-upower-startup")};
    QStringList connections;

private:
    static bool writeFile(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    static QString execArgument(QString argument)
    {
        argument.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        argument.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        return QLatin1Char('"') + argument + QLatin1Char('"');
    }
};

} // namespace QindaQt::Tests
