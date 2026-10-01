// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/session_binding.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <QProcess>
namespace QindaQt::Services::Portal {
// Owns at most one separate consent helper. The selected public binding and
// bus handle must outlive this same-thread object. Its executable is a trusted
// composition input, never a portal option/environment override. Every helper
// consumes a newly owned ordinary display FD; no pathname reconnect fallback.
// Lock uncertainty/revocation retires before terminating the child. Helper
// output is bounded, validated and rechecked against current native Unlocked.
class ProcessAccessConsent final : public AccessConsent {
    Q_OBJECT
public:
    ProcessAccessConsent(PortalSessionBinding &, QDBusConnection,
                         QString executable, QObject *parent = nullptr);
    ~ProcessAccessConsent() override;
    bool admitted() const override;
    void ask(RequestToken, const AccessQuestion &) override;
    void cancel(RequestToken) override;
private:
    void complete(int exitCode, QProcess::ExitStatus);
    PortalSessionBinding &m_binding;
    QString m_executable;
    QindaQt::Services::SessionLockState::QtNativeLockTransport m_transport;
    QindaQt::Services::SessionLockState::NativeLockStateMonitor m_monitor;
    QProcess m_process;
    RequestToken m_token = 0;
    AccessQuestion m_question;
    QByteArray m_output;
    int m_fd = -1;
};
} // namespace QindaQt::Services::Portal
