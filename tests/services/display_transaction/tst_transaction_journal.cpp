// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

namespace
{

Journal journal()
{
    const Display::Snapshot base = Test::snapshot();
    return {.schemaVersion = kJournalSchemaVersion,
            .transactionId = QStringLiteral("tx"),
            .phase = JournalPhase::AwaitingConfirmation,
            .reason = Display::TransactionReason::TransportUncertain,
            .preimage = DisplayTopology::candidateFromSnapshot(base),
            .target = Test::changedCandidate(base),
            .revertAttempt = 0};
}

void setBigEndian32(QByteArray &payload, const qsizetype offset, const quint32 value)
{
    QVERIFY(offset >= 0 && offset + 4 <= payload.size());
    payload[offset] = static_cast<char>((value >> 24U) & 0xffU);
    payload[offset + 1] = static_cast<char>((value >> 16U) & 0xffU);
    payload[offset + 2] = static_cast<char>((value >> 8U) & 0xffU);
    payload[offset + 3] = static_cast<char>(value & 0xffU);
}

} // namespace

class TransactionJournalTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void canonicalRoundTrip();
    void rejectsInvalidValues();
    void hostileBytesPreservePriorDestination();
};

void TransactionJournalTests::canonicalRoundTrip()
{
    const Journal source = journal();
    const JournalEncodeResult encoded = encodeJournal(source);
    QVERIFY2(encoded.succeeded(), qPrintable(encoded.reasonCode));
    Journal decoded;
    const JournalDecodeResult result = decodeJournal(encoded.payload, decoded);
    QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
    QCOMPARE(decoded, source);
    QCOMPARE(encodeJournal(decoded).payload, encoded.payload);
}

void TransactionJournalTests::rejectsInvalidValues()
{
    Journal invalid = journal();
    invalid.schemaVersion++;
    QVERIFY(!isValidJournal(invalid));
    invalid = journal();
    invalid.transactionId.clear();
    QVERIFY(!isValidJournal(invalid));
    invalid = journal();
    invalid.phase = static_cast<JournalPhase>(99);
    QVERIFY(!isValidJournal(invalid));
    invalid = journal();
    invalid.reason = static_cast<Display::TransactionReason>(99);
    QVERIFY(!isValidJournal(invalid));
    invalid = journal();
    invalid.revertAttempt = kMaximumRevertAttempts + 1;
    QVERIFY(!isValidJournal(invalid));
    invalid = journal();
    invalid.target.outputs[0].stableId = QStringLiteral("different-output");
    QVERIFY(!isValidJournal(invalid));
}

void TransactionJournalTests::hostileBytesPreservePriorDestination()
{
    const QByteArray canonical = encodeJournal(journal()).payload;
    Journal destination = journal();
    destination.transactionId = QStringLiteral("prior");
    const Journal prior = destination;

    QByteArray payload = canonical;
    setBigEndian32(payload, 4, 0);
    QCOMPARE(decodeJournal(payload, destination).error,
             JournalCodecError::UnsupportedVersion);
    QCOMPARE(destination, prior);
    payload = canonical;
    setBigEndian32(payload, 4, 2);
    QCOMPARE(decodeJournal(payload, destination).error,
             JournalCodecError::UnsupportedVersion);
    QCOMPARE(destination, prior);
    payload = canonical.first(canonical.size() - 1);
    QCOMPARE(decodeJournal(payload, destination).error, JournalCodecError::Truncated);
    QCOMPARE(destination, prior);
    payload = canonical + QByteArray(1, '\0');
    QCOMPARE(decodeJournal(payload, destination).error, JournalCodecError::Truncated);
    QCOMPARE(destination, prior);
    payload = QByteArray(kMaximumJournalBytes + 1, '\0');
    QCOMPARE(decodeJournal(payload, destination).error,
             JournalCodecError::PayloadTooLarge);
    QCOMPARE(destination, prior);
}

QTEST_GUILESS_MAIN(TransactionJournalTests)
#include "tst_transaction_journal.moc"
