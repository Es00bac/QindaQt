// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace QindaQt::Link {

// All inputs are the raw 32-byte values defined by the protocol page.
// spkiD = desktop SPKI digest, spkiP = phone SPKI digest.

// SHA-256("QLINK-COMMIT-1" || nd || spkiD || spkiP)
[[nodiscard]] QByteArray pairingCommitment(const QByteArray &nd, const QByteArray &spkiD,
                                           const QByteArray &spkiP);

// uint32_be(SHA-256("QLINK-SAS-1" || spkiD || spkiP || nd || np)[0..3]) mod 1e6,
// as six digits with leading zeros ("007 123" formatting is a UI concern).
[[nodiscard]] QString pairingCode(const QByteArray &spkiD, const QByteArray &spkiP,
                                  const QByteArray &nd, const QByteArray &np);

} // namespace QindaQt::Link
