// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agent_identity.h"
#include "polkit_attempt_port.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace QindaQt::Apps::PolkitAgent {

// Creates one fresh attempt for the identity token the controller selected.
// Called once at start() and again for every automatic retry.
using AttemptFactory =
    std::function<std::unique_ptr<AuthenticationAttempt>(const QString &identityToken)>;

// The retry/cancel state machine for one polkit initiateAuthentication
// request (ADR-0290, plan slice PF15). No PolkitQt1 type appears in this
// header; `identityToken` is opaque (see AgentIdentity::token).
//
// AGENT-CONTRACT: a failed attempt that was not cancelled starts a brand new
// AuthenticationAttempt automatically (the dialog stays open and re-prompts,
// mirroring polkit-kde-agent/polkit-gnome). This class never enforces a
// retry ceiling; polkitd owns that and stops the dialog by calling
// Listener::cancelAuthentication(), which the caller must route to
// cancelExternally().
class PolkitAttemptController final : public QObject {
    Q_OBJECT

public:
    PolkitAttemptController(QList<AgentIdentity> identities, int preferredIdentityIndex,
                            AttemptFactory factory, QObject *parent = nullptr);

    // Locks in which identity to authenticate as. Only effective before
    // start(); AGENT-GUARD: changing it afterward would silently retry as a
    // different identity than the one the user saw prompted for.
    void setSelectedIdentityIndex(int index);
    [[nodiscard]] int selectedIdentityIndex() const noexcept { return m_selectedIndex; }
    [[nodiscard]] const QList<AgentIdentity> &identities() const noexcept { return m_identities; }

    // Creates the first attempt. Calling twice is a no-op.
    void start();
    // Forwards to the live attempt's respond(); a no-op before start() or
    // after completion.
    void authenticate(const QString &response);
    // User-initiated cancel (the dialog's Cancel button or Escape).
    void cancel();
    // polkitd-initiated cancel (Listener::cancelAuthentication()).
    void cancelExternally();
    [[nodiscard]] bool finished() const noexcept { return m_finished; }

Q_SIGNALS:
    void promptRequested(const QString &text, bool echo);
    void infoMessage(const QString &text);
    // Raw PAM/polkit error text (Session::showError), unrelated to retry.
    void errorMessage(const QString &text);
    // A wrong-password (or otherwise failed, non-cancelled) attempt; the
    // dialog should show this message and clear/refocus the response field.
    // A new attempt has already been started by the time this fires.
    void attemptFailed(const QString &message);
    // Fires exactly once, when the request is fully resolved (success, user
    // cancel, or external cancel).
    void completed(bool gainedAuthorization);

private:
    void beginAttempt();
    void handleCompleted(bool gainedAuthorization);

    QList<AgentIdentity> m_identities;
    int m_selectedIndex;
    AttemptFactory m_factory;
    std::unique_ptr<AuthenticationAttempt> m_attempt;
    bool m_started = false;
    bool m_finished = false;
    bool m_cancelRequested = false;
};

} // namespace QindaQt::Apps::PolkitAgent
