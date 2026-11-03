// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/caller_policy.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>

#include <unistd.h>

namespace QindaQt::Link {

bool DBusProcessInspector::pidOf(const QString &busName, uint *pid)
{
    if (!m_bus.isConnected() || m_bus.interface() == nullptr) {
        return false;
    }
    const QDBusReply<uint> reply = m_bus.interface()->servicePid(busName);
    if (!reply.isValid() || reply.value() == 0) {
        return false;
    }
    *pid = reply.value();
    return true;
}

QString DBusProcessInspector::executableOf(uint pid)
{
    char buffer[4096];
    const QByteArray link = QFile::encodeName(QStringLiteral("/proc/%1/exe").arg(pid));
    const ssize_t n = ::readlink(link.constData(), buffer, sizeof buffer - 1);
    return n > 0 ? QFile::decodeName(QByteArray(buffer, int(n))) : QString();
}

qulonglong DBusProcessInspector::startTimeOf(uint pid)
{
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray stat = f.readAll();
    // The command name may contain spaces and parentheses: parse after the last ')'.
    const qsizetype close = stat.lastIndexOf(')');
    if (close < 0) {
        return 0;
    }
    const QList<QByteArray> fields = stat.mid(close + 2).split(' ');
    return fields.size() > 19 ? fields.at(19).toULongLong() : 0;  // overall field 22
}

CallerPolicy::CallerPolicy(QDBusConnection bus, std::unique_ptr<ProcessInspector> inspector,
                           QStringList trustedExecutables)
    : m_bus(std::move(bus)), m_inspector(std::move(inspector))
{
    for (const QString &path : trustedExecutables) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        m_trusted.append(canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical);
    }
}

bool CallerPolicy::isTrusted(const QString &busName, QString *exeBasename)
{
    uint pid = 0;
    if (busName.isEmpty() || !m_inspector->pidOf(busName, &pid)) {
        return false;
    }
    // AGENT-GUARD: bind the executable to one process incarnation. The start
    // time is read before and after the exe link; a PID reused in between
    // changes it (or fails), and the check fails closed.
    const qulonglong before = m_inspector->startTimeOf(pid);
    const QString exe = m_inspector->executableOf(pid);
    const qulonglong after = m_inspector->startTimeOf(pid);
    if (before == 0 || before != after || exe.isEmpty() || exe.endsWith(QLatin1String(" (deleted)"))) {
        return false;
    }
    if (!m_trusted.contains(exe)) {
        return false;
    }
    if (exeBasename != nullptr) {
        *exeBasename = QFileInfo(exe).fileName();
    }
    return true;
}

bool CallerPolicy::authorize(const QDBusMessage &message, QString *callerName)
{
    if (isTrusted(message.service(), callerName)) {
        return true;
    }
    // AGENT-NOTE: logs the member only; arguments can be message content.
    qWarning("Link1: denied %s from an untrusted caller", qPrintable(message.member()));
    message.setDelayedReply(true);
    m_bus.send(message.createErrorReply(QStringLiteral("org.qindaqt.Link1.Error.Denied"),
                                        QStringLiteral("caller is not a trusted QindaLink client")));
    return false;
}

} // namespace QindaQt::Link
