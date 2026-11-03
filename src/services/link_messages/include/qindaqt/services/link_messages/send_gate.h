// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

namespace QindaQt::Link {

// Admission control for Messages.Send (ADR-0438). Implemented by link_service
// (rate limit plus a desktop notice); the adaptor stays free of policy.
//
// AGENT-CONTRACT: admit() is called once per Send, after the caller was
// authorized and before anything is sent to the phone. It returns an empty
// string when the send may proceed (the implementation has then recorded it
// and announced it to the user), or a protocol-style error code such as
// "rate-limited" when it must be refused.
class SendGate {
public:
    virtual ~SendGate() = default;
    [[nodiscard]] virtual QString admit(const QString &deviceId, int recipients, const QString &requestedBy) = 0;
};

} // namespace QindaQt::Link
