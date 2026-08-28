// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_transaction/transaction_types.h>

#include <QtCore/QByteArrayView>

namespace QindaQt::DisplayTransaction
{

enum class JournalCodecError {
    None,
    InvalidValue,
    PayloadTooLarge,
    Truncated,
    InvalidMagic,
    UnsupportedVersion,
};

struct JournalEncodeResult {
    QByteArray payload;
    JournalCodecError error = JournalCodecError::None;
    QString reasonCode;

    [[nodiscard]] bool succeeded() const noexcept { return error == JournalCodecError::None; }
};

struct JournalDecodeResult {
    JournalCodecError error = JournalCodecError::None;
    QString reasonCode;

    [[nodiscard]] bool succeeded() const noexcept { return error == JournalCodecError::None; }
};

[[nodiscard]] bool isValidJournal(const Journal &journal);
[[nodiscard]] JournalEncodeResult encodeJournal(const Journal &journal);
// Values are borrowed during each call and returned payloads are owned. These
// functions are reentrant and thread-safe. Failure preserves destination
// exactly; recovery never receives a partial preimage or target from hostile
// or torn bytes. Codec/schema additions require a new accepted version.
[[nodiscard]] JournalDecodeResult decodeJournal(QByteArrayView payload, Journal &destination);

} // namespace QindaQt::DisplayTransaction
