// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_protocol_test_support.h"
#include <QtTest>
using namespace MediaTest;
class MediaValidationTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void identityAndProviderLineageAreStructuralOnly() {
    auto v = snapshot(); QVERIFY(validateSnapshot(v).accepted()); // same drive, two partitions
    v.rows[1].volumeDisplayId = v.rows[0].volumeDisplayId;
    QCOMPARE(validateSnapshot(v).error, ValueError::DuplicateIdentity);
    v = snapshot(); v.rows[1].attachment.handle = v.rows[0].attachment.handle;
    QCOMPARE(validateSnapshot(v).error, ValueError::DuplicateIdentity);
    v = snapshot(); v.lineage.owner = QStringLiteral("org.qindaqt.RemovableMedia1");
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidOwner);
    for (const auto &owner : {QStringLiteral(":1..2"), QStringLiteral(":1"), QStringLiteral(":1.2/3"), QStringLiteral(":1.2.")}) {
      v = snapshot(); v.lineage.owner = owner;
      QCOMPARE(validateSnapshot(v).error, ValueError::InvalidOwner);
    }
    v = snapshot(); v.lineage.revision = 0;
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidLineage);
    v = snapshot(); v.rows[0].attachment.generation = 0;
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidAttachment);
    for (const auto &id : {QStringLiteral("/dev/sdz1"), QStringLiteral("has space"), QString::fromUtf8("café"), QString(kMaxIdentifierBytes + 1, 'a')}) {
      v = snapshot(); v.rows[0].volumeDisplayId = id;
      QCOMPARE(validateSnapshot(v).error, ValueError::InvalidIdentifier);
    }
    v = snapshot(); v.availability = Availability::Unavailable;
    QCOMPARE(validateSnapshot(v).error, ValueError::InconsistentValue);
    v.rows.clear(); v.lineage = {}; QVERIFY(validateSnapshot(v).accepted());
  }
  void mountRootsAreBoundedLiteralNormalizedPaths_data() {
    QTest::addColumn<QString>("root"); QTest::addColumn<int>("error");
    QTest::newRow("relative") << QStringLiteral("media/USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("uri") << QStringLiteral("file:///media/USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("dotdot") << QStringLiteral("/media/../USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("dot") << QStringLiteral("/media/./USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("repeated-separator") << QStringLiteral("/media//USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("network-prefix") << QStringLiteral("//media/USB") << int(ValueError::InvalidMountRoot);
    QTest::newRow("trailing-separator") << QStringLiteral("/media/USB/") << int(ValueError::InvalidMountRoot);
    QTest::newRow("nul") << (QStringLiteral("/media/") + QChar::Null + QStringLiteral("USB")) << int(ValueError::InvalidText);
    QTest::newRow("oversized") << (QStringLiteral("/") + QString(kMaxMountRootUtf8Bytes, 'a')) << int(ValueError::LimitExceeded);
    QTest::newRow("multibyte-bound") << (QStringLiteral("/") + QString(kMaxMountRootUtf8Bytes / 2, QChar(0x00e9))) << int(ValueError::LimitExceeded);
  }
  void mountRootsAreBoundedLiteralNormalizedPaths() {
    QFETCH(QString, root); QFETCH(int, error);
    auto v = snapshot(); auto &volume = v.rows[0]; volume.mountState = MountState::Mounted;
    volume.mountRoots = {root}; volume.preferredRoot = root;
    QCOMPARE(int(validateSnapshot(v).error), error);
    const auto encoded = encodeSnapshot(v); QCOMPARE(encoded.error, CodecError::InvalidValue);
    QVERIFY(encoded.payload.isEmpty());
  }
  void contradictoryRootAndActionFactsRejectWithoutAdmissionPolicy() {
    auto v = snapshot(); v.rows[0].mountRoots = {QStringLiteral("/media/USB")};
    QCOMPARE(validateSnapshot(v).error, ValueError::InconsistentValue);
    v.rows[0].mountState = MountState::Mounted;
    v.rows[0].preferredRoot = QStringLiteral("/other");
    QCOMPARE(validateSnapshot(v).error, ValueError::InconsistentValue);
    v.rows[0].preferredRoot = v.rows[0].mountRoots.first();
    v.rows[0].mountRoots.append(v.rows[0].preferredRoot);
    QCOMPARE(validateSnapshot(v).error, ValueError::DuplicateIdentity);
    v = snapshot(); v.rows[0].actions.open.enabled = true;
    QCOMPARE(validateSnapshot(v).error, ValueError::InconsistentValue);
    v = snapshot(); v.rows[0].locked = true;
    QCOMPARE(validateSnapshot(v).error, ValueError::InconsistentValue);
    v = snapshot(); v.rows[0].actions.mount = {true, DisabledReason::None};
    // Structurally valid availability is only the provider's report; no action
    // is actually dispatched or admitted by a successful validation/codec.
    QVERIFY(validateSnapshot(v).accepted());
  }
  void pendingInitiatingLineageSurvivesItsRemovedRow() {
    auto v = snapshot(); v.rows.clear();
    auto pending = request(); pending.action = Action::Remove;
    v.pending = PendingOperation{QStringLiteral("op"), pending, ProgressPhase::Ejecting};
    QVERIFY(validateSnapshot(v).accepted());
    v.pending->request.lineage.epoch = QStringLiteral("other");
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidLineage);
    v.pending->request.lineage = v.lineage; ++v.pending->request.lineage.revision;
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidLineage);
    v.pending->request.lineage = v.lineage; v.pending->phase = ProgressPhase::Idle;
    QCOMPARE(validateSnapshot(v).error, ValueError::InvalidEnum);
  }
  void admissionAndAuthoritativeResultShapes() {
    auto a = admission(); a.operationId.clear();
    QCOMPARE(validateActionAdmission(a).error, ValueError::InvalidIdentifier);
    a = admission(); a.status = AdmissionStatus::Busy;
    QCOMPARE(validateActionAdmission(a).error, ValueError::InconsistentValue);
    auto r = result(); r.confirmingRevision.reset();
    QCOMPARE(validateOperationResult(r).error, ValueError::InconsistentValue);
    r = result(); r.confirmingRevision = r.request.lineage.revision;
    QCOMPARE(validateOperationResult(r).error, ValueError::InconsistentValue);
    r = result(); r.request.action = Action::Remove; r.confirmingRevision.reset();
    QCOMPARE(validateOperationResult(r).error, ValueError::InconsistentValue);
    r.removalMode = RemovalMode::Unmounted; QVERIFY(validateOperationResult(r).accepted());
    r.status = OperationStatus::Uncertain;
    QCOMPARE(validateOperationResult(r).error, ValueError::InconsistentValue);
    r.removalMode = RemovalMode::None; QVERIFY(validateOperationResult(r).accepted());
    r.confirmingRevision = 5;
    QCOMPARE(validateOperationResult(r).error, ValueError::InconsistentValue);
  }
  void invalidSourceUtf16AndOversizedCollectionsRefuseEncoding() {
    for (const auto surrogate : {ushort(0xd800), ushort(0xdc00)}) {
      auto v = snapshot(); v.rows[0].displayName = QString(QChar(surrogate));
      QCOMPARE(validateSnapshot(v).error, ValueError::InvalidText);
      const auto encoded = encodeSnapshot(v); QCOMPARE(encoded.error, CodecError::InvalidValue);
      QVERIFY(encoded.payload.isEmpty());
    }
    auto v = snapshot(); v.rows[0].displayName = QString(kMaxLabelUtf8Bytes / 2 + 1, QChar(0x00e9));
    QCOMPARE(validateSnapshot(v).error, ValueError::LimitExceeded);
    v = snapshot(); v.rows[0].diagnostic.message = QString(kMaxMessageUtf8Bytes + 1, 'a');
    QCOMPARE(validateSnapshot(v).error, ValueError::LimitExceeded);
    v = snapshot(); v.rows.clear();
    for (qsizetype i = 0; i <= kMaxVolumes; ++i) v.rows.append(row(QString::number(i)));
    QCOMPARE(validateSnapshot(v).error, ValueError::LimitExceeded);
    const auto encoded = encodeSnapshot(v); QVERIFY(!encoded.succeeded()); QVERIFY(encoded.payload.isEmpty());
    v = snapshot(); auto &mounted = v.rows[0]; mounted.mountState = MountState::Mounted;
    for (qsizetype i = 0; i <= kMaxMountRoots; ++i) mounted.mountRoots.append(QStringLiteral("/m") + QString::number(i));
    mounted.preferredRoot = mounted.mountRoots.first();
    QCOMPARE(validateSnapshot(v).error, ValueError::LimitExceeded);
  }
};
QTEST_GUILESS_MAIN(MediaValidationTest)
#include "tst_media_validation.moc"
