// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/caller_policy.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>

#include <sys/stat.h>
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

bool DBusProcessInspector::executableFileId(uint pid, quint64 *dev, quint64 *ino)
{
    struct stat st {};
    if (::stat(QFile::encodeName(QStringLiteral("/proc/%1/exe").arg(pid)).constData(), &st) != 0) {
        return false;
    }
    *dev = quint64(st.st_dev);
    *ino = quint64(st.st_ino);
    return true;
}

bool DBusProcessInspector::fileIdOfPath(const QString &path, quint64 *dev, quint64 *ino)
{
    struct stat st {};
    if (::stat(QFile::encodeName(path).constData(), &st) != 0) {
        return false;
    }
    *dev = quint64(st.st_dev);
    *ino = quint64(st.st_ino);
    return true;
}

static QString readNs(const QString &link)
{
    char buffer[256];
    const ssize_t n = ::readlink(QFile::encodeName(link).constData(), buffer, sizeof buffer - 1);
    return n > 0 ? QString::fromLatin1(buffer, int(n)) : QString();
}

QString DBusProcessInspector::mountNamespaceOf(uint pid) { return readNs(QStringLiteral("/proc/%1/ns/mnt").arg(pid)); }
QString DBusProcessInspector::ownMountNamespace() { return readNs(QStringLiteral("/proc/self/ns/mnt")); }

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
    // AGENT-GUARD: compare inode identity, not the path string. The caller must also live in the service's mount
    // namespace; in another one the same inode could still be a different view of the filesystem.
    const QString ownNs = m_inspector->ownMountNamespace();
    if (ownNs.isEmpty() || m_inspector->mountNamespaceOf(pid) != ownNs) {
        return false;
    }
    quint64 dev = 0, ino = 0;
    if (!m_inspector->executableFileId(pid, &dev, &ino)) {
        return false;
    }
    QString matched;
    for (const QString &trusted : std::as_const(m_trusted)) {  // looked up each time: survives package upgrades
        quint64 tdev = 0, tino = 0;
        if (m_inspector->fileIdOfPath(trusted, &tdev, &tino) && tdev == dev && tino == ino) {
            matched = trusted;
            break;
        }
    }
    if (matched.isEmpty()) {
        return false;
    }
    // Re-check that the process incarnation did not change while we looked.
    if (m_inspector->startTimeOf(pid) != before) {
        return false;
    }
    if (exeBasename != nullptr) {
        *exeBasename = QFileInfo(matched).fileName();
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
