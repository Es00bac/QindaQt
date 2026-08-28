// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_transaction/transaction_journal.h>

#include <qindaqt/services/display_protocol/display_codec.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>

#include <algorithm>
#include <iterator>

namespace QindaQt::DisplayTransaction
{
namespace
{

inline constexpr char kMagic[] = {'Q', 'D', 'J', '1'};
inline constexpr quint32 kCodecVersion = 1;

bool sameOutputSet(const Display::Candidate &left, const Display::Candidate &right)
{
    QSet<QString> leftIds;
    QSet<QString> rightIds;
    for (const Display::CandidateOutput &output : left.outputs) {
        leftIds.insert(output.stableId);
    }
    for (const Display::CandidateOutput &output : right.outputs) {
        rightIds.insert(output.stableId);
    }
    return leftIds == rightIds;
}

void writeBytes(QDataStream &stream, const QByteArray &value)
{
    stream << static_cast<quint32>(value.size());
    stream.writeRawData(value.constData(), static_cast<qint64>(value.size()));
}

bool readBytes(QDataStream &stream, QBuffer &buffer, QByteArray &value,
               const qsizetype maximum)
{
    quint32 size = 0;
    stream >> size;
    if (stream.status() != QDataStream::Ok
        || static_cast<quint64>(size) > static_cast<quint64>(maximum)
        || static_cast<qint64>(size) > buffer.bytesAvailable()) {
        return false;
    }
    QByteArray decoded(static_cast<qsizetype>(size), Qt::Uninitialized);
    if (stream.readRawData(decoded.data(), static_cast<qint64>(decoded.size()))
        != static_cast<qint64>(decoded.size())) {
        return false;
    }
    value = std::move(decoded);
    return true;
}

bool readText(QDataStream &stream, QBuffer &buffer, QString &value,
              const qsizetype maximum)
{
    QByteArray bytes;
    if (!readBytes(stream, buffer, bytes, maximum)) {
        return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString decoded = decoder.decode(bytes);
    if (decoder.hasError() || decoded.contains(QChar::Null)) {
        return false;
    }
    value = std::move(decoded);
    return true;
}

JournalDecodeResult decodeFailure(const JournalCodecError error, const char *reason)
{
    return {.error = error, .reasonCode = QString::fromLatin1(reason)};
}

} // namespace

bool isValidJournal(const Journal &journal)
{
    if (journal.schemaVersion != kJournalSchemaVersion || journal.transactionId.isEmpty()
        || !Display::isBoundedText(journal.transactionId,
                                   Display::kMaxTransactionIdUtf8Bytes)
        || static_cast<quint32>(journal.phase) > static_cast<quint32>(JournalPhase::Stuck)
        || static_cast<quint32>(journal.reason)
            > static_cast<quint32>(Display::TransactionReason::TransportUncertain)
        || journal.revertAttempt > kMaximumRevertAttempts
        || !Display::validateCandidate(journal.preimage).accepted
        || !Display::validateCandidate(journal.target).accepted
        || journal.preimage.baseEpoch != journal.target.baseEpoch
        || journal.preimage.baseRevision != journal.target.baseRevision
        || !sameOutputSet(journal.preimage, journal.target)) {
        return false;
    }
    return true;
}

JournalEncodeResult encodeJournal(const Journal &journal)
{
    if (!isValidJournal(journal)) {
        return {.payload = {},
                .error = JournalCodecError::InvalidValue,
                .reasonCode = QStringLiteral("invalid-journal")};
    }
    const Display::EncodeResult preimage = Display::encodeCandidate(journal.preimage);
    const Display::EncodeResult target = Display::encodeCandidate(journal.target);
    if (!preimage.succeeded() || !target.succeeded()) {
        return {.payload = {},
                .error = JournalCodecError::InvalidValue,
                .reasonCode = QStringLiteral("invalid-journal-candidate")};
    }
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.writeRawData(kMagic, static_cast<qint64>(std::size(kMagic)));
    stream << kCodecVersion << journal.schemaVersion;
    writeBytes(stream, journal.transactionId.toUtf8());
    stream << static_cast<quint32>(journal.phase) << static_cast<quint32>(journal.reason)
           << journal.revertAttempt;
    writeBytes(stream, preimage.payload);
    writeBytes(stream, target.payload);
    buffer.close();
    if (stream.status() != QDataStream::Ok || payload.size() > kMaximumJournalBytes) {
        return {.payload = {},
                .error = JournalCodecError::PayloadTooLarge,
                .reasonCode = QStringLiteral("journal-too-large")};
    }
    return {.payload = std::move(payload),
            .error = JournalCodecError::None,
            .reasonCode = {}};
}

JournalDecodeResult decodeJournal(const QByteArrayView payload, Journal &destination)
{
    if (payload.size() > kMaximumJournalBytes) {
        return decodeFailure(JournalCodecError::PayloadTooLarge, "journal-too-large");
    }
    QByteArray storage(payload.data(), payload.size());
    QBuffer buffer(&storage);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);
    char magic[std::size(kMagic)]{};
    if (stream.readRawData(magic, static_cast<qint64>(std::size(magic)))
        != static_cast<qint64>(std::size(magic))) {
        return decodeFailure(JournalCodecError::Truncated, "truncated-journal");
    }
    if (!std::equal(std::begin(magic), std::end(magic), std::begin(kMagic))) {
        return decodeFailure(JournalCodecError::InvalidMagic, "invalid-journal-magic");
    }
    quint32 codecVersion = 0;
    Journal journal;
    quint32 phase = 0;
    quint32 reason = 0;
    QByteArray preimagePayload;
    QByteArray targetPayload;
    stream >> codecVersion >> journal.schemaVersion;
    if (stream.status() != QDataStream::Ok) {
        return decodeFailure(JournalCodecError::Truncated, "truncated-journal");
    }
    if (codecVersion != kCodecVersion || journal.schemaVersion != kJournalSchemaVersion) {
        return decodeFailure(JournalCodecError::UnsupportedVersion,
                             "unsupported-journal-version");
    }
    if (!readText(stream, buffer, journal.transactionId,
                  Display::kMaxTransactionIdUtf8Bytes)) {
        return decodeFailure(JournalCodecError::Truncated, "invalid-journal-id");
    }
    stream >> phase >> reason >> journal.revertAttempt;
    if (stream.status() != QDataStream::Ok
        || !readBytes(stream, buffer, preimagePayload, Display::kMaxSerializedBytes)
        || !readBytes(stream, buffer, targetPayload, Display::kMaxSerializedBytes)
        || !buffer.atEnd()) {
        return decodeFailure(JournalCodecError::Truncated, "truncated-journal");
    }
    journal.phase = static_cast<JournalPhase>(phase);
    journal.reason = static_cast<Display::TransactionReason>(reason);
    Display::Candidate preimage;
    Display::Candidate target;
    if (!Display::decodeCandidate(preimagePayload, preimage).succeeded()
        || !Display::decodeCandidate(targetPayload, target).succeeded()) {
        return decodeFailure(JournalCodecError::InvalidValue, "invalid-journal-candidate");
    }
    journal.preimage = std::move(preimage);
    journal.target = std::move(target);
    if (!isValidJournal(journal)) {
        return decodeFailure(JournalCodecError::InvalidValue, "invalid-journal");
    }
    destination = std::move(journal);
    return {};
}

} // namespace QindaQt::DisplayTransaction
