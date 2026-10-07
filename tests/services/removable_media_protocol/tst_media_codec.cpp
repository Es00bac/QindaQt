// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_protocol_test_support.h"
#include <QtTest>
#include <limits>
using namespace MediaTest;
class MediaCodecTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void canonicalLittleEndianVectors() {
    const auto empty = encodeSnapshot(Snapshot{});
    QVERIFY(empty.succeeded());
    QCOMPARE(empty.payload, QByteArray::fromHex(
        "51524d44" "01000000" "01000000" // magic, codec, message kind
        "01000000" "00000000" "00000000" "0000000000000000" // protocol, lineage
        "00000000" "00000000" "00" "00000000" "00000000"));
    ActionRequest v; v.requestId = QStringLiteral("r");
    v.lineage = {QStringLiteral(":1.2"), QStringLiteral("e"), 1};
    v.attachment = {QStringLiteral("h"), 1};
    const auto encoded = encodeActionRequest(v); QVERIFY(encoded.succeeded());
    QCOMPARE(encoded.payload, QByteArray::fromHex(
        "51524d440100000002000000010000000100000072040000003a312e32"
        "010000006501000000000000000100000068010000000000000004000000"));
    Snapshot decoded;
    QVERIFY(decodeSnapshot(empty.payload, decoded).succeeded());
    QVERIFY(decoded == Snapshot{});
  }
  void completeOwningSnapshotRoundTrip() {
    auto v = snapshot(); auto &mounted = v.rows[0];
    mounted.mountState = MountState::Mounted;
    mounted.mountRoots = {QStringLiteral("/run/media/test/USB"), QStringLiteral("/mnt/USB")};
    mounted.preferredRoot = mounted.mountRoots.first(); mounted.readOnly = ReadOnlyState::ReadOnly;
    mounted.actions.open = {true, DisabledReason::None};
    mounted.progress = ProgressPhase::Refreshing; mounted.outcome = OperationStatus::Applied;
    mounted.diagnostic.message = QString::fromUtf8("Ready — café");
    v.rows[1].encrypted = v.rows[1].locked = true;
    v.pending = PendingOperation{QStringLiteral("operation-1"), request(), ProgressPhase::Mounting};
    auto encoded = encodeSnapshot(v); QVERIFY(encoded.succeeded());
    Snapshot decoded; QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded());
    QVERIFY(decoded == v); QCOMPARE(encodeSnapshot(decoded).payload, encoded.payload);
    encoded.payload.fill('x');
    QVERIFY(decoded == v); // no borrowed text/root aliases survive decoding
  }
  void unicodeBomAndReplacementCharacterAreLiteralData() {
    auto v = snapshot();
    const QString text = QString(QChar(0xfeff)) + QString(QChar(0xfffd)) +
        QString::fromUtf8("café\n");
    v.rows[0].displayName = text; v.diagnostic.message = text;
    const auto encoded = encodeSnapshot(v); QVERIFY(encoded.succeeded());
    Snapshot decoded; QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded());
    QVERIFY(decoded == v); QCOMPARE(encodeSnapshot(decoded).payload, encoded.payload);
  }
  void allActionsAdmissionAndTerminalStatusesRoundTrip() {
    for (quint32 action = 0; action <= static_cast<quint32>(Action::ShowDetails); ++action) {
      auto v = request(); v.action = static_cast<Action>(action);
      const auto encoded = encodeActionRequest(v); QVERIFY(encoded.succeeded());
      ActionRequest decoded; QVERIFY(decodeActionRequest(encoded.payload, decoded).succeeded());
      QVERIFY(decoded == v); QCOMPARE(encodeActionRequest(decoded).payload, encoded.payload);
    }
    for (quint32 status = 0; status <= static_cast<quint32>(AdmissionStatus::Invalid); ++status) {
      auto v = admission(); v.status = static_cast<AdmissionStatus>(status);
      if (v.status != AdmissionStatus::Accepted) v.operationId.clear();
      const auto encoded = encodeActionAdmission(v); QVERIFY(encoded.succeeded());
      ActionAdmission decoded; QVERIFY(decodeActionAdmission(encoded.payload, decoded).succeeded());
      QVERIFY(decoded == v); QCOMPARE(encodeActionAdmission(decoded).payload, encoded.payload);
    }
    for (quint32 status = 1; status <= static_cast<quint32>(OperationStatus::Uncertain); ++status) {
      auto v = result(); v.status = static_cast<OperationStatus>(status);
      if (v.status != OperationStatus::Applied) v.confirmingRevision.reset();
      const auto encoded = encodeOperationResult(v); QVERIFY(encoded.succeeded());
      OperationResult decoded; QVERIFY(decodeOperationResult(encoded.payload, decoded).succeeded());
      QVERIFY(decoded == v); QCOMPARE(encodeOperationResult(decoded).payload, encoded.payload);
    }
    for (quint32 mode = 1; mode <= static_cast<quint32>(RemovalMode::PoweredOff); ++mode) {
      auto v = result(); v.request.action = Action::Remove; v.confirmingRevision.reset();
      v.removalMode = static_cast<RemovalMode>(mode);
      const auto encoded = encodeOperationResult(v); QVERIFY(encoded.succeeded());
      OperationResult decoded; QVERIFY(decodeOperationResult(encoded.payload, decoded).succeeded());
      QVERIFY(decoded == v);
    }
    auto details = result(); details.request.action = Action::ShowDetails;
    details.confirmingRevision.reset(); QVERIFY(encodeOperationResult(details).succeeded());
  }
  void allScalarAndCollectionBoundaries() {
    auto v = snapshot(); v.rows.clear();
    v.lineage.owner = QStringLiteral(":a.") + QString(kMaxOwnerBytes - 3, QLatin1Char('a'));
    v.lineage.epoch = QString(kMaxIdentifierBytes, QLatin1Char('e'));
    v.lineage.revision = std::numeric_limits<quint64>::max();
    for (qsizetype i = 0; i < kMaxVolumes; ++i) {
      auto volume = row(QString::number(i));
      volume.driveDisplayId = QStringLiteral("drive-") + QString::number(i % kMaxDrives);
      volume.displayName = QString(kMaxLabelUtf8Bytes / 2, QChar(0x00e9));
      volume.kind = QString(kMaxLabelUtf8Bytes, QLatin1Char('k'));
      volume.sizeBytes = std::numeric_limits<quint64>::max();
      volume.partitionNumber = std::numeric_limits<quint32>::max();
      volume.attachment.generation = std::numeric_limits<quint64>::max();
      v.rows.append(volume);
    }
    v.rows[0].volumeDisplayId = QString(kMaxIdentifierBytes, QLatin1Char('v'));
    v.rows[0].attachment.handle = QString(kMaxIdentifierBytes, QLatin1Char('h'));
    auto &mounted = v.rows[0]; mounted.mountState = MountState::Mounted;
    for (qsizetype i = 0; i < kMaxMountRoots; ++i)
      mounted.mountRoots.append(QStringLiteral("/") + QString(kMaxMountRootUtf8Bytes - 2, QLatin1Char('a')) + QString::number(i));
    mounted.preferredRoot = mounted.mountRoots.first();
    v.diagnostic.message = QString(kMaxMessageUtf8Bytes, QLatin1Char('m'));
    const auto encoded = encodeSnapshot(v); QVERIFY(encoded.succeeded());
    Snapshot decoded; QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded()); QVERIFY(decoded == v);
    auto action = request(); action.requestId = QString(kMaxIdentifierBytes, QLatin1Char('r'));
    action.lineage = v.lineage; action.attachment = mounted.attachment;
    const auto bytes = encodeActionRequest(action); QVERIFY(bytes.succeeded());
    ActionRequest decodedAction; QVERIFY(decodeActionRequest(bytes.payload, decodedAction).succeeded());
    QVERIFY(decodedAction == action);
  }
  void aggregatePayloadExactlyAtLimitAndOneByteOver() {
    auto v = snapshot(); v.rows.clear();
    for (qsizetype i = 0; i < kMaxVolumes; ++i) {
      auto volume = row(QString::number(i));
      volume.mountState = MountState::Mounted;
      volume.mountRoots = {QStringLiteral("/m") + QString::number(i)};
      volume.preferredRoot = volume.mountRoots.first(); v.rows.append(volume);
    }
    auto baseline = encodeSnapshot(v); QVERIFY(baseline.succeeded());
    qsizetype remaining = kMaxSnapshotBytes - baseline.payload.size();
    if (remaining % 2 != 0) { v.diagnostic.message = QStringLiteral("x"); --remaining; }
    for (auto &volume : v.rows) {
      const qsizetype extra = std::min(remaining / 2, kMaxMountRootUtf8Bytes - volume.preferredRoot.size());
      volume.preferredRoot += QString(extra, QLatin1Char('x'));
      volume.mountRoots[0] = volume.preferredRoot; remaining -= 2 * extra;
    }
    QCOMPARE(remaining, qsizetype(0));
    const auto encoded = encodeSnapshot(v); QVERIFY(encoded.succeeded());
    QCOMPARE(encoded.payload.size(), kMaxSnapshotBytes);
    Snapshot decoded; QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded()); QVERIFY(decoded == v);
    v.diagnostic.message += QLatin1Char('x'); QVERIFY(validateSnapshot(v).accepted());
    const auto rejected = encodeSnapshot(v); QCOMPARE(rejected.error, CodecError::PayloadTooLarge);
    QVERIFY(rejected.payload.isEmpty());
  }
  void nonReadyAndRetiredPendingHandleRemainRepresentable() {
    Snapshot v; v.availability = Availability::Unavailable;
    v.diagnostic = {DiagnosticCode::Unavailable, QStringLiteral("Media support unavailable")};
    const auto encoded = encodeSnapshot(v); QVERIFY(encoded.succeeded());
    Snapshot decoded; QVERIFY(decodeSnapshot(encoded.payload, decoded).succeeded()); QVERIFY(decoded == v);
    v = snapshot(); v.rows.clear();
    auto pending = request(); pending.action = Action::Remove;
    v.pending = PendingOperation{QStringLiteral("operation-1"), pending, ProgressPhase::PoweringOff};
    QVERIFY(encodeSnapshot(v).succeeded());
  }
};
QTEST_GUILESS_MAIN(MediaCodecTest)
#include "tst_media_codec.moc"
