// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_color_assignment/assignment_document.h>

#include <QtTest>

using namespace QindaQt::DisplayColor;

namespace
{

constexpr auto kHex64 = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

QVariant recordValue(const QString &profileId, const QString &lineage)
{
    return QVariantMap{{QStringLiteral("profile"), profileId},
                       {QStringLiteral("lineage"), lineage}};
}

} // namespace

class AssignmentDocumentTests final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsCanonicalDocuments();
    void ordersRecordsDeterministically();
    void decodesEmptyDocuments();
    void rejectsHostileDocuments();
    void encodesFailClosedOnInvalidRecords();
    void validatesDrafts();
    void appliesDraftsPurely();
    void retainsAssignmentsForDisconnectedOutputs();

private:
    const QString m_hex = QString::fromLatin1(kHex64);
};

void AssignmentDocumentTests::roundTripsCanonicalDocuments()
{
    AssignmentDocument document;
    ColorAssignmentRecord record;
    record.outputStableId = QStringLiteral("DP-1");
    record.profileId = QStringLiteral("vendor-srgb");
    record.lineageFingerprint = QByteArray::fromHex(kHex64);
    document.records.append(record);

    const std::optional<QVariant> encoded = encodeAssignmentDocument(document);
    QVERIFY(encoded.has_value());
    const AssignmentDocumentDecodeResult decoded = decodeAssignmentDocument(*encoded);
    QVERIFY(decoded.ok);
    QCOMPARE(decoded.document, document);

    // Encoding is canonical: two documents with the same content produce
    // byte-identical QVariant structures.
    const std::optional<QVariant> again = encodeAssignmentDocument(document);
    QVERIFY(again.has_value());
    QCOMPARE(*again, *encoded);
}

void AssignmentDocumentTests::ordersRecordsDeterministically()
{
    const QVariantMap raw{
        {QStringLiteral("zz-Output"), recordValue(QStringLiteral("b"), m_hex)},
        {QStringLiteral("aa-Output"), recordValue(QStringLiteral("a"), QString())},
        {QStringLiteral("mm-Output"), recordValue(QStringLiteral("c"), QString())}};
    const AssignmentDocumentDecodeResult decoded = decodeAssignmentDocument(QVariant(raw));
    QVERIFY(decoded.ok);
    QCOMPARE(decoded.document.records.size(), 3);
    QCOMPARE(decoded.document.records.at(0).outputStableId, QStringLiteral("aa-Output"));
    QCOMPARE(decoded.document.records.at(1).outputStableId, QStringLiteral("mm-Output"));
    QCOMPARE(decoded.document.records.at(2).outputStableId, QStringLiteral("zz-Output"));
    QCOMPARE(decoded.document.records.at(0).lineageFingerprint, QByteArray());
    QCOMPARE(decoded.document.records.at(2).lineageFingerprint,
             QByteArray::fromHex(kHex64));
}

void AssignmentDocumentTests::decodesEmptyDocuments()
{
    // The schema default is an empty object: a valid, empty document.
    const AssignmentDocumentDecodeResult empty = decodeAssignmentDocument(QVariantMap{});
    QVERIFY(empty.ok);
    QVERIFY(empty.document.records.isEmpty());

    // Absence and null are not documents; they are reported, never guessed.
    const AssignmentDocumentDecodeResult absent = decodeAssignmentDocument(QVariant{});
    QVERIFY(!absent.ok);
    QCOMPARE(absent.reasonCode, QStringLiteral("absent-value"));
    const AssignmentDocumentDecodeResult nullValue =
        decodeAssignmentDocument(QVariant::fromValue(nullptr));
    QVERIFY(!nullValue.ok);
    QCOMPARE(nullValue.reasonCode, QStringLiteral("absent-value"));
}

void AssignmentDocumentTests::rejectsHostileDocuments()
{
    // Not an object at all.
    QCOMPARE(decodeAssignmentDocument(QVariant(QStringLiteral("garbage"))).reasonCode,
             QStringLiteral("not-an-object"));

    // Record is not an object.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"), QVariant(QString("x"))}}))
                 .reasonCode,
             QStringLiteral("record-not-an-object"));

    // Extra unknown field in a record.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"),
                  QVariantMap{{QStringLiteral("profile"), QStringLiteral("p")},
                              {QStringLiteral("lineage"), m_hex},
                              {QStringLiteral("extra"), true}}}}))
                 .reasonCode,
             QStringLiteral("record-field-set"));

    // Wrong field types.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"),
                  QVariantMap{{QStringLiteral("profile"), 5},
                              {QStringLiteral("lineage"), m_hex}}}}))
                 .reasonCode,
             QStringLiteral("record-field-type"));

    // Output key outside the stable-ID grammar.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("../escape"), recordValue(QStringLiteral("p"), QString())}}))
                 .reasonCode,
             QStringLiteral("invalid-output-id"));

    // Profile identifier outside the grammar.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"),
                  recordValue(QStringLiteral("has space"), QString())}}))
                 .reasonCode,
             QStringLiteral("invalid-profile-id"));

    // Lineage must be empty or exactly 64 lowercase hex characters.
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"), recordValue(QStringLiteral("p"), QStringLiteral("AB"))}}))
                 .reasonCode,
             QStringLiteral("invalid-lineage"));
    QCOMPARE(decodeAssignmentDocument(QVariant(QVariantMap{
                 {QStringLiteral("DP-1"),
                  recordValue(QStringLiteral("p"), m_hex.toUpper())}}))
                 .reasonCode,
             QStringLiteral("invalid-lineage"));

    // More outputs than the C0 aggregate cap rejects the whole document.
    QVariantMap flood;
    for (int i = 0; i < 33; ++i) {
        flood.insert(QStringLiteral("out-%1").arg(i, 2, 10, QLatin1Char('0')),
                     recordValue(QStringLiteral("p"), QString()));
    }
    QCOMPARE(decodeAssignmentDocument(QVariant(flood)).reasonCode,
             QStringLiteral("output-cap-exceeded"));
}

void AssignmentDocumentTests::encodesFailClosedOnInvalidRecords()
{
    AssignmentDocument bad;
    ColorAssignmentRecord record;
    record.outputStableId = QStringLiteral("DP-1");
    record.profileId = QStringLiteral("bad id");
    bad.records.append(record);
    QVERIFY(!encodeAssignmentDocument(bad).has_value());

    AssignmentDocument shortLineage;
    ColorAssignmentRecord digest;
    digest.outputStableId = QStringLiteral("DP-1");
    digest.profileId = QStringLiteral("p");
    digest.lineageFingerprint = QByteArray(16, '\1');
    shortLineage.records.append(digest);
    QVERIFY(!encodeAssignmentDocument(shortLineage).has_value());
}

void AssignmentDocumentTests::validatesDrafts()
{
    ColorAssignmentDraft duplicateTargets;
    duplicateTargets.entries.append(
        {QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(), false});
    duplicateTargets.entries.append(
        {QStringLiteral("DP-1"), QStringLiteral("q"), QByteArray(), false});
    QCOMPARE(validateColorAssignmentDraft(duplicateTargets).reasonCode.isEmpty(), false);

    ColorAssignmentDraft invalidProfile;
    invalidProfile.entries.append(
        {QStringLiteral("DP-1"), QStringLiteral("p/q"), QByteArray(), false});
    QCOMPARE(validateColorAssignmentDraft(invalidProfile).ok, false);

    ColorAssignmentDraft badDigest;
    badDigest.entries.append({QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(31, '\7'), false});
    QCOMPARE(validateColorAssignmentDraft(badDigest).reasonCode,
             QStringLiteral("invalid-lineage"));

    // Removes need no profile identity.
    ColorAssignmentDraft removal;
    removal.entries.append({QStringLiteral("DP-1"), QString(), QByteArray(), true});
    QCOMPARE(validateColorAssignmentDraft(removal).ok, true);
}

void AssignmentDocumentTests::appliesDraftsPurely()
{
    AssignmentDocument base;
    ColorAssignmentRecord existing;
    existing.outputStableId = QStringLiteral("DP-1");
    existing.profileId = QStringLiteral("old");
    base.records.append(existing);

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-2"), QStringLiteral("new"), QByteArray::fromHex(kHex64), false});
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("updated"), QByteArray(), false});
    draft.entries.append({QStringLiteral("Ghost"), QString(), QByteArray(), true});

    const ColorAssignmentApplyResult result = applyColorAssignmentDraft(base, draft);
    QVERIFY(result.ok);
    QCOMPARE(result.next.records.size(), 2);
    QCOMPARE(result.next.records.at(0).outputStableId, QStringLiteral("DP-1"));
    QCOMPARE(result.next.records.at(0).profileId, QStringLiteral("updated"));
    QCOMPARE(result.next.records.at(1).outputStableId, QStringLiteral("DP-2"));
    QCOMPARE(result.next.records.at(1).lineageFingerprint, QByteArray::fromHex(kHex64));

    // The base document is never mutated by an apply.
    QCOMPARE(base.records.size(), 1);
    QCOMPARE(base.records.first().profileId, QStringLiteral("old"));

    // An over-cap result is refused instead of truncated.
    ColorAssignmentDraft flood;
    for (int i = 0; i < 33; ++i) {
        flood.entries.append(
            {QStringLiteral("out-%1").arg(i, 2, 10, QLatin1Char('0')),
             QStringLiteral("p"), QByteArray(), false});
    }
    const ColorAssignmentApplyResult refused = applyColorAssignmentDraft(AssignmentDocument{}, flood);
    QVERIFY(!refused.ok);
    QCOMPARE(refused.reasonCode, QStringLiteral("output-cap-exceeded"));
}

void AssignmentDocumentTests::retainsAssignmentsForDisconnectedOutputs()
{
    AssignmentDocument base;
    base.records.append({QStringLiteral("connected"), QStringLiteral("old"), QByteArray()});
    base.records.append({QStringLiteral("disconnected"), QStringLiteral("kept"), QByteArray()});
    ColorAssignmentDraft draft;
    draft.entries.append(
        {QStringLiteral("connected"), QStringLiteral("updated"), QByteArray(), false});

    // AGENT-NOTE: P2.4 required the disconnected-output retention policy to
    // be explicit and pinned instead of emerging accidentally from merge code.
    const ColorAssignmentApplyResult result = applyColorAssignmentDraft(base, draft);
    QVERIFY(result.ok);
    QCOMPARE(result.next.records.size(), 2);
    QCOMPARE(result.next.records.at(0).profileId, QStringLiteral("updated"));
    QCOMPARE(result.next.records.at(1).outputStableId, QStringLiteral("disconnected"));
    QCOMPARE(result.next.records.at(1).profileId, QStringLiteral("kept"));
}

QTEST_MAIN(AssignmentDocumentTests)
#include "tst_assignment_document.moc"
