// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

QByteArray rawRoot(const QJsonObject &root) {
  return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

class TaskListWireTests final : public QObject {
  Q_OBJECT

private slots:
  void decodesValidInventories();
  void rejectsMalformedWindowsPayload();
  void rejectsHostileWindowEntries();
  void rejectsOversizedWindowsPayload();
  void windowsInventoryRequiresTheSchemaTwoFence();
  void windowsInventoryBoundIsExact();
  void rejectsMalformedContainersPayload();
  void rejectsHostileContainerEntries();
  void rejectsMalformedScopePayload();
  void rejectsHostileScopeEntries();
  void scopeSnapshotRequiresCompleteLineage();
  void scopeSnapshotBoundIsExact();
};

void TaskListWireTests::decodesValidInventories() {
  const StandardScene scene = standardScene();

  const auto windows = TaskListWireDecoder::decodeWindows(scene.windows);
  QVERIFY2(windows.ok(), qPrintable(windows.message));
  QCOMPARE(windows.windows.size(), 3);
  QCOMPARE(windows.windows.at(0).windowId, QStringLiteral("w1"));
  QCOMPARE(windows.windows.at(1).containerId, QStringLiteral("c1"));
  QVERIFY(!windows.windows.at(1).skipTaskbar);
  QVERIFY(windows.windows.at(2).skipTaskbar);
  QVERIFY(windows.windows.at(2).minimized);
  // The schema-2 fence names the same generation the scope snapshot carries.
  QCOMPARE(windows.epoch, kScopeEpoch);
  QCOMPARE(windows.revision, quint64(1));
  QVERIFY(windows.generationAvailable);

  const auto containers = TaskListWireDecoder::decodeContainers(scene.containers);
  QVERIFY2(containers.ok(), qPrintable(containers.message));
  QCOMPARE(containers.containers.size(), 1);
  QCOMPARE(containers.containers.constFirst().containerId, QStringLiteral("c1"));
  QCOMPARE(containers.containers.constFirst().revision, quint64(7));
  QCOMPARE(containers.containers.constFirst().authority,
           TaskListContainerAuthority::HybridProcess);

  const auto scope = TaskListWireDecoder::decodeScopeSnapshot(scene.scope);
  QVERIFY2(scope.ok(), qPrintable(scope.message));
  QCOMPARE(scope.snapshot.epoch, kScopeEpoch);
  QCOMPARE(scope.snapshot.revision, quint64(1));
  QCOMPARE(scope.snapshot.scopes.size(), 3);
  QCOMPARE(scope.snapshot.scopes.at(0).outputId, QStringLiteral("output-1"));
  QCOMPARE(scope.snapshot.scopes.at(0).workspaceIds,
           QStringList{QStringLiteral("ws-1")});
}

void TaskListWireTests::rejectsMalformedWindowsPayload() {
  QCOMPARE(TaskListWireDecoder::decodeWindows("not json").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows("[1]").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows("{}").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"),
                         QStringLiteral("unavailable")}}))
               .error,
           TaskListWireError::Unavailable);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"), QStringLiteral("bogus")}}))
               .error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")}}))
               .error,
           TaskListWireError::UnsupportedSchema);
}

// AGENT-NOTE: Review finding P1-1/P2-1 (rejected candidate 3a5ae17): the
// schema-2 Windows() fence (epoch, revision, generationAvailable) names the
// retained ShellVisibilitySnapshot generation; the decoder must require it so
// the producer never joins scope truth without an exact lineage fence.
void TaskListWireTests::windowsInventoryRequiresTheSchemaTwoFence() {
  auto payload = [](const QJsonObject &extra,
                    std::initializer_list<const char *> remove = {}) {
    QJsonObject root{{QStringLiteral("status"), QStringLiteral("ok")},
                     {QStringLiteral("schemaVersion"), 2},
                     {QStringLiteral("epoch"), kScopeEpoch},
                     {QStringLiteral("revision"), QStringLiteral("1")},
                     {QStringLiteral("generationAvailable"), true},
                     {QStringLiteral("windows"), QJsonArray{}}};
    for (const char *key : remove) {
      root.remove(QLatin1StringView(key));
    }
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
      root.insert(it.key(), it.value());
    }
    return rawRoot(root);
  };
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({})).error,
           TaskListWireError::None);
  // Schema 1 (or none) predates the fence and is not task-list input.
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("schemaVersion"), 1}}))
               .error,
           TaskListWireError::UnsupportedSchema);
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({}, {"schemaVersion"}))
               .error,
           TaskListWireError::UnsupportedSchema);
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({}, {"epoch"}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("epoch"),
                         QStringLiteral("not-a-uuid")}}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({}, {"revision"}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"),
                         QStringLiteral("007")}}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({}, {"generationAvailable"}))
               .error,
           TaskListWireError::InvalidLineage);
  // An available generation can never carry the zero revision.
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"), QStringLiteral("0")}}))
               .error,
           TaskListWireError::InvalidLineage);
  // Before the first visibility generation the fence is legitimately zero.
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"), QStringLiteral("0")},
                        {QStringLiteral("generationAvailable"), false}}))
               .error,
           TaskListWireError::None);
}

// AGENT-NOTE: Review finding P2-1 (rejected candidate 3a5ae17): the 4,096 /
// 4,097 T1 Windows() boundary must be a registered row, not a scratch check.
void TaskListWireTests::windowsInventoryBoundIsExact() {
  const StandardScene atLimit = standaloneScene(4096);
  const auto decoded = TaskListWireDecoder::decodeWindows(atLimit.windows);
  QVERIFY2(decoded.ok(), qPrintable(decoded.message));
  QCOMPARE(decoded.windows.size(), 4096);

  const StandardScene overLimit = standaloneScene(4097);
  QCOMPARE(TaskListWireDecoder::decodeWindows(overLimit.windows).error,
           TaskListWireError::LimitExceeded);
}

void TaskListWireTests::rejectsHostileWindowEntries() {
  auto withWindow = [](QJsonObject window) {
    return windowsPayload({std::move(window)});
  };
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow({})).error,
           TaskListWireError::InvalidWindow);

  auto oversized = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  oversized.insert(QStringLiteral("id"), QString(513, QLatin1Char('x')));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(oversized)).error,
           TaskListWireError::InvalidWindow);

  auto controlTitle = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  controlTitle.insert(QStringLiteral("title"),
                      QStringLiteral("bad\ttitle"));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(controlTitle)).error,
           TaskListWireError::InvalidWindow);

  auto loneSurrogate = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  loneSurrogate.insert(QStringLiteral("applicationId"),
                       QStringLiteral("app") + QChar(0xD800));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(loneSurrogate)).error,
           TaskListWireError::InvalidWindow);

  auto emptyApplication = windowJson(QStringLiteral("w1"), QStringLiteral(""));
  QCOMPARE(
      TaskListWireDecoder::decodeWindows(withWindow(emptyApplication)).error,
      TaskListWireError::InvalidWindow);

  QCOMPARE(TaskListWireDecoder::decodeWindows(
               withWindow(windowJson(QStringLiteral("w1"), QStringLiteral("a"))))
               .error,
           TaskListWireError::None);
  QCOMPARE(TaskListWireDecoder::decodeWindows(windowsPayload(
                       {windowJson(QStringLiteral("w1"), QStringLiteral("a")),
                        windowJson(QStringLiteral("w1"), QStringLiteral("b"))}))
               .error,
           TaskListWireError::DuplicateWindowId);
}

void TaskListWireTests::rejectsOversizedWindowsPayload() {
  QByteArray payload = windowsPayload({});
  payload.append(QByteArray(4 * 1024 * 1024, ' '));
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload).error,
           TaskListWireError::PayloadTooLarge);
}

void TaskListWireTests::rejectsMalformedContainersPayload() {
  QCOMPARE(TaskListWireDecoder::decodeContainers("[]").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")}}))
               .error,
           TaskListWireError::MalformedPayload);
  QJsonArray entries;
  for (int index = 0; index < 2049; ++index) {
    entries.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("c%1").arg(index)},
        {QStringLiteral("revision"), QStringLiteral("1")},
        {QStringLiteral("authority"), QStringLiteral("hybrid-process")}});
  }
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("containers"), entries}}))
               .error,
           TaskListWireError::LimitExceeded);
}

void TaskListWireTests::rejectsHostileContainerEntries() {
  auto entry = [](const QString &id, const QString &revision,
                  const QString &authority) {
    return containersPayload({{id, 1, authority}}).replace(
        QByteArrayLiteral("\"1\""), revision.toUtf8());
  };
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 1,
                                   QStringLiteral("bridge")}}))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 0,
                                   QStringLiteral("hybrid-process")}}))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               entry(QStringLiteral("c1"), QStringLiteral("\"007\""),
                     QStringLiteral("hybrid-process")))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 1,
                                   QStringLiteral("control-bridge")},
                                  {QStringLiteral("c1"), 2,
                                   QStringLiteral("hybrid-process")}}))
               .error,
           TaskListWireError::DuplicateContainerId);
}

void TaskListWireTests::rejectsMalformedScopePayload() {
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot("{}").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"),
                         QStringLiteral("unavailable")}}))
               .error,
           TaskListWireError::Unavailable);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("schemaVersion"), 2},
                        {QStringLiteral("epoch"), QStringLiteral("e")},
                        {QStringLiteral("revision"), QStringLiteral("1")},
                        {QStringLiteral("windows"), QJsonArray{}}}))
               .error,
           TaskListWireError::UnsupportedSchema);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("epoch"), QStringLiteral("e")},
                        {QStringLiteral("revision"), QStringLiteral("0")},
                        {QStringLiteral("windows"), QJsonArray{}}}))
               .error,
           TaskListWireError::InvalidLineage);
}

void TaskListWireTests::rejectsHostileScopeEntries() {
  auto bothScopes = scopeEntryJson(QStringLiteral("w1"),
                                   QStringLiteral("output-1"),
                                   {QStringLiteral("ws-1")}, true);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload({bothScopes}))
               .error,
           TaskListWireError::InvalidScope);

  auto emptyOutput = scopeEntryJson(QStringLiteral("w1"), QStringLiteral(""),
                                    {QStringLiteral("ws-1")});
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload({emptyOutput}))
               .error,
           TaskListWireError::InvalidScope);

  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload(
                       {scopeEntryJson(QStringLiteral("w1"),
                                       QStringLiteral("output-1"),
                                       {QStringLiteral("ws-1")}),
                        scopeEntryJson(QStringLiteral("w1"),
                                       QStringLiteral("output-1"),
                                       {QStringLiteral("ws-2")})}))
               .error,
           TaskListWireError::DuplicateScopeId);
}

// AGENT-NOTE: Review finding P1-1 (rejected candidate 3a5ae17): the decoder
// previously ignored the required outputGeneration/scope/outputs fields,
// accepted a non-UUID epoch, and never validated window output membership, so
// foreign-lineage scope payloads published as current truth.
void TaskListWireTests::scopeSnapshotRequiresCompleteLineage() {
  auto mutate = [](const QByteArray &base, const QByteArray &key,
                   const QByteArray &replacement) {
    QByteArray copy = base;
    return copy.replace(key, replacement);
  };
  const QByteArray valid = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})});
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(valid).error,
           TaskListWireError::None);

  // The three required top-level fields the rejected candidate ignored.
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               mutate(valid, "\"outputGeneration\":\"1\",", ""))
               .error,
           TaskListWireError::InvalidLineage);
  QByteArray noScope = QJsonDocument(
                           QJsonObject{{QStringLiteral("status"),
                                        QStringLiteral("ok")},
                                       {QStringLiteral("schemaVersion"), 1},
                                       {QStringLiteral("epoch"), kScopeEpoch},
                                       {QStringLiteral("revision"),
                                        QStringLiteral("1")},
                                       {QStringLiteral("outputGeneration"),
                                        QStringLiteral("1")},
                                       {QStringLiteral("outputs"),
                                        QJsonArray{outputJson(
                                            QStringLiteral("output-1"))}},
                                       {QStringLiteral("windows"),
                                        QJsonArray{}}})
                           .toJson(QJsonDocument::Compact);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(noScope).error,
           TaskListWireError::InvalidScope);
  QByteArray noOutputs = QJsonDocument(
                             QJsonObject{{QStringLiteral("status"),
                                          QStringLiteral("ok")},
                                         {QStringLiteral("schemaVersion"), 1},
                                         {QStringLiteral("epoch"), kScopeEpoch},
                                         {QStringLiteral("revision"),
                                          QStringLiteral("1")},
                                         {QStringLiteral("outputGeneration"),
                                          QStringLiteral("1")},
                                         {QStringLiteral("scope"),
                                          QJsonObject{{QStringLiteral(
                                                           "workspaceId"),
                                                       QStringLiteral(
                                                           "workspace-1")}}},
                                         {QStringLiteral("windows"),
                                          QJsonArray{}}})
                             .toJson(QJsonDocument::Compact);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(noOutputs).error,
           TaskListWireError::InvalidScope);

  // A non-UUID epoch is not lineage.
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               mutate(valid, kScopeEpoch.toUtf8(), "e"))
               .error,
           TaskListWireError::InvalidLineage);

  // A window scoped to an output the snapshot does not declare rejects the
  // complete candidate.
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               scopePayload({scopeEntryJson(QStringLiteral("w1"),
                                            QStringLiteral("output-foreign"),
                                            {QStringLiteral("ws-1")})}))
               .error,
           TaskListWireError::InvalidScope);

  // Output entries must carry identity, geometry, and scale; duplicates and
  // hostile scales reject the whole snapshot.
  QByteArray duplicateOutputs = valid;
  duplicateOutputs.replace(
      QByteArrayLiteral("\"outputs\":["),
      QByteArrayLiteral("\"outputs\":[") +
          QJsonDocument(outputJson(QStringLiteral("output-1")))
              .toJson(QJsonDocument::Compact) +
          QByteArrayLiteral(","));
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(duplicateOutputs).error,
           TaskListWireError::InvalidScope);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               mutate(valid, "\"scale\":1", "\"scale\":17"))
               .error,
           TaskListWireError::InvalidScope);
  QByteArray missingGeometry = valid;
  missingGeometry.replace(
      QJsonDocument(outputJson(QStringLiteral("output-1")))
          .toJson(QJsonDocument::Compact),
      QByteArrayLiteral("{\"id\":\"output-1\",\"scale\":1}"));
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(missingGeometry).error,
           TaskListWireError::InvalidScope);
}

// AGENT-NOTE: Review finding P2-1 (rejected candidate 3a5ae17): the scope
// snapshot 4,096 / 4,097 boundary must be a registered row.
void TaskListWireTests::scopeSnapshotBoundIsExact() {
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               standaloneScene(4096).scope)
               .error,
           TaskListWireError::None);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               standaloneScene(4097).scope)
               .error,
           TaskListWireError::LimitExceeded);
}

QTEST_GUILESS_MAIN(TaskListWireTests)
#include "tst_task_list_wire.moc"
