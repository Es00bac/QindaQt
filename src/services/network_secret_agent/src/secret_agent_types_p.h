// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QVariant>

namespace QindaQt::Network::SecretAgent::Private {

// AGENT-CONTRACT: This intentionally scrubs shared nested storage. Every
// alias must be dead or another secret-bearing copy that must also be scrubbed;
// never pass a value another live request/settings owner still needs.
void wipeVariantValue(QVariant &value) noexcept;

// AGENT-CONTRACT: This intentionally scrubs shared storage. Every alias must
// be dead or another secret-bearing copy that must also be scrubbed; never
// pass a value another live request/settings owner still needs. Static or
// raw-data views are cleared without writing.
void wipeStringValue(QString &text) noexcept;

// AGENT-CONTRACT: This intentionally scrubs shared storage. Every alias must
// be dead or another secret-bearing copy that must also be scrubbed; never
// pass a value another live request/settings owner still needs.
void wipeByteArrayValue(QByteArray &bytes) noexcept;

} // namespace QindaQt::Network::SecretAgent::Private
