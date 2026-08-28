// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QByteArrayView>

namespace QindaQt::Power {

enum class CodecError {
  None,
  InvalidValue,
  PayloadTooLarge,
  Truncated,
  InvalidMagic,
  UnsupportedCodecVersion,
};

struct EncodeResult {
  QByteArray payload;
  CodecError error = CodecError::None;
  QString reasonCode;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == CodecError::None;
  }
};

struct DecodeResult {
  CodecError error = CodecError::None;
  QString reasonCode;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == CodecError::None;
  }
};

// AGENT-CONTRACT: Canonical codecs are reentrant and borrow input only for the
// duration of a call. Decoders validate a complete temporary and replace the
// destination only on success, so malformed input can never partially replace
// a client's last accepted snapshot. Any byte-layout change requires a new
// accepted codec/protocol version.
[[nodiscard]] EncodeResult encodeSnapshot(const Snapshot &snapshot);
[[nodiscard]] DecodeResult decodeSnapshot(QByteArrayView payload,
                                          Snapshot &destination);
[[nodiscard]] EncodeResult encodeOperationResult(const OperationResult &result);
[[nodiscard]] DecodeResult decodeOperationResult(QByteArrayView payload,
                                                 OperationResult &destination);

} // namespace QindaQt::Power
