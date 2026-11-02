// SPDX-License-Identifier: GPL-3.0-or-later
#include "adb_session.h"

#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <array>
#include <utility>

namespace qindaqt::link_mirror {
namespace {

constexpr int kQuickTimeoutMs = 5000;
constexpr int kPushTimeoutMs = 20000;
constexpr int kStopGraceMs = 3000;
constexpr qsizetype kRecentLines = 40;

} // namespace

AdbSession::AdbSession(QString adbPath, QString serial, QObject *parent)
    : QObject(parent), m_adb(std::move(adbPath)), m_serial(std::move(serial))
{
    m_server.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_server, &QProcess::readyReadStandardOutput, this, &AdbSession::readServerOutput);
    connect(&m_server, &QProcess::finished, this, [this](int exitCode) {
        readServerOutput();
        Q_EMIT serverExited(exitCode);
    });
}

AdbSession::~AdbSession()
{
    stop();
}

QString AdbSession::defaultAdbPath()
{
    const QString portage = QStringLiteral("/usr/bin/adb");
    if (QFileInfo(portage).isExecutable()) {
        return portage;
    }
    return QStandardPaths::findExecutable(QStringLiteral("adb"));
}

QString AdbSession::newToken()
{
    std::array<quint32, 4> words{};
    QRandomGenerator::system()->fillRange(words.data(), words.size());
    QByteArray bytes(reinterpret_cast<const char *>(words.data()), sizeof words);
    return QString::fromLatin1(bytes.toHex());
}

AdbSession::Result AdbSession::run(const QStringList &arguments, int timeoutMs) const
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(m_adb, QStringList{QStringLiteral("-s"), m_serial} + arguments);
    Result result;
    if (!process.waitForStarted(kQuickTimeoutMs)) {
        result.error = QStringLiteral("cannot run %1: %2").arg(m_adb, process.errorString());
        return result;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(kQuickTimeoutMs);
        result.error = QStringLiteral("adb %1 timed out").arg(arguments.value(0));
        return result;
    }
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    result.out = process.readAll();
    if (result.exitCode != 0) {
        result.error = QStringLiteral("adb %1 failed: %2").arg(arguments.value(0), QString::fromUtf8(result.out).trimmed());
    }
    return result;
}

std::optional<QString> AdbSession::resolveClassPath(const QString &serverJar, QString *error)
{
    if (!serverJar.isEmpty()) {
        if (!QFileInfo(serverJar).isReadable()) {
            *error = QStringLiteral("server jar %1 is not readable").arg(serverJar);
            return std::nullopt;
        }
        const Result push = run({QStringLiteral("push"), serverJar, QString::fromLatin1(kDevJarDevicePath)}, kPushTimeoutMs);
        if (!push.error.isEmpty()) {
            *error = push.error;
            return std::nullopt;
        }
        return QString::fromLatin1(kDevJarDevicePath);
    }
    const Result path = run({QStringLiteral("shell"), QStringLiteral("pm"), QStringLiteral("path"),
                             QString::fromLatin1(kLinkPackage)},
                            kQuickTimeoutMs);
    for (const QByteArray &line : path.out.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("package:") && trimmed.endsWith("/base.apk")) {
            return QString::fromUtf8(trimmed.mid(8));
        }
    }
    *error = QStringLiteral("no mirror server: pass --server-jar (build it with "
                            "phone/mirror_server/build-dev-jar.sh) or install the QindaLink app");
    return std::nullopt;
}

bool AdbSession::startServer(const QString &classPath, const QString &token, bool verbose, QString *error)
{
    // AGENT-GUARD: the token reaches the phone only as an argument of our own app_process
    // command; never write it to a log line or a file.
    QStringList arguments{QStringLiteral("-s"), m_serial, QStringLiteral("shell"),
                          QStringLiteral("CLASSPATH=") + classPath, QStringLiteral("app_process"),
                          QStringLiteral("/"), QStringLiteral("org.qindaqt.link.mirror.Server"),
                          QStringLiteral("version=%1").arg(kProtocolVersion), QStringLiteral("token=") + token};
    if (verbose) {
        arguments.append(QStringLiteral("verbose=1"));
    }
    m_server.start(m_adb, arguments);
    if (!m_server.waitForStarted(kQuickTimeoutMs)) {
        *error = QStringLiteral("cannot start %1: %2").arg(m_adb, m_server.errorString());
        return false;
    }
    return true;
}

std::optional<quint16> AdbSession::forward(const QString &token, QString *error)
{
    const Result result = run({QStringLiteral("forward"), QStringLiteral("tcp:0"),
                               QStringLiteral("localabstract:") + QLatin1String(kSocketPrefix) + token},
                              kQuickTimeoutMs);
    if (!result.error.isEmpty()) {
        *error = result.error;
        return std::nullopt;
    }
    bool ok = false;
    const uint port = result.out.trimmed().toUInt(&ok);
    if (!ok || port == 0 || port > 65535) {
        *error = QStringLiteral("adb forward printed no port");
        return std::nullopt;
    }
    m_forwardPort = static_cast<quint16>(port);
    return m_forwardPort;
}

void AdbSession::stop()
{
    if (m_forwardPort) {
        const Result removed = run({QStringLiteral("forward"), QStringLiteral("--remove"),
                                    QStringLiteral("tcp:%1").arg(*m_forwardPort)},
                                   kQuickTimeoutMs);
        if (!removed.error.isEmpty()) {
            qWarning("qindaqt-link-mirror: %s", qPrintable(removed.error));
        }
        m_forwardPort.reset();
    }
    if (m_server.state() != QProcess::NotRunning) {
        // The server exits by itself once its socket closes; give it that chance first so
        // it can release the virtual display cleanly.
        if (!m_server.waitForFinished(kStopGraceMs)) {
            m_server.terminate();
            if (!m_server.waitForFinished(kQuickTimeoutMs)) {
                m_server.kill();
                m_server.waitForFinished(kQuickTimeoutMs);
            }
        }
    }
}

void AdbSession::readServerOutput()
{
    m_partialLine.append(m_server.readAllStandardOutput());
    qsizetype newline = 0;
    while ((newline = m_partialLine.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_partialLine.left(newline)).trimmed();
        m_partialLine.remove(0, newline + 1);
        if (line.isEmpty()) {
            continue;
        }
        m_recent.append(line);
        if (m_recent.size() > kRecentLines) {
            m_recent.removeFirst();
        }
        Q_EMIT serverLine(line);
    }
}

} // namespace qindaqt::link_mirror
