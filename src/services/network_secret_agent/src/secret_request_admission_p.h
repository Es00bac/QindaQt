// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

namespace QindaQt::Network::SecretAgent::Private {

// Returns true only when every section, property name, and value of an inbound
// NetworkManager connection map fits the admission budget documented in
// docs/wiki/architecture/network-secret-agent.md#request-admission.
// AGENT-CONTRACT: Qt D-Bus wire values (QDBusArgument) are read through
// detached cursors; the caller's map is left intact for wipeSettingsMap().
// Any value shape this walker cannot account for is rejected.
[[nodiscard]] bool boundedConnection(const NmSettingsMap &connection);

// Bounded, non-empty (unless allowEmpty), NUL-free display/key text.
[[nodiscard]] bool boundedText(const QString &text, bool allowEmpty = false);

} // namespace QindaQt::Network::SecretAgent::Private
