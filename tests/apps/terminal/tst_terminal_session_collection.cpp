// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"

#include <QHash>
#include <QSet>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QWidget>
#include <QtTest>

#include <csignal>
#include <memory>

using namespace QindaQt::Apps::Terminal;

namespace {

struct LifecycleStats final {
  QHash<ProcessId, QString> directories;
  int created = 0;
  int shutdowns = 0;
  QSet<ProcessId> stopped;
  QList<QPair<ProcessId, int>> sentSignals;
};

class CollectionMonitor final : public ProcessMonitor {
public:
  explicit CollectionMonitor(QSharedPointer<LifecycleStats> stats)
      : m_stats(std::move(stats)) {}

  QString workingDirectory(ProcessId pid) override { return m_stats->directories.value(pid); }

  ProcessExitInfo reap(ProcessId pid) override {
    return m_stats->stopped.contains(pid)
               ? ProcessExitInfo{ProcessState::Exited, false, 0, true}
               : ProcessExitInfo{ProcessState::Running, false, 0, true};
  }

  ProcessGroupState processGroupState(ProcessId pid) override {
    return m_stats->stopped.contains(pid) ? ProcessGroupState::Empty
                                          : ProcessGroupState::NonEmpty;
  }

  bool signalProcessGroup(ProcessId pid, int signalNumber) override {
    m_stats->sentSignals.append({pid, signalNumber});
    return true;
  }

private:
  QSharedPointer<LifecycleStats> m_stats;
};

class CollectionBackend final : public TerminalSessionBackend {
public:
  CollectionBackend(ProcessId pid, QSharedPointer<LifecycleStats> stats)
      : m_pid(pid), m_stats(std::move(stats)) {}

  ~CollectionBackend() override { delete m_widget; }

  StartOutcome start(const TerminalLaunchRequest &) override {
    m_widget = new QWidget;
    return {.ok = true, .diagnostic = {}};
  }

  void requestShutdown() override {
    ++m_stats->shutdowns;
    m_stats->stopped.insert(m_pid);
    delete m_widget;
    m_widget = nullptr;
  }

  ProcessId shellProcessId() const override { return m_pid; }
  QWidget *terminalWidget() override { return m_widget; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}

private:
  ProcessId m_pid = 0;
  QSharedPointer<LifecycleStats> m_stats;
  QWidget *m_widget = nullptr;
};

struct CollectionHarness final {
  QSharedPointer<LifecycleStats> stats =
      QSharedPointer<LifecycleStats>::create();
  CollectionMonitor monitor{stats};
  TerminalSessionContext context{
      {},
      QStringLiteral("/bin/true"),
      {},
      {},
  };

  std::unique_ptr<TerminalSessionCollection> makeCollection() {
    const auto captured = stats;
    TerminalSession::BackendFactory factory =
        [captured](const TerminalProfile &) {
          const ProcessId pid = 7000 + captured->created++;
          return std::make_unique<CollectionBackend>(pid, captured);
        };
    return std::make_unique<TerminalSessionCollection>(
        context, std::move(factory), &monitor, TeardownBounds{2, 2, 2, 1});
  }
};

} // namespace

class TerminalSessionCollectionTest final : public QObject {
  Q_OBJECT

private slots:
  void newTabInheritsOnlyOwnedLiveDirectory();
  void listIsBoundedAndMovable();
  void closeOneAndCloseAllDisposeEveryOwnedChild();
  void destructionForcesEveryRemainingChildDown();
  void titlesAreBoundedAndSanitized();
};

void TerminalSessionCollectionTest::newTabInheritsOnlyOwnedLiveDirectory() {
  CollectionHarness harness;
  harness.context.workingDirectory = QStringLiteral("/tmp");
  auto collection = harness.makeCollection();
  auto *first = collection->addSession(builtinDefaultProfile()).session;
  QVERIFY(first);
  harness.stats->directories.insert(7000, QStringLiteral("/usr"));
  auto *second = collection->addSession(builtinDefaultProfile(), first).session;
  QVERIFY(second);
  QCOMPARE(second->workingDirectory(), QStringLiteral("/usr"));
  harness.stats->directories.remove(7000);
  auto *fallback = collection->addSession(builtinDefaultProfile(), first).session;
  QVERIFY(fallback);
  QCOMPARE(fallback->workingDirectory(), QStringLiteral("/tmp"));
  auto other = harness.makeCollection();
  harness.stats->directories.insert(7000, QStringLiteral("/usr"));
  auto *foreign = other->addSession(builtinDefaultProfile(), first).session;
  QVERIFY(foreign);
  QCOMPARE(foreign->workingDirectory(), QStringLiteral("/tmp"));
}

void TerminalSessionCollectionTest::listIsBoundedAndMovable() {
  CollectionHarness harness;
  auto collection = harness.makeCollection();
  QSignalSpy rejected(collection.get(),
                      &TerminalSessionCollection::sessionAddRejected);
  for (int index = 0; index < TerminalSessionCollection::kMaxSessions;
       ++index) {
    QVERIFY(collection->addSession(builtinDefaultProfile()).session != nullptr);
  }
  QCOMPARE(collection->count(), TerminalSessionCollection::kMaxSessions);
  QVERIFY(collection->addSession(builtinDefaultProfile()).session == nullptr);
  QCOMPARE(rejected.count(), 1);

  TerminalSession *last = collection->sessionAt(collection->count() - 1);
  collection->moveSession(last, 0);
  QCOMPARE(collection->sessionAt(0), last);
}

void TerminalSessionCollectionTest::
    closeOneAndCloseAllDisposeEveryOwnedChild() {
  CollectionHarness harness;
  auto collection = harness.makeCollection();
  TerminalSession *first =
      collection->addSession(builtinDefaultProfile()).session;
  QVERIFY(first != nullptr);
  QVERIFY(collection->addSession(builtinDefaultProfile()).session != nullptr);
  QVERIFY(collection->addSession(builtinDefaultProfile()).session != nullptr);
  QSignalSpy removed(collection.get(),
                     &TerminalSessionCollection::sessionRemoved);

  collection->requestCloseSession(first);
  QTRY_COMPARE(collection->count(), 2);
  QCOMPARE(removed.count(), 1);

  QSignalSpy closed(collection.get(),
                    &TerminalSessionCollection::allSessionsClosed);
  collection->requestCloseAll();
  QTRY_COMPARE(collection->count(), 0);
  QTRY_COMPARE(closed.count(), 1);
  QCOMPARE(closed.first().at(0).toBool(), true);
  QCOMPARE(harness.stats->shutdowns, 3);
}

void TerminalSessionCollectionTest::destructionForcesEveryRemainingChildDown() {
  CollectionHarness harness;
  {
    auto collection = harness.makeCollection();
    for (int index = 0; index < 4; ++index) {
      QVERIFY(collection->addSession(builtinDefaultProfile()).session !=
              nullptr);
    }
    QCOMPARE(harness.stats->shutdowns, 0);
  }
  QCOMPARE(harness.stats->shutdowns, 4);
  QCOMPARE(harness.stats->stopped.size(), 4);
  QVERIFY(harness.stats->sentSignals.isEmpty());
}

void TerminalSessionCollectionTest::titlesAreBoundedAndSanitized() {
  QCOMPARE(sanitizeSessionTitle(QStringLiteral("  one\n\t two\rthree  ")),
           QStringLiteral("one two three"));
  QCOMPARE(sanitizeSessionTitle(QStringLiteral("safe\u202Eevil")),
           QStringLiteral("safeevil"));
  QCOMPARE(sanitizeSessionTitle(QString(QChar(0xD800)) + QStringLiteral("ok")),
           QStringLiteral("ok"));
  const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
  const QString bounded = sanitizeSessionTitle(
      QString(kMaxSessionTitleLength - 1, QLatin1Char('a')) + emoji);
  QCOMPARE(bounded.size(), kMaxSessionTitleLength - 1);
  QVERIFY(!bounded.endsWith(QChar(0xD83D)));
}

QTEST_MAIN(TerminalSessionCollectionTest)
#include "tst_terminal_session_collection.moc"
