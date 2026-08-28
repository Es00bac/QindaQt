// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_codec.h>
#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_protocol/display_limits.h>

#include "support/display_protocol_test_data.h"

#include <QtDBus/QDBusMetaType>
#include <QtCore/QVariant>
#include <QtTest>

using namespace QindaQt::Display;

namespace
{

void setBigEndian32(QByteArray &payload, const qsizetype offset, const quint32 value)
{
    QVERIFY(offset >= 0 && offset + 4 <= payload.size());
    payload[offset] = static_cast<char>((value >> 24U) & 0xffU);
    payload[offset + 1] = static_cast<char>((value >> 16U) & 0xffU);
    payload[offset + 2] = static_cast<char>((value >> 8U) & 0xffU);
    payload[offset + 3] = static_cast<char>(value & 0xffU);
}

template<typename T>
QDBusArgument marshalledArgument(const T &value)
{
    QDBusArgument writer;
    writer << value;
    return qvariant_cast<QDBusArgument>(QVariant::fromValue(writer));
}

} // namespace

class DisplayProtocolCodecTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void candidateRoundTripIsCanonical();
    void snapshotRoundTripIsCanonical();
    void rejectsOldNewTruncatedAndOversizedPayloadsWithoutPartialOutput();
    void fixedDbusSignaturesAndRejectsNonDemarshallingArguments();
};

void DisplayProtocolCodecTests::candidateRoundTripIsCanonical()
{
    const Candidate source = Test::candidate();
    const EncodeResult encoded = encodeCandidate(source);
    QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));
    Candidate decoded;
    const DecodeResult result = decodeCandidate(encoded.payload, decoded);
    QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
    QCOMPARE(decoded, source);
    QCOMPARE(encodeCandidate(decoded).payload, encoded.payload);
}

void DisplayProtocolCodecTests::snapshotRoundTripIsCanonical()
{
    Snapshot source = Test::snapshot();
    source.transactions = {{.transactionId = QStringLiteral("transaction"),
                            .state = TransactionState::AwaitingConfirmation,
                            .reason = TransactionReason::TransportUncertain,
                            .initiatingEpoch = source.serviceEpoch,
                            .baseRevision = source.revision,
                            .observedRevision = source.revision,
                            .deadlineMonotonicMilliseconds = 42'000,
                            .revertAttempt = 0}};
    const EncodeResult encoded = encodeSnapshot(source);
    QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));
    Snapshot decoded;
    const DecodeResult result = decodeSnapshot(encoded.payload, decoded);
    QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
    QCOMPARE(decoded, source);
    QCOMPARE(encodeSnapshot(decoded).payload, encoded.payload);
}

void DisplayProtocolCodecTests::rejectsOldNewTruncatedAndOversizedPayloadsWithoutPartialOutput()
{
    const Candidate original = Test::candidate();
    const QByteArray canonical = encodeCandidate(original).payload;
    Candidate destination = original;
    destination.baseRevision = 99;
    const Candidate prior = destination;

    QByteArray changed = canonical;
    setBigEndian32(changed, 4, 0);
    QCOMPARE(decodeCandidate(changed, destination).error,
             CodecError::UnsupportedCodecVersion);
    QCOMPARE(destination, prior);
    changed = canonical;
    setBigEndian32(changed, 4, kCanonicalCodecVersion + 1);
    QCOMPARE(decodeCandidate(changed, destination).error,
             CodecError::UnsupportedCodecVersion);
    QCOMPARE(destination, prior);
    changed = canonical;
    setBigEndian32(changed, 8, kProtocolVersion + 1);
    QCOMPARE(decodeCandidate(changed, destination).error, CodecError::InvalidValue);
    QCOMPARE(destination, prior);
    changed = canonical.first(canonical.size() - 1);
    QCOMPARE(decodeCandidate(changed, destination).error, CodecError::Truncated);
    QCOMPARE(destination, prior);
    changed = canonical + QByteArray(1, '\0');
    QCOMPARE(decodeCandidate(changed, destination).error, CodecError::InvalidValue);
    QCOMPARE(destination, prior);
    changed = QByteArray(kMaxSerializedBytes + 1, '\0');
    QCOMPARE(decodeCandidate(changed, destination).error, CodecError::PayloadTooLarge);
    QCOMPARE(destination, prior);
}

void DisplayProtocolCodecTests::fixedDbusSignaturesAndRejectsNonDemarshallingArguments()
{
    registerDBusTypes();
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Mode>()), "(siiub)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Output>()),
             "(ssssssiibbbbbsiiiiduusa(siiub))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<CandidateOutput>()),
             "(sbbsiiduus)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Candidate>()),
             "(usta(sbbsiiduus))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<TransactionSummary>()),
             "(suustttu)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Snapshot>()),
             "(ustaya(ssssssiibbbbbsiiiiduusa(siiub))a(suustttu))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<OperationResult>()),
             "(uuusttss)");

    Candidate candidateDestination = Test::candidate();
    candidateDestination.baseRevision = 99;
    const Candidate candidatePrior = candidateDestination;
    // A locally marshalled argument has a correct static signature but is
    // deliberately write-only. Positive inbound demarshalling needs a real
    // D-Bus message and belongs to D2 private-bus integration; D1 must reject
    // this object without attempting extraction or mutating the destination.
    QVERIFY(!decodeCandidateArgument(marshalledArgument(Test::candidate()),
                                     candidateDestination)
                 .accepted);
    QCOMPARE(candidateDestination, candidatePrior);
    QVERIFY(!decodeCandidateArgument(marshalledArgument(QStringLiteral("wrong-signature")),
                                     candidateDestination)
                 .accepted);
    QCOMPARE(candidateDestination, candidatePrior);

    Snapshot oversized = Test::snapshot();
    oversized.outputs.clear();
    for (qsizetype index = 0; index <= kMaxOutputs; ++index) {
        Output value = Test::output(QStringLiteral("conn:%1").arg(index),
                                    QStringLiteral("DP-%1").arg(index));
        value.primary = index == 0;
        value.priority = static_cast<quint32>(index + 1);
        oversized.outputs.push_back(std::move(value));
    }
    Snapshot destination = Test::snapshot();
    destination.revision = 88;
    const Snapshot prior = destination;
    const DBusDecodeResult decoded = decodeSnapshotArgument(
        marshalledArgument(oversized), destination);
    QVERIFY(!decoded.accepted);
    QCOMPARE(destination, prior);
}

QTEST_GUILESS_MAIN(DisplayProtocolCodecTests)
#include "tst_display_protocol_codec.moc"
