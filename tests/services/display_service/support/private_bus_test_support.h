// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileDevice>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>

namespace QindaQt::DisplayService::TestSupport
{

class PrivateSessionBus final
{
public:
    PrivateSessionBus()
        : m_root(QDir::temp().filePath(
              QStringLiteral("qindaqt-display-private-bus-XXXXXX")))
    {
    }

    ~PrivateSessionBus() { stop(); }

    bool start(QString *error)
    {
        if (!m_root.isValid()) {
            *error = QStringLiteral("could not create disposable private root");
            return false;
        }
        const QString runtime = m_root.filePath(QStringLiteral("runtime"));
        const QString home = m_root.filePath(QStringLiteral("home"));
        const QString config = m_root.filePath(QStringLiteral("config"));
        const QString data = m_root.filePath(QStringLiteral("data"));
        const QString cache = m_root.filePath(QStringLiteral("cache"));
        const QString state = m_root.filePath(QStringLiteral("state"));
        for (const QString &directory : {runtime, home, config, data, cache, state}) {
            if (!QDir().mkpath(directory)) {
                *error = QStringLiteral("could not create disposable bus roots");
                return false;
            }
        }
        if (!QFile::setPermissions(runtime, QFileDevice::ReadOwner
                                               | QFileDevice::WriteOwner
                                               | QFileDevice::ExeOwner)) {
            *error = QStringLiteral("could not secure disposable runtime root");
            return false;
        }

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        for (const QString &name : {QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                                    QStringLiteral("DISPLAY"),
                                    QStringLiteral("WAYLAND_DISPLAY"),
                                    QStringLiteral("XAUTHORITY")}) {
            environment.remove(name);
        }
        environment.insert(QStringLiteral("HOME"), home);
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        environment.insert(QStringLiteral("XDG_DATA_HOME"), data);
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), cache);
        environment.insert(QStringLiteral("XDG_STATE_HOME"), state);

        m_process.setProcessEnvironment(environment);
        m_process.setWorkingDirectory(m_root.path());
        m_process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--address=unix:path=%1")
                             .arg(m_root.filePath(QStringLiteral("bus"))),
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
            *error = QStringLiteral("private dbus-daemon returned no address");
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
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            m_process.waitForFinished(1'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QTemporaryDir m_root;
    QProcess m_process;
    QString m_address;
};

inline QString privateConnectionName(const QString &role)
{
    return QStringLiteral("qindaqt-display-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

inline QByteArray compositorPayload(const quint64 generation,
                                    const QString &model =
                                        QStringLiteral("Reference Display"))
{
    const QJsonObject output{
        {QStringLiteral("name"), QStringLiteral("DP-1")},
        {QStringLiteral("geometry"),
         QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0},
                     {QStringLiteral("width"), 1920},
                     {QStringLiteral("height"), 1080}}},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("refreshRateMilliHz"), 60'000},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("internal"), false},
        {QStringLiteral("uuid"), QStringLiteral("runtime-uuid")},
        {QStringLiteral("priority"), 0},
        {QStringLiteral("physicalSizeMm"),
         QJsonObject{{QStringLiteral("width"), 600},
                     {QStringLiteral("height"), 340}}},
        {QStringLiteral("manufacturer"), QStringLiteral("Qinda")},
        {QStringLiteral("model"), model}};
    return QJsonDocument(
               QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                           {QStringLiteral("schemaVersion"), 1},
                           {QStringLiteral("outputGeneration"),
                            QString::number(generation)},
                           {QStringLiteral("outputs"), QJsonArray{output}}})
        .toJson(QJsonDocument::Compact);
}

} // namespace QindaQt::DisplayService::TestSupport
