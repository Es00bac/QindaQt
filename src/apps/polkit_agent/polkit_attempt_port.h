// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Apps::PolkitAgent {

// One live authentication attempt, seen from the pure dialog logic. The
// production implementation (polkit_listener.cpp) wraps a real, single-use
// PolkitQt1::Agent::Session; a fresh AuthenticationAttempt is created for
// every retry, exactly as a fresh Session is required upstream. Tests
// implement this directly with no polkit type in sight.
//
// AGENT-CONTRACT: signals stay in the default (public) access region Qt's
// Q_SIGNALS convention puts them in, so a test double may call them directly
// to drive PolkitAttemptController's state machine without a real PAM
// conversation (see tst_polkit_attempt_controller.cpp's FakeAttempt).
class AuthenticationAttempt : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~AuthenticationAttempt() override = default;

    // Answers whatever the most recent request() asked for (the password, or
    // a later multi-step PAM prompt such as an OTP). The response is passed
    // through only; an implementation must never retain a copy past this call.
    virtual void respond(const QString &response) = 0;
    // User- or polkitd-initiated cancellation of this one attempt.
    virtual void cancel() = 0;

Q_SIGNALS:
    // A PAM conversation prompt. `echo` true means the response may be shown
    // in plain text (polkit's own contract for Session::request).
    void request(const QString &text, bool echo);
    void showInfo(const QString &text);
    void showError(const QString &text);
    // Fires exactly once per attempt. `gainedAuthorization` false covers both
    // a failed attempt (wrong password) and a cancellation; the caller tells
    // them apart from its own cancel-requested state, not from this signal.
    void completed(bool gainedAuthorization);
};

} // namespace QindaQt::Apps::PolkitAgent
