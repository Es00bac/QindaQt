// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_codec.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include "power_codec_p.h"

#include <algorithm>
#include <iterator>

namespace QindaQt::Power {
using namespace CodecPrivate;

EncodeResult encodeSnapshot(const Snapshot &snapshot) {
  if (const ValidationResult validation = validateSnapshot(snapshot);
      !validation.accepted) {
    return {.payload = {},
            .error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  Writer writer;
  writer.raw(kSnapshotMagic, std::size(kSnapshotMagic));
  writer.u32(kCanonicalCodecVersion);
  writer.u32(snapshot.protocolVersion);
  writer.u64(snapshot.epoch);
  writer.u64(snapshot.revision);
  writer.u32(static_cast<quint32>(snapshot.availability));
  writer.u32(static_cast<quint32>(snapshot.capabilities.toInt()));
  writer.text(snapshot.reasonCode);
  writer.text(snapshot.diagnostic);
  writeSourceTruth(writer, snapshot.source);
  writeComposite(writer, snapshot.composite);
  writeList(writer, snapshot.supplies, writeSupply);
  writeProfileState(writer, snapshot.profiles);
  writeList(writer, snapshot.inhibitors, writeInhibitor);
  writeList(writer, snapshot.keyboardBacklights, writeKeyboardBacklight);
  writeList(writer, snapshot.internalBacklights, writeInternalBacklight);
  writeWaylandBinding(writer, snapshot.waylandBinding);
  if (!writer.good()) {
    return {.payload = {},
            .error = CodecError::PayloadTooLarge,
            .reasonCode = QStringLiteral("snapshot-encode-failed")};
  }
  return {
      .payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeSnapshot(const QByteArrayView payload,
                            Snapshot &destination) {
  Reader reader(payload);
  char magic[std::size(kSnapshotMagic)]{};
  quint32 codecVersion = 0;
  quint32 availability = 0;
  quint32 capabilities = 0;
  Snapshot snapshot;
  if (!reader.raw(magic, std::size(magic))) {
    return readerFailure(reader, QStringLiteral("truncated-snapshot"));
  }
  if (!std::equal(std::begin(magic), std::end(magic),
                  std::begin(kSnapshotMagic))) {
    return {.error = CodecError::InvalidMagic,
            .reasonCode = QStringLiteral("invalid-snapshot-magic")};
  }
  if (!reader.u32(codecVersion)) {
    return readerFailure(reader, QStringLiteral("truncated-snapshot"));
  }
  if (codecVersion != kCanonicalCodecVersion) {
    return {.error = CodecError::UnsupportedCodecVersion,
            .reasonCode = QStringLiteral("unsupported-codec-version")};
  }
  if (!reader.u32(snapshot.protocolVersion) || !reader.u64(snapshot.epoch) ||
      !reader.u64(snapshot.revision) || !reader.u32(availability) ||
      !reader.u32(capabilities) ||
      !reader.text(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes) ||
      !reader.text(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes) ||
      !readSourceTruth(reader, snapshot.source) ||
      !readComposite(reader, snapshot.composite) ||
      !readBoundedList(reader, snapshot.supplies, kMaxPowerSupplies,
                       readSupply) ||
      !readProfileState(reader, snapshot.profiles) ||
      !readBoundedList(reader, snapshot.inhibitors, kMaxInhibitors,
                       readInhibitor) ||
      !readBoundedList(reader, snapshot.keyboardBacklights,
                       kMaxKeyboardBacklights, readKeyboardBacklight) ||
      !readBoundedList(reader, snapshot.internalBacklights,
                       kMaxInternalBacklights, readInternalBacklight) ||
      !readWaylandBinding(reader, snapshot.waylandBinding)) {
    return readerFailure(reader, QStringLiteral("invalid-snapshot-field"));
  }
  if (!reader.finished()) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = QStringLiteral("trailing-snapshot-bytes")};
  }
  snapshot.availability = static_cast<Availability>(availability);
  snapshot.capabilities = Capabilities::fromInt(capabilities);
  if (const ValidationResult validation = validateSnapshot(snapshot);
      !validation.accepted) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  destination = std::move(snapshot);
  return {};
}

EncodeResult encodeOperationResult(const OperationResult &result) {
  if (const ValidationResult validation = validateOperationResult(result);
      !validation.accepted) {
    return {.payload = {},
            .error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  Writer writer;
  writer.raw(kOperationResultMagic, std::size(kOperationResultMagic));
  writer.u32(kCanonicalCodecVersion);
  writer.u32(static_cast<quint32>(result.kind));
  writer.u32(static_cast<quint32>(result.status));
  writer.u64(result.initiatingEpoch);
  writer.u64(result.initiatingRevision);
  writer.u64(result.observedEpoch);
  writer.u64(result.observedRevision);
  writer.text(result.reasonCode);
  writer.text(result.diagnostic);
  if (!writer.good()) {
    return {.payload = {},
            .error = CodecError::PayloadTooLarge,
            .reasonCode = QStringLiteral("result-encode-failed")};
  }
  return {
      .payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeOperationResult(const QByteArrayView payload,
                                   OperationResult &destination) {
  Reader reader(payload);
  char magic[std::size(kOperationResultMagic)]{};
  quint32 codecVersion = 0;
  quint32 kind = 0;
  quint32 status = 0;
  OperationResult result;
  if (!reader.raw(magic, std::size(magic))) {
    return readerFailure(reader, QStringLiteral("truncated-result"));
  }
  if (!std::equal(std::begin(magic), std::end(magic),
                  std::begin(kOperationResultMagic))) {
    return {.error = CodecError::InvalidMagic,
            .reasonCode = QStringLiteral("invalid-result-magic")};
  }
  if (!reader.u32(codecVersion)) {
    return readerFailure(reader, QStringLiteral("truncated-result"));
  }
  if (codecVersion != kCanonicalCodecVersion) {
    return {.error = CodecError::UnsupportedCodecVersion,
            .reasonCode = QStringLiteral("unsupported-codec-version")};
  }
  if (!reader.u32(kind) || !reader.u32(status) ||
      !reader.u64(result.initiatingEpoch) ||
      !reader.u64(result.initiatingRevision) ||
      !reader.u64(result.observedEpoch) ||
      !reader.u64(result.observedRevision) ||
      !reader.text(result.reasonCode, kMaxReasonCodeUtf8Bytes) ||
      !reader.text(result.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return readerFailure(reader, QStringLiteral("invalid-result-field"));
  }
  if (!reader.finished()) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = QStringLiteral("trailing-result-bytes")};
  }
  result.kind = static_cast<OperationKind>(kind);
  result.status = static_cast<OperationStatus>(status);
  if (const ValidationResult validation = validateOperationResult(result);
      !validation.accepted) {
    return {.error = CodecError::InvalidValue,
            .reasonCode = validation.reasonCode};
  }
  destination = std::move(result);
  return {};
}

} // namespace QindaQt::Power
