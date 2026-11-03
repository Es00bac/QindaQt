// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/caller_authority.h>

#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Link {

// Facts about a bus peer. The production implementation asks the bus daemon
// for the peer PID and reads /proc; tests inject a fake.
class ProcessInspector {
public:
    virtual ~ProcessInspector() = default;
    [[nodiscard]] virtual bool pidOf(const QString &busName, uint *pid) = 0;
    // Target of /proc/<pid>/exe exactly as the kernel reports it (may end in " (deleted)"); empty on failure.
    [[nodiscard]] virtual QString executableOf(uint pid) = 0;
    // Field 22 of /proc/<pid>/stat; 0 on failure.
    [[nodiscard]] virtual qulonglong startTimeOf(uint pid) = 0;
    // Inode identity of the file behind /proc/<pid>/exe (stat follows the magic link to the real inode, so a
    // bind mount in the caller's own mount namespace cannot fake it). False on failure.
    [[nodiscard]] virtual bool executableFileId(uint pid, quint64 *dev, quint64 *ino) = 0;
    // Inode identity of a path in the service's own namespace. False when it does not exist.
    [[nodiscard]] virtual bool fileIdOfPath(const QString &path, quint64 *dev, quint64 *ino) = 0;
    // Mount namespace names ("mnt:[4026531841]"); empty on failure.
    [[nodiscard]] virtual QString mountNamespaceOf(uint pid) = 0;
    [[nodiscard]] virtual QString ownMountNamespace() = 0;
};

class DBusProcessInspector final : public ProcessInspector {
public:
    explicit DBusProcessInspector(QDBusConnection bus) : m_bus(std::move(bus)) {}
    bool pidOf(const QString &busName, uint *pid) override;
    QString executableOf(uint pid) override;
    qulonglong startTimeOf(uint pid) override;
    bool executableFileId(uint pid, quint64 *dev, quint64 *ino) override;
    bool fileIdOfPath(const QString &path, quint64 *dev, quint64 *ino) override;
    QString mountNamespaceOf(uint pid) override;
    QString ownMountNamespace() override;

private:
    QDBusConnection m_bus;
};

// The single caller policy for every Link1.* adaptor (ADR-0438).
//
// A caller is trusted when the file behind its bus connection's executable has
// the same device and inode as a trusted file, looked up in the service's own
// mount namespace, and the caller shares the service's mount namespace. The
// set comes only from the service's command line. Identities are compared,
// never path strings: a process in a private mount namespace can bind-mount
// any file over a trusted path and make /proc/<pid>/exe read as that path.
//
// AGENT-NOTE: this raises the bar for other same-uid programs, it is not a
// boundary against same-uid code (see ADR-0438, "What this does not stop").
// AGENT-GUARD: never extend the trusted set from a D-Bus method, a file or an
// environment variable the caller can influence; that would let the very
// clients this policy exists to stop promote themselves.
class CallerPolicy final : public CallerAuthority {
public:
    CallerPolicy(QDBusConnection bus, std::unique_ptr<ProcessInspector> inspector, QStringList trustedExecutables);

    // Pure decision used by authorize(); `exeBasename` receives the caller's file name when trusted.
    [[nodiscard]] bool isTrusted(const QString &busName, QString *exeBasename = nullptr);
    [[nodiscard]] bool authorize(const QDBusMessage &message, QString *callerName = nullptr) override;

private:
    QDBusConnection m_bus;
    std::unique_ptr<ProcessInspector> m_inspector;
    QStringList m_trusted;  // canonical absolute paths; their inode identity is looked up on every check
};

} // namespace QindaQt::Link
