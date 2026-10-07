// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_validation.h>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
namespace QindaQt::RemovableMedia {
enum class MessageKind : quint32 {
  Snapshot = 1, ActionRequest = 2, ActionAdmission = 3, OperationResult = 4
};
enum class CodecError {
  None, InvalidValue, PayloadTooLarge, Truncated, InvalidMagic,
  UnsupportedCodecVersion, WrongMessageKind, InvalidBoolean, InvalidUtf8,
  TrailingBytes
};
struct CodecResult {
  CodecError error = CodecError::None;
  ValueError valueError = ValueError::None;
  [[nodiscard]] bool succeeded() const noexcept { return error == CodecError::None; }
};
struct EncodeResult final : CodecResult { QByteArray payload; };
// AGENT-CONTRACT: ADR-0350 little-endian envelope layout is protocol-v1 ABI.
// All calls are reentrant, without ambient state. Input views are borrowed
// only during decoding; destinations own copies and change only on success.
// Failure returns typed metadata and no encoded prefix/partial destination.
// Byte-layout changes require a new accepted codec version; no guessing.
[[nodiscard]] EncodeResult encodeSnapshot(const Snapshot &value);
[[nodiscard]] CodecResult decodeSnapshot(QByteArrayView bytes, Snapshot &destination);
[[nodiscard]] EncodeResult encodeActionRequest(const ActionRequest &value);
[[nodiscard]] CodecResult decodeActionRequest(QByteArrayView bytes, ActionRequest &destination);
[[nodiscard]] EncodeResult encodeActionAdmission(const ActionAdmission &value);
[[nodiscard]] CodecResult decodeActionAdmission(QByteArrayView bytes, ActionAdmission &destination);
[[nodiscard]] EncodeResult encodeOperationResult(const OperationResult &value);
[[nodiscard]] CodecResult decodeOperationResult(QByteArrayView bytes, OperationResult &destination);
} // namespace QindaQt::RemovableMedia
