// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/adapters/backlight_writer.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: writes the internal panel's raw brightness through the
// user's own seat session on the system bus
// (org.freedesktop.login1.Session.SetBrightness, subsystem `backlight`).
// ADR-0186 records why: on real laptop hardware the kernel's brightness
// attribute is owned by root and not writable by the session user, while
// logind performs the same write for the session that owns the seat with no
// polkit prompt. This adapter therefore replaces privilege escalation with
// delegation: it never chowns, never installs a udev rule, never uses a setuid
// helper, and never reads or writes a kernel attribute itself.
//
// AGENT-GUARD: the transport fence in check_boundary.cmake forbids this file
// from naming a kernel class path. Describe the attribute, never its location;
// the device *name* is all this adapter is given.
//
// AGENT-GUARD: `…/session/auto` resolves to the *caller's* session, which for
// an ssh shell or a non-seat service is a session with an empty seat; logind
// answers SetBrightness there with "no seat". Resolution therefore accepts
// `auto` only when it actually reports a seat, and otherwise picks this uid's
// seat session from Manager.ListSessions, preferring the active one.
//
// Threading: write() blocks on one bounded system-bus round trip. Callers
// serialize internal-brightness requests one at a time
// (ProductionBatteryCollaborator), so the resident service can stall for at
// most one timeout per request. Do not make this asynchronous without moving
// the whole BacklightWriter seam to a callback contract.
class LogindBacklightWriter final : public BacklightWriter
{
public:
    explicit LogindBacklightWriter(const QDBusConnection &systemBus);
    ~LogindBacklightWriter() override;

    [[nodiscard]] bool available() override;
    [[nodiscard]] QString unavailableDiagnostic() const override;
    [[nodiscard]] BacklightWriteOutcome write(const QString &deviceName,
                                              quint32 value) override;

    // Test seam: the uid whose seat session is selected from ListSessions.
    // Production leaves this at the real uid of the running process.
    void setSubjectUid(quint32 uid);
    // Observability for tests and diagnostics; empty until a session resolves.
    [[nodiscard]] QString resolvedSessionPath() const;

private:
    enum class CallResult : quint32 {
        Succeeded = 0,
        // The object or service went away: a re-resolve may recover.
        Stale = 1,
        // logind answered, and its answer was no.
        Refused = 2,
    };

    bool resolveSession();
    [[nodiscard]] bool sessionReportsSeat(const QString &sessionPath) const;
    [[nodiscard]] bool sessionIsActive(const QString &sessionPath) const;
    [[nodiscard]] QStringList seatSessionPathsForSubject() const;
    CallResult callSetBrightness(const QString &deviceName, quint32 value,
                                 QString &errorName, QString &errorText) const;

    QDBusConnection m_bus;
    QString m_sessionPath;
    QString m_diagnostic;
    QElapsedTimer m_lastFailedProbe;
    quint32 m_subjectUid;
    bool m_resolved = false;
};

} // namespace QindaQt::Power::Upstream
