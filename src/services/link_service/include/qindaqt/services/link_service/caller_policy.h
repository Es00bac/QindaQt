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
};

class DBusProcessInspector final : public ProcessInspector {
public:
    explicit DBusProcessInspector(QDBusConnection bus) : m_bus(std::move(bus)) {}
    bool pidOf(const QString &busName, uint *pid) override;
    QString executableOf(uint pid) override;
    qulonglong startTimeOf(uint pid) override;

private:
    QDBusConnection m_bus;
};

// The single caller policy for every Link1.* adaptor (ADR-0438).
//
// A caller is trusted when the executable behind its bus connection is in the
// trusted set. The set comes only from the service's command line.
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
    QStringList m_trusted;  // canonical absolute paths
};

} // namespace QindaQt::Link
