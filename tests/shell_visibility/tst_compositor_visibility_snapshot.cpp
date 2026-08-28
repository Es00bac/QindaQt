// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>
#include <utility>

using namespace QindaQt::ShellVisibility;

namespace {

QJsonObject rectangle(int x, int y, int width, int height) {
  return {{QStringLiteral("x"), x},
          {QStringLiteral("y"), y},
          {QStringLiteral("width"), width},
          {QStringLiteral("height"), height}};
}

QJsonObject output(QString id = QStringLiteral("eDP-1"),
                   QJsonObject geometry = rectangle(0, 0, 1920, 1080),
                   double scale = 1.0) {
  return {{QStringLiteral("id"), std::move(id)},
          {QStringLiteral("geometry"), std::move(geometry)},
          {QStringLiteral("scale"), scale}};
}

QJsonObject window(QString id = QStringLiteral("window"),
                   QString outputId = QStringLiteral("eDP-1")) {
  return {{QStringLiteral("id"), std::move(id)},
          {QStringLiteral("outputId"), std::move(outputId)},
          {QStringLiteral("frameGeometry"), rectangle(0, 0, 800, 600)},
          {QStringLiteral("workspaceIds"),
           QJsonArray{QStringLiteral("workspace-1")}},
          {QStringLiteral("onAllWorkspaces"), false},
          {QStringLiteral("activityIds"),
           QJsonArray{QStringLiteral("activity-a")}},
          {QStringLiteral("active"), true},
          {QStringLiteral("maximized"), false},
          {QStringLiteral("minimized"), false},
          {QStringLiteral("hidden"), false}};
}

QJsonObject root() {
  return {{QStringLiteral("status"), QStringLiteral("ok")},
          {QStringLiteral("schemaVersion"), 1},
          {QStringLiteral("epoch"), QStringLiteral("epoch-a")},
          {QStringLiteral("revision"), QStringLiteral("7")},
          {QStringLiteral("outputGeneration"), QStringLiteral("4")},
          {QStringLiteral("scope"),
           QJsonObject{{QStringLiteral("workspaceId"),
                        QStringLiteral("workspace-1")},
                       {QStringLiteral("activityId"),
                        QStringLiteral("activity-a")}}},
          {QStringLiteral("outputs"), QJsonArray{output()}},
          {QStringLiteral("windows"), QJsonArray{window()}}};
}

QByteArray encode(const QJsonObject &object) {
  return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

class CompositorVisibilitySnapshotTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void decodesACompleteCoherentSnapshot();
  void preservesMultipleWorkspacesNegativeGeometryAndFractionalScale();
  void rejectsOversizedAndMalformedDocuments();
  void rejectsStatusSchemaAndRevisionFailures();
  void rejectsMalformedScopeAndOutputFields();
  void rejectsMalformedWindowFieldsAndInconsistentInventory();
  void enforcesCollectionAndIdentifierLimitsBeforePublication();
};

void CompositorVisibilitySnapshotTest::decodesACompleteCoherentSnapshot() {
  const auto result = CompositorVisibilitySnapshotDecoder::decode(encode(root()));

  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.snapshot->epoch, QStringLiteral("epoch-a"));
  QCOMPARE(result.snapshot->revision, quint64(7));
  QCOMPARE(result.snapshot->outputGeneration, quint64(4));
  QCOMPARE(result.snapshot->outputs.size(), 1);
  QCOMPARE(result.snapshot->outputs[0].id, QStringLiteral("eDP-1"));
  QCOMPARE(result.snapshot->outputs[0].geometry, QRect(0, 0, 1920, 1080));
  QCOMPARE(result.snapshot->outputs[0].scale, 1.0);
  QCOMPARE(result.snapshot->windows.size(), 1);
  QCOMPARE(result.snapshot->windows[0].workspaceIds,
           QStringList{QStringLiteral("workspace-1")});
  QVERIFY(result.snapshot->windows[0].active);
  QCOMPARE(result.snapshot->scope.workspaceId, QStringLiteral("workspace-1"));
}

void CompositorVisibilitySnapshotTest::
    preservesMultipleWorkspacesNegativeGeometryAndFractionalScale() {
  QJsonObject document = root();
  document[QStringLiteral("revision")] =
      QString::number(std::numeric_limits<quint64>::max());
  document[QStringLiteral("outputs")] =
      QJsonArray{output(QStringLiteral("DP-1"),
                        rectangle(-2560, -100, 2560, 1440), 1.25)};
  QJsonObject candidate = window(QStringLiteral("multi"), QStringLiteral("DP-1"));
  candidate[QStringLiteral("frameGeometry")] = rectangle(-1800, -50, 900, 700);
  candidate[QStringLiteral("workspaceIds")] =
      QJsonArray{QStringLiteral("workspace-2"),
                 QStringLiteral("workspace-1")};
  candidate[QStringLiteral("activityIds")] = QJsonArray{};
  document[QStringLiteral("windows")] = QJsonArray{candidate};

  const auto result = CompositorVisibilitySnapshotDecoder::decode(encode(document));

  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.snapshot->revision, std::numeric_limits<quint64>::max());
  QCOMPARE(result.snapshot->outputs[0].geometry,
           QRect(-2560, -100, 2560, 1440));
  QCOMPARE(result.snapshot->outputs[0].scale, 1.25);
  QCOMPARE(result.snapshot->windows[0].workspaceIds.size(), 2);
  QVERIFY(result.snapshot->windows[0].activityIds.isEmpty());
}

void CompositorVisibilitySnapshotTest::rejectsOversizedAndMalformedDocuments() {
  QByteArray oversized(CompositorVisibilitySnapshotDecoder::MaxPayloadBytes + 1,
                       'x');
  auto result = CompositorVisibilitySnapshotDecoder::decode(oversized);
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::PayloadTooLarge);
  QVERIFY(!result.snapshot.has_value());

  result = CompositorVisibilitySnapshotDecoder::decode(QByteArrayLiteral("{"));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidJson);

  result = CompositorVisibilitySnapshotDecoder::decode(QByteArrayLiteral("[]"));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidRoot);
}

void CompositorVisibilitySnapshotTest::rejectsStatusSchemaAndRevisionFailures() {
  QJsonObject document = root();
  document[QStringLiteral("status")] = QStringLiteral("rejected");
  auto result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidRoot);

  document = root();
  document[QStringLiteral("schemaVersion")] = 2;
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::UnsupportedSchema);

  const QJsonArray badEpochs = {
      QJsonValue(), QJsonValue(QString{}), QJsonValue(QStringLiteral(" padded ")),
      QJsonValue(QString(
          CompositorVisibilitySnapshotDecoder::MaxIdentifierCharacters + 1,
          QLatin1Char('e')))};
  for (const QJsonValue &badEpoch : badEpochs) {
    document = root();
    document[QStringLiteral("epoch")] = badEpoch;
    result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
    QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidEpoch);
    QVERIFY(!result.snapshot.has_value());
  }

  const QJsonArray badRevisions = {
      QJsonValue(7), QJsonValue(QStringLiteral("0")),
      QJsonValue(QStringLiteral("07")),
      QJsonValue(QStringLiteral("-1")),
      QJsonValue(QStringLiteral("18446744073709551616"))};
  for (const QJsonValue &badRevision : badRevisions) {
    document = root();
    document[QStringLiteral("revision")] = badRevision;
    result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
    QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidRevision);
  }
  for (const QJsonValue &badGeneration : badRevisions) {
    document = root();
    document[QStringLiteral("outputGeneration")] = badGeneration;
    result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
    QCOMPARE(result.error.code,
             CompositorSnapshotErrorCode::InvalidOutputGeneration);
  }
}

void CompositorVisibilitySnapshotTest::rejectsMalformedScopeAndOutputFields() {
  QJsonObject document = root();
  document[QStringLiteral("scope")] = QJsonObject{};
  auto result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);

  document = root();
  document[QStringLiteral("outputs")] =
      QJsonArray{output(QStringLiteral("eDP-1"), rectangle(0, 0, 0, 1080))};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);

  document = root();
  document[QStringLiteral("outputs")] =
      QJsonArray{output(QStringLiteral("eDP-1"), rectangle(0, 0, 1920, 1080), 0.0)};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);

  document = root();
  document[QStringLiteral("outputs")] = QJsonArray{output(), output()};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidInventory);
}

void CompositorVisibilitySnapshotTest::
    rejectsMalformedWindowFieldsAndInconsistentInventory() {
  QJsonObject document = root();
  QJsonObject candidate = window();
  candidate[QStringLiteral("active")] = QStringLiteral("true");
  document[QStringLiteral("windows")] = QJsonArray{candidate};
  auto result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);

  document = root();
  candidate = window();
  candidate[QStringLiteral("workspaceIds")] = QJsonArray{};
  document[QStringLiteral("windows")] = QJsonArray{candidate};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidInventory);

  document = root();
  candidate = window(QStringLiteral("window"), QStringLiteral("missing"));
  document[QStringLiteral("windows")] = QJsonArray{candidate};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidInventory);

  document = root();
  candidate = window();
  candidate[QStringLiteral("minimized")] = true;
  document[QStringLiteral("windows")] = QJsonArray{candidate};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidInventory);
}

void CompositorVisibilitySnapshotTest::
    enforcesCollectionAndIdentifierLimitsBeforePublication() {
  QJsonObject document = root();
  QJsonArray outputs;
  for (qsizetype index = 0;
       index <= CompositorVisibilitySnapshotDecoder::MaxOutputs; ++index) {
    outputs.append(output(QStringLiteral("output-%1").arg(index)));
  }
  document[QStringLiteral("outputs")] = outputs;
  auto result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code,
           CompositorSnapshotErrorCode::CollectionLimitExceeded);

  document = root();
  document[QStringLiteral("outputs")] =
      QJsonArray{output(QString(
          CompositorVisibilitySnapshotDecoder::MaxIdentifierCharacters + 1,
          QLatin1Char('x')))};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);

  document = root();
  QJsonObject candidate = window();
  QJsonArray memberships;
  for (qsizetype index = 0;
       index <= CompositorVisibilitySnapshotDecoder::MaxScopeMemberships;
       ++index) {
    memberships.append(QStringLiteral("workspace-%1").arg(index));
  }
  candidate[QStringLiteral("workspaceIds")] = memberships;
  document[QStringLiteral("windows")] = QJsonArray{candidate};
  result = CompositorVisibilitySnapshotDecoder::decode(encode(document));
  QCOMPARE(result.error.code, CompositorSnapshotErrorCode::InvalidField);
}

QTEST_GUILESS_MAIN(CompositorVisibilitySnapshotTest)
#include "tst_compositor_visibility_snapshot.moc"
