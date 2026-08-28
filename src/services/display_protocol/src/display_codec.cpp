// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_codec.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "display_codec_p.h"

#include <algorithm>
#include <iterator>

namespace QindaQt::Display
{
using namespace CodecPrivate;

EncodeResult encodeCandidate(const Candidate &candidate)
{
    if (const ValidationResult validation = validateCandidate(candidate); !validation.accepted) {
        return {.payload = {},
                .error = CodecError::InvalidValue,
                .reasonCode = validation.reasonCode};
    }
    Writer writer;
    writer.raw(kCandidateMagic, std::size(kCandidateMagic));
    writer.u32(kCanonicalCodecVersion);
    writer.u32(candidate.protocolVersion);
    writer.text(candidate.baseEpoch);
    writer.u64(candidate.baseRevision);
    writer.u32(static_cast<quint32>(candidate.outputs.size()));
    for (const CandidateOutput &output : candidate.outputs) {
        writeCandidateOutput(writer, output);
    }
    if (!writer.good()) {
        return {.payload = {},
                .error = CodecError::PayloadTooLarge,
                .reasonCode = QStringLiteral("candidate-encode-failed")};
    }
    return {.payload = writer.take(), .error = CodecError::None, .reasonCode = {}};
}

DecodeResult decodeCandidate(const QByteArrayView payload, Candidate &destination)
{
    Reader reader(payload);
    char magic[std::size(kCandidateMagic)]{};
    quint32 codecVersion = 0;
    Candidate candidate;
    quint32 outputCount = 0;
    if (!reader.raw(magic, std::size(magic))) {
        return readerFailure(reader, QStringLiteral("truncated-candidate"));
    }
    if (!std::equal(std::begin(magic), std::end(magic), std::begin(kCandidateMagic))) {
        return {.error = CodecError::InvalidMagic,
                .reasonCode = QStringLiteral("invalid-candidate-magic")};
    }
    if (!reader.u32(codecVersion)) {
        return readerFailure(reader, QStringLiteral("truncated-candidate"));
    }
    if (codecVersion != kCanonicalCodecVersion) {
        return {.error = CodecError::UnsupportedCodecVersion,
                .reasonCode = QStringLiteral("unsupported-codec-version")};
    }
    if (!reader.u32(candidate.protocolVersion)
        || !reader.text(candidate.baseEpoch, kMaxServiceEpochUtf8Bytes)
        || !reader.u64(candidate.baseRevision) || !reader.u32(outputCount)) {
        return readerFailure(reader, QStringLiteral("truncated-candidate"));
    }
    if (outputCount == 0 || outputCount > static_cast<quint32>(kMaxCandidateOutputs)) {
        return {.error = CodecError::PayloadTooLarge,
                .reasonCode = QStringLiteral("invalid-candidate-output-count")};
    }
    candidate.outputs.reserve(static_cast<qsizetype>(outputCount));
    for (quint32 index = 0; index < outputCount; ++index) {
        CandidateOutput output;
        if (!readCandidateOutput(reader, output)) {
            return readerFailure(reader, QStringLiteral("truncated-candidate-output"));
        }
        candidate.outputs.push_back(std::move(output));
    }
    if (!reader.finished()) {
        return readerFailure(reader, QStringLiteral("trailing-candidate-bytes"));
    }
    if (const ValidationResult validation = validateCandidate(candidate); !validation.accepted) {
        return {.error = CodecError::InvalidValue, .reasonCode = validation.reasonCode};
    }
    destination = std::move(candidate);
    return {};
}

} // namespace QindaQt::Display
