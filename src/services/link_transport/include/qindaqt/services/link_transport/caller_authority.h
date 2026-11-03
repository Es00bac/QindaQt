// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>

class QDBusMessage;

namespace QindaQt::Link {

// Decides whether the sender of a Link1.* D-Bus call may use a protected
// member (ADR-0438). Every adaptor takes one; there is no default, and a
// null authority means "deny" at the call sites.
//
// AGENT-CONTRACT: authorize() returns true only for a trusted caller. On
// false it has already queued the reply `org.qindaqt.Link1.Error.Denied`
// (delayed-reply style) so the adaptor just returns. `callerName`, when not
// null, receives the caller executable's basename for audit text.
class CallerAuthority {
public:
    virtual ~CallerAuthority() = default;
    [[nodiscard]] virtual bool authorize(const QDBusMessage &message, QString *callerName = nullptr) = 0;
};

} // namespace QindaQt::Link
