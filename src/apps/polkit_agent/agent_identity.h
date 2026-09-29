// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace QindaQt::Apps::PolkitAgent {

// One identity the user may authenticate as. Pure value type: no polkit type
// crosses this boundary, so the selection and formatting logic below is
// unit-testable without polkitd, D-Bus, or the polkit-qt library.
struct AgentIdentity final {
    // "Full Name (login)" for an ordinary user, "Administrator (root)" for
    // uid 0; see formatUserIdentityLabel().
    QString displayLabel;
    // Opaque to this layer. The polkit-facing adapter (polkit_listener.cpp)
    // round-trips this through PolkitQt1::Identity::toString()/fromString()
    // to start a new Session for the same identity on retry; nothing here
    // interprets it.
    QString token;
    bool isCurrentUser = false;

    friend bool operator==(const AgentIdentity &, const AgentIdentity &) = default;
};

// "Full Name (login)" when a full name is known, the bare login when it is
// not, and "User <uid>" when passwd carried neither (an honest fallback, not
// a silent blank). uid 0 is always "Administrator (root)" regardless of what
// passwd's gecos field says, matching every other first-party agent.
[[nodiscard]] QString formatUserIdentityLabel(qint64 uid, const QString &loginName,
                                              const QString &fullName);

// The identity polkit listed for the current user when one is present
// (matched by AgentIdentity::isCurrentUser, set by the caller from a uid
// comparison), otherwise the first identity polkit offered. Returns -1 only
// when polkit offered no identity at all, which the caller must treat as a
// hard failure: authentication cannot proceed without one.
[[nodiscard]] int selectPreferredIdentityIndex(const QList<AgentIdentity> &identities);

} // namespace QindaQt::Apps::PolkitAgent
