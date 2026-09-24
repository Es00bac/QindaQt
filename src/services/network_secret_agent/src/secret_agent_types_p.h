// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QVariant>

namespace QindaQt::Network::SecretAgent::Private {

// Scrub a decoded, directly owned variant before releasing it. Qt D-Bus owns
// the original wire argument until the incoming method call returns.
void wipeVariantValue(QVariant &value) noexcept;

// Scrub a decoded, directly owned string (shared allocations included) before
// releasing it. Static or raw-data views are cleared without writing.
void wipeStringValue(QString &text) noexcept;

// Scrub a decoded byte array without detaching its shared allocation.
void wipeByteArrayValue(QByteArray &bytes) noexcept;

} // namespace QindaQt::Network::SecretAgent::Private
