// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_codec.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "display_codec_p.h"

#include <algorithm>
#include <iterator>

namespace QindaQt::Display
{
namespace
{

using namespace CodecPrivate;

void writeOutput(Writer &writer, const Output &output)
{
    writer.text(output.stableId);
    writer.text(output.connectorName);
    writer.text(output.runtimeCompositorUuid);
    writer.text(output.label);
    writer.text(output.manufacturer);
    writer.text(output.model);
    writer.i32(output.physicalSizeMillimeters.width());
    writer.i32(output.physicalSizeMillimeters.height());
    writer.boolean(output.hasSerial);
    writer.boolean(output.internal);
    writer.boolean(output.ambiguousIdentity);
    writer.boolean(output.enabled);
    writer.boolean(output.primary);
    writer.text(output.modeId);
    writer.i32(output.position.x());
    writer.i32(output.position.y());
    writer.i32(output.logicalSize.width());
    writer.i32(output.logicalSize.height());
    writer.real(output.scale);
    writer.u32(static_cast<quint32>(output.transform));
    writer.u32(output.priority);
    writer.text(output.replicationSourceStableId);
    writer.u32(static_cast<quint32>(output.modes.size()));
    for (const Mode &mode : output.modes) {
        writeMode(writer, mode);
    }
}

bool readOutput(Reader &reader, Output &output)
{
    qint32 physicalWidth = 0;
    qint32 physicalHeight = 0;
    qint32 x = 0;
    qint32 y = 0;
    qint32 logicalWidth = 0;
    qint32 logicalHeight = 0;
    quint32 transform = 0;
    quint32 modeCount = 0;
    if (!reader.text(output.stableId, kMaxStableIdUtf8Bytes)
        || !reader.text(output.connectorName, kMaxConnectorNameUtf8Bytes)
        || !reader.text(output.runtimeCompositorUuid, kMaxRuntimeUuidUtf8Bytes)
        || !reader.text(output.label, kMaxLabelUtf8Bytes)
        || !reader.text(output.manufacturer, kMaxManufacturerUtf8Bytes)
        || !reader.text(output.model, kMaxModelUtf8Bytes) || !reader.i32(physicalWidth)
        || !reader.i32(physicalHeight) || !reader.boolean(output.hasSerial)
        || !reader.boolean(output.internal) || !reader.boolean(output.ambiguousIdentity)
        || !reader.boolean(output.enabled) || !reader.boolean(output.primary)
        || !reader.text(output.modeId, kMaxModeIdUtf8Bytes) || !reader.i32(x)
        || !reader.i32(y) || !reader.i32(logicalWidth) || !reader.i32(logicalHeight)
        || !reader.real(output.scale) || !reader.u32(transform)
        || !reader.u32(output.priority)
        || !reader.text(output.replicationSourceStableId, kMaxStableIdUtf8Bytes)
        || !reader.u32(modeCount)) {
        return false;
    }
    if (modeCount > static_cast<quint32>(kMaxModesPerOutput)) {
        reader.fail(CodecError::PayloadTooLarge);
        return false;
    }
    output.physicalSizeMillimeters = QSize(physicalWidth, physicalHeight);
    output.position = QPoint(x, y);
    output.logicalSize = QSize(logicalWidth, logicalHeight);
    output.transform = static_cast<Transform>(transform);
    output.modes.reserve(static_cast<qsizetype>(modeCount));
    for (quint32 index = 0; index < modeCount; ++index) {
        Mode mode;
        if (!readMode(reader, mode)) {
            return false;
        }
        output.modes.push_back(std::move(mode));
    }
    return true;
}

void writeTransaction(Writer &writer, const TransactionSummary &summary)
{
    writer.text(summary.transactionId);
    writer.u32(static_cast<quint32>(summary.state));
    writer.u32(static_cast<quint32>(summary.reason));
    writer.text(summary.initiatingEpoch);
    writer.u64(summary.baseRevision);
    writer.u64(summary.observedRevision);
    writer.u64(summary.deadlineMonotonicMilliseconds);
    writer.u32(summary.revertAttempt);
}

bool readTransaction(Reader &reader, TransactionSummary &summary)
{
    quint32 state = 0;
    quint32 reason = 0;
    if (!reader.text(summary.transactionId, kMaxTransactionIdUtf8Bytes)
        || !reader.u32(state) || !reader.u32(reason)
        || !reader.text(summary.initiatingEpoch, kMaxServiceEpochUtf8Bytes)
        || !reader.u64(summary.baseRevision) || !reader.u64(summary.observedRevision)
        || !reader.u64(summary.deadlineMonotonicMilliseconds)
        || !reader.u32(summary.revertAttempt)) {
        return false;
    }
    summary.state = static_cast<TransactionState>(state);
    summary.reason = static_cast<TransactionReason>(reason);
    return true;
}

} // namespace

EncodeResult encodeSnapshot(const Snapshot &snapshot)
{
    if (const ValidationResult validation = validateSnapshot(snapshot); !validation.accepted) {
        return {.payload = {},
                .error = CodecError::InvalidValue,
                .reasonCode = validation.reasonCode};
    }
    CodecPrivate::Writer writer;
    writer.raw(CodecPrivate::kSnapshotMagic, std::size(CodecPrivate::kSnapshotMagic));
    writer.u32(kCanonicalCodecVersion);
    writer.u32(snapshot.protocolVersion);
    writer.text(snapshot.serviceEpoch);
    writer.u64(snapshot.revision);
    writer.bytes(snapshot.liveFingerprint);
    writer.u32(static_cast<quint32>(snapshot.outputs.size()));
    for (const Output &output : snapshot.outputs) {
        writeOutput(writer, output);
    }
    writer.u32(static_cast<quint32>(snapshot.transactions.size()));
    for (const TransactionSummary &summary : snapshot.transactions) {
        writeTransaction(writer, summary);
    }
    if (!writer.good()) {
        return {.payload = {},
                .error = CodecError::PayloadTooLarge,
                .reasonCode = QStringLiteral("snapshot-encode-failed")};
    }
    return {.payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeSnapshot(const QByteArrayView payload, Snapshot &destination)
{
    CodecPrivate::Reader reader(payload);
    char magic[std::size(CodecPrivate::kSnapshotMagic)]{};
    quint32 codecVersion = 0;
    Snapshot snapshot;
    quint32 outputCount = 0;
    quint32 transactionCount = 0;
    if (!reader.raw(magic, std::size(magic))) {
        return CodecPrivate::readerFailure(reader, QStringLiteral("truncated-snapshot"));
    }
    if (!std::equal(std::begin(magic), std::end(magic),
                    std::begin(CodecPrivate::kSnapshotMagic))) {
        return {.error = CodecError::InvalidMagic,
                .reasonCode = QStringLiteral("invalid-snapshot-magic")};
    }
    if (!reader.u32(codecVersion)) {
        return CodecPrivate::readerFailure(reader, QStringLiteral("truncated-snapshot"));
    }
    if (codecVersion != kCanonicalCodecVersion) {
        return {.error = CodecError::UnsupportedCodecVersion,
                .reasonCode = QStringLiteral("unsupported-codec-version")};
    }
    if (!reader.u32(snapshot.protocolVersion)
        || !reader.text(snapshot.serviceEpoch, kMaxServiceEpochUtf8Bytes)
        || !reader.u64(snapshot.revision)
        || !reader.bytes(snapshot.liveFingerprint, kFingerprintBytes)
        || !reader.u32(outputCount)) {
        return CodecPrivate::readerFailure(reader, QStringLiteral("truncated-snapshot"));
    }
    if (outputCount == 0 || outputCount > static_cast<quint32>(kMaxOutputs)) {
        return {.error = CodecError::PayloadTooLarge,
                .reasonCode = QStringLiteral("invalid-snapshot-output-count")};
    }
    snapshot.outputs.reserve(static_cast<qsizetype>(outputCount));
    for (quint32 index = 0; index < outputCount; ++index) {
        Output output;
        if (!readOutput(reader, output)) {
            return CodecPrivate::readerFailure(reader, QStringLiteral("invalid-output"));
        }
        snapshot.outputs.push_back(std::move(output));
    }
    if (!reader.u32(transactionCount)
        || transactionCount > static_cast<quint32>(kMaxTransactions)) {
        return CodecPrivate::readerFailure(reader, QStringLiteral("invalid-transaction-count"));
    }
    snapshot.transactions.reserve(static_cast<qsizetype>(transactionCount));
    for (quint32 index = 0; index < transactionCount; ++index) {
        TransactionSummary summary;
        if (!readTransaction(reader, summary)) {
            return CodecPrivate::readerFailure(reader, QStringLiteral("invalid-transaction"));
        }
        snapshot.transactions.push_back(std::move(summary));
    }
    if (!reader.finished()) {
        return CodecPrivate::readerFailure(reader, QStringLiteral("trailing-snapshot-bytes"));
    }
    if (const ValidationResult validation = validateSnapshot(snapshot); !validation.accepted) {
        return {.error = CodecError::InvalidValue, .reasonCode = validation.reasonCode};
    }
    destination = std::move(snapshot);
    return {};
}

} // namespace QindaQt::Display
