// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>

#include "support/display_client_test_support.h"
#include "support/fake_display_transport.h"

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <utility>

using namespace QindaQt;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

namespace {

using Display::ErrorCode;
using Display::OperationStatus;

const QString kOwner = QStringLiteral(":1.70");
const QString kEpoch = QStringLiteral("test-epoch");
const QString kFirst = QStringLiteral("edid:test");
const QString kSecond = QStringLiteral("edid:second");

Display::Snapshot twoOutputs(const quint64 revision) {
  Display::Snapshot value = testSnapshot(kEpoch, revision);
  Display::Output second = value.outputs.constFirst();
  second.stableId = kSecond;
  second.connectorName = QStringLiteral("HDMI-A-1");
  second.runtimeCompositorUuid = QStringLiteral("runtime-second");
  second.primary = false;
  second.priority = 2;
  second.position = QPoint(1920, 0);
  value.outputs.append(second);
  return value;
}

Display::BrightnessSnapshot joined(const Display::Snapshot &topology,
                                   const quint64 revision,
                                   const quint32 firstValue = 6'000) {
  Display::BrightnessSnapshot value{.protocolVersion = 1,
                                    .serviceEpoch = topology.serviceEpoch,
                                    .topologyRevision = topology.revision,
                                    .revision = revision,
                                    .outputs = {},
                                    .wireValid = true};
  for (const Display::Output &output : topology.outputs) {
    value.outputs.append({.stableId = output.stableId,
                          .capable = true,
                          .observed = true,
                          .value = 3'000});
  }
  value.outputs.first().value = firstValue;
  return value;
}

Display::BrightnessRequest requestFor(const Display::BrightnessSnapshot &published,
                                      const quint32 value,
                                      const QString &stableId = kFirst) {
  return {.baseEpoch = published.serviceEpoch,
          .baseRevision = published.revision,
          .stableId = stableId,
          .value = value};
}

Display::OperationResult immediate(const OperationStatus status,
                                   const quint64 initiating,
                                   const quint64 observed,
                                   const QString &diagnostic = {},
                                   const ErrorCode error = ErrorCode::None) {
  return {.kind = Display::OperationKind::ImmediatePolicy,
          .status = status,
          .error = error,
          .initiatingEpoch = kEpoch,
          .initiatingRevision = initiating,
          .observedRevision = observed,
          .transactionId = {},
          .diagnostic = diagnostic,
          .wireValid = true};
}

void ready(Client &client, FakeDisplayTransport &transport,
           const Display::Snapshot &topology) {
  client.start();
  transport.publishOwner(kOwner);
  transport.replySnapshot(transport.fetches.constLast(), topology);
}

// Changed may repeat a topology revision. The client re-reads the complete
// snapshot, then asks for brightness again.
void rereadAt(FakeDisplayTransport &transport, const Display::Snapshot &topology) {
  transport.publishInvalidation(kOwner, topology.serviceEpoch, topology.revision);
  transport.replySnapshot(transport.fetches.constLast(), topology);
}

Display::OperationResult resultFor(const QSignalSpy &spy, const quint64 requestId) {
  for (qsizetype index = 0; index < spy.size(); ++index) {
    if (spy.at(index).at(0).toULongLong() == requestId) {
      return qvariant_cast<Display::OperationResult>(spy.at(index).at(1));
    }
  }
  return {};
}

} // namespace

class DisplayClientBrightnessTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void publishesOnlyBrightnessJoinedToTheHeldSnapshot();
  void brightnessOnlyRepublicationIsMonotonic();
  void lineageLossWithdrawsBrightness();
  void localRefusalsNeverReachTheTransport();
  void delayedReplyCompletesOnceWithoutReplay();
};

void DisplayClientBrightnessTest::publishesOnlyBrightnessJoinedToTheHeldSnapshot() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy changed(&client, &Client::brightnessChanged);
  ready(client, transport, twoOutputs(1));
  QCOMPARE(client.state(), ClientState::Ready);
  QCOMPARE(transport.brightnessFetches.size(), 1);
  QCOMPARE(transport.brightnessFetches.constFirst().owner, kOwner);
  QVERIFY(!client.brightness().has_value());

  // Rows for a newer topology revision do not join the held snapshot. The
  // client reads that snapshot before it asks for brightness again.
  const qsizetype snapshotFetches = transport.fetches.size();
  transport.replyBrightness(transport.brightnessFetches.constLast(),
                            joined(twoOutputs(2), 3));
  QVERIFY(!client.brightness().has_value());
  QCOMPARE(transport.fetches.size(), snapshotFetches + 1);
  transport.replySnapshot(transport.fetches.constLast(), twoOutputs(2));
  QCOMPARE(transport.brightnessFetches.size(), 2);

  Display::BrightnessSnapshot foreignEpoch = joined(twoOutputs(2), 3);
  foreignEpoch.serviceEpoch = QStringLiteral("other-epoch");
  Display::BrightnessSnapshot reordered = joined(twoOutputs(2), 3);
  std::swap(reordered.outputs[0], reordered.outputs[1]);
  Display::BrightnessSnapshot inconsistent = joined(twoOutputs(2), 3);
  inconsistent.outputs[1].observed = false;
  for (const Display::BrightnessSnapshot &refused :
       {foreignEpoch, reordered, inconsistent}) {
    transport.replyBrightness(transport.brightnessFetches.constLast(), refused);
    QVERIFY(!client.brightness().has_value());
    rereadAt(transport, twoOutputs(2));
  }
  QCOMPARE(changed.size(), 0);

  const Display::BrightnessSnapshot accepted = joined(twoOutputs(2), 3);
  transport.replyBrightness(transport.brightnessFetches.constLast(), accepted);
  QCOMPARE(changed.size(), 1);
  QVERIFY(client.brightness() == accepted);
  QCOMPARE(client.state(), ClientState::Ready);
}

void DisplayClientBrightnessTest::brightnessOnlyRepublicationIsMonotonic() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy changed(&client, &Client::brightnessChanged);
  QSignalSpy snapshots(&client, &Client::snapshotChanged);
  const Display::Snapshot topology = twoOutputs(1);
  ready(client, transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 5));
  QCOMPARE(changed.size(), 1);

  // A brightness-only republication repeats the topology revision.
  rereadAt(transport, topology);
  QCOMPARE(snapshots.size(), 1);
  transport.replyBrightness(transport.brightnessFetches.constLast(),
                            joined(topology, 6, 2'500));
  QCOMPARE(changed.size(), 2);
  QCOMPARE(client.brightness()->revision, quint64{6});
  QCOMPARE(client.brightness()->outputs.constFirst().value, quint32{2'500});

  // Older, same-revision hybrid, and duplicate replies never replace truth.
  for (const Display::BrightnessSnapshot &refused :
       {joined(topology, 5, 9'000), joined(topology, 6, 9'000),
        joined(topology, 6, 2'500)}) {
    rereadAt(transport, topology);
    transport.replyBrightness(transport.brightnessFetches.constLast(), refused);
    QCOMPARE(changed.size(), 2);
    QVERIFY(client.brightness() == joined(topology, 6, 2'500));
  }

  // A new topology revision withdraws rows that no longer join before the
  // rows for that revision are read.
  transport.publishInvalidation(kOwner, kEpoch, 2);
  transport.replySnapshot(transport.fetches.constLast(), twoOutputs(2));
  QVERIFY(!client.brightness().has_value());
  QCOMPARE(changed.size(), 3);
  transport.replyBrightness(transport.brightnessFetches.constLast(),
                            joined(twoOutputs(2), 7, 2'500));
  QCOMPARE(client.brightness()->revision, quint64{7});
  QCOMPARE(changed.size(), 4);
}

void DisplayClientBrightnessTest::lineageLossWithdrawsBrightness() {
  FakeDisplayTransport transport;
  Client client(&transport);
  const Display::Snapshot topology = twoOutputs(1);
  ready(client, transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 5));
  QVERIFY(client.brightness().has_value());

  // A service without brightness keeps topology Ready but publishes no rows.
  rereadAt(transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), {}, false,
                            QStringLiteral("service-unavailable"));
  QVERIFY(!client.brightness().has_value());
  QCOMPARE(client.state(), ClientState::Ready);
  QVERIFY(client.hasSnapshot());

  rereadAt(transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 6));
  QVERIFY(client.brightness().has_value());
  transport.publishInvalidation(kOwner, kEpoch, 1, false);
  QVERIFY(!client.brightness().has_value());
  QCOMPARE(client.state(), ClientState::Unavailable);

  // After owner replacement the prior owner's in-flight read cannot publish.
  rereadAt(transport, topology);
  const FakeDisplayTransport::Fetch prior = transport.brightnessFetches.constLast();
  transport.publishOwner(QStringLiteral(":1.71"));
  transport.replySnapshot(transport.fetches.constLast(), topology);
  transport.replyBrightness(prior, joined(topology, 7));
  QVERIFY(!client.brightness().has_value());
  QCOMPARE(transport.brightnessFetches.constLast().owner, QStringLiteral(":1.71"));
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 1));
  QVERIFY(client.brightness().has_value());

  client.stop();
  QVERIFY(!client.brightness().has_value());
}

void DisplayClientBrightnessTest::localRefusalsNeverReachTheTransport() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy completions(&client, &Client::operationCompleted);
  const Display::Snapshot topology = twoOutputs(1);
  const Display::BrightnessSnapshot published = joined(topology, 5);
  struct Refusal {
    quint64 requestId = 0;
    const char *diagnostic = "";
    ErrorCode error = ErrorCode::None;
  };
  QList<Refusal> refusals;

  refusals.append({client.setOutputBrightness(requestFor(published, 2'500)),
                   "client-not-running", ErrorCode::InvalidTransition});
  ready(client, transport, topology);
  refusals.append({client.setOutputBrightness(requestFor(published, 2'500)),
                   "no-snapshot", ErrorCode::InvalidTransition});
  transport.replyBrightness(transport.brightnessFetches.constLast(), published);

  Display::BrightnessSnapshot older = published;
  older.revision = 4;
  Display::BrightnessSnapshot otherEpoch = published;
  otherEpoch.serviceEpoch = QStringLiteral("other-epoch");
  refusals.append({client.setOutputBrightness(requestFor(older, 2'500)),
                   "stale-revision", ErrorCode::StaleRevision});
  refusals.append({client.setOutputBrightness(requestFor(otherEpoch, 2'500)),
                   "stale-revision", ErrorCode::StaleRevision});
  refusals.append({client.setOutputBrightness(requestFor(
                       published, 2'500, QStringLiteral("edid:unplugged"))),
                   "unknown-output", ErrorCode::InvalidCandidate});
  refusals.append({client.setOutputBrightness(requestFor(published, 10'001)),
                   "invalid-brightness-request", ErrorCode::InvalidCandidate});
  QCOMPARE(transport.brightnessSubmissions.size(), 0);

  QVERIFY(client.setOutputBrightness(requestFor(published, 2'500)) != 0);
  QCOMPARE(transport.brightnessSubmissions.size(), 1);
  refusals.append({client.setOutputBrightness(requestFor(published, 1'000, kSecond)),
                   "operation-pending", ErrorCode::TransactionActive});

  QTRY_COMPARE(completions.size(), refusals.size());
  for (const Refusal &refusal : std::as_const(refusals)) {
    const Display::OperationResult result = resultFor(completions, refusal.requestId);
    QCOMPARE(result.kind, Display::OperationKind::ImmediatePolicy);
    QCOMPARE(result.status, OperationStatus::Rejected);
    QCOMPARE(result.diagnostic, QString::fromLatin1(refusal.diagnostic));
    QCOMPARE(result.error, refusal.error);
  }
  QCOMPARE(transport.brightnessSubmissions.size(), 1);
  QCOMPARE(client.state(), ClientState::Busy);
}

void DisplayClientBrightnessTest::delayedReplyCompletesOnceWithoutReplay() {
  FakeDisplayTransport transport;
  Client client(&transport);
  QSignalSpy completions(&client, &Client::operationCompleted);
  const Display::Snapshot topology = twoOutputs(1);
  ready(client, transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 5));

  const quint64 first = client.setOutputBrightness(requestFor(*client.brightness(), 2'500));
  QCOMPARE(client.state(), ClientState::Busy);
  QCOMPARE(transport.brightnessSubmissions.size(), 1);
  QCOMPARE(transport.brightnessSubmissions.constFirst().owner, kOwner);
  QCOMPARE(transport.brightnessSubmissions.constFirst().requestId, first);
  QVERIFY(transport.brightnessSubmissions.constFirst().request
          == requestFor(joined(topology, 5), 2'500));
  QTest::qWait(20);
  QCOMPARE(completions.size(), 0);

  // Display1 publishes the observed value before its delayed reply; a
  // complete re-read still follows the reply.
  rereadAt(transport, topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(),
                            joined(topology, 6, 2'500));
  const qsizetype fetchesBeforeReply = transport.fetches.size();
  transport.replyOperationAs(kOwner, first, immediate(OperationStatus::Succeeded, 5, 6));
  QTRY_COMPARE(completions.size(), 1);
  QVERIFY(resultFor(completions, first) == immediate(OperationStatus::Succeeded, 5, 6));
  QCOMPARE(client.state(), ClientState::Ready);
  QCOMPARE(transport.fetches.size(), fetchesBeforeReply + 1);
  transport.replySnapshot(transport.fetches.constLast(), topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(),
                            joined(topology, 6, 2'500));
  const Display::BrightnessSnapshot current = *client.brightness();

  // A success naming another initiating revision is not this request's.
  const quint64 mismatched = client.setOutputBrightness(requestFor(current, 4'000));
  transport.replyOperationAs(kOwner, mismatched,
                             immediate(OperationStatus::Succeeded, 5, 7));
  QTRY_COMPARE(completions.size(), 2);
  QCOMPARE(resultFor(completions, mismatched).status, OperationStatus::Uncertain);
  QCOMPARE(resultFor(completions, mismatched).diagnostic,
           QStringLiteral("lineage-mismatch"));

  // A typed service refusal passes through unchanged.
  const quint64 busy = client.setOutputBrightness(requestFor(current, 4'000));
  const Display::OperationResult busyResult =
      immediate(OperationStatus::Busy, 6, 6, QStringLiteral("compositor-busy"),
                ErrorCode::TransactionActive);
  transport.replyOperationAs(kOwner, busy, busyResult);
  QTRY_COMPARE(completions.size(), 3);
  QVERIFY(resultFor(completions, busy) == busyResult);

  // Owner loss while the delayed reply is outstanding completes once as
  // Uncertain; the lost owner's late reply is dropped and nothing is resent.
  const quint64 lost = client.setOutputBrightness(requestFor(current, 4'000));
  QCOMPARE(transport.brightnessSubmissions.size(), 4);
  transport.publishOwner(QStringLiteral(":1.71"));
  QTRY_COMPARE(completions.size(), 4);
  QCOMPARE(resultFor(completions, lost).status, OperationStatus::Uncertain);
  QCOMPARE(resultFor(completions, lost).diagnostic, QStringLiteral("owner-changed"));
  QVERIFY(!client.brightness().has_value());
  transport.replyOperationAs(kOwner, lost, immediate(OperationStatus::Succeeded, 6, 7));
  QTest::qWait(20);
  QCOMPARE(completions.size(), 4);

  // A transport timeout is final and uncertain as well.
  transport.replySnapshot(transport.fetches.constLast(), topology);
  transport.replyBrightness(transport.brightnessFetches.constLast(), joined(topology, 1));
  client.setRequestTimeout(30);
  const quint64 timedOut =
      client.setOutputBrightness(requestFor(*client.brightness(), 1'000));
  QTRY_COMPARE(completions.size(), 5);
  QCOMPARE(resultFor(completions, timedOut).status, OperationStatus::Uncertain);
  QCOMPARE(resultFor(completions, timedOut).diagnostic,
           QStringLiteral("transport-timeout"));
  QCOMPARE(transport.brightnessSubmissions.size(), 5);
}

QTEST_GUILESS_MAIN(DisplayClientBrightnessTest)
#include "tst_display_client_brightness.moc"
