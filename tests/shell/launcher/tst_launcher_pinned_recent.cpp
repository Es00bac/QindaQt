// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_pinned_recent.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

namespace {

PinnedApplications pinnedWith(const QVector<QString> &ids)
{
  PinnedApplications pinned;
  for (const QString &id : ids) {
    if (pinned.pin(id) != PinError::None)
      qFatal("pinnedWith received an invalid fixture identity");
  }
  return pinned;
}

} // namespace

class LauncherPinnedRecentTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void pinAppendUnpinAndDuplicateRules();
  void pinCeilingIsEnforced();
  void reorderingIsBoundedAndDeterministic();
  void reorderRejectsUnknownIds();
  void recentIsBoundedMostRecentlyUsed();
  void recentRecordIsIdempotentForKnownIds();
  void recentRejectsEmptyIdsAndClears();
  void hostileIdentityLengthsAreRejectedWithoutRetention();
  void pinnedAndRecentBoundsDifferByDesign();
};

void LauncherPinnedRecentTest::pinAppendUnpinAndDuplicateRules()
{
  auto pinned = pinnedWith({ QStringLiteral("app.a"), QStringLiteral("app.b") });
  QCOMPARE(pinned.ids(), (QVector<QString> { QStringLiteral("app.a"),
                                             QStringLiteral("app.b") }));
  QVERIFY(pinned.contains(QStringLiteral("app.a")));

  QCOMPARE(pinned.pin(QStringLiteral("app.a")), PinError::AlreadyPinned);
  QCOMPARE(pinned.pin(QString()), PinError::UnknownId);
  QCOMPARE(pinned.unpin(QStringLiteral("app.a")), PinError::None);
  QCOMPARE(pinned.unpin(QStringLiteral("app.a")), PinError::NotPinned);
  QCOMPARE(pinned.ids(), QVector<QString> { QStringLiteral("app.b") });
}

void LauncherPinnedRecentTest::pinCeilingIsEnforced()
{
  PinnedApplications pinned;
  for (int index = 0; index < Bounds::maxPinnedEntries; ++index)
    QCOMPARE(pinned.pin(QStringLiteral("app.%1").arg(index)), PinError::None);
  QVERIFY(pinned.isFull());
  QCOMPARE(pinned.pin(QStringLiteral("app.overflow")), PinError::LimitReached);
  QCOMPARE(pinned.ids().size(), Bounds::maxPinnedEntries);

  // Freeing one slot makes room again; capacity is a hard bound, not a queue.
  QCOMPARE(pinned.unpin(QStringLiteral("app.0")), PinError::None);
  QCOMPARE(pinned.pin(QStringLiteral("app.overflow")), PinError::None);
}

void LauncherPinnedRecentTest::reorderingIsBoundedAndDeterministic()
{
  auto pinned = pinnedWith({ QStringLiteral("app.a"), QStringLiteral("app.b"),
                             QStringLiteral("app.c") });

  QCOMPARE(pinned.moveUp(QStringLiteral("app.c")), PinError::None);
  QCOMPARE(pinned.ids(), (QVector<QString> { QStringLiteral("app.a"),
                                             QStringLiteral("app.c"),
                                             QStringLiteral("app.b") }));
  QCOMPARE(pinned.moveUp(QStringLiteral("app.c")), PinError::None);
  QCOMPARE(pinned.ids(), (QVector<QString> { QStringLiteral("app.c"),
                                             QStringLiteral("app.a"),
                                             QStringLiteral("app.b") }));
  // Moving past the top is a no-op, not a wrap-around.
  QCOMPARE(pinned.moveUp(QStringLiteral("app.c")), PinError::None);

  QCOMPARE(pinned.moveDown(QStringLiteral("app.a")), PinError::None);
  QCOMPARE(pinned.ids(), (QVector<QString> { QStringLiteral("app.c"),
                                             QStringLiteral("app.b"),
                                             QStringLiteral("app.a") }));
  // Moving past the bottom is a no-op too.
  QCOMPARE(pinned.moveDown(QStringLiteral("app.a")), PinError::None);
}

void LauncherPinnedRecentTest::reorderRejectsUnknownIds()
{
  auto pinned = pinnedWith({ QStringLiteral("app.a") });
  QCOMPARE(pinned.moveUp(QStringLiteral("app.z")), PinError::NotPinned);
  QCOMPARE(pinned.moveDown(QStringLiteral("app.z")), PinError::NotPinned);
}

void LauncherPinnedRecentTest::recentIsBoundedMostRecentlyUsed()
{
  RecentApplications recent;
  for (int index = 0; index < Bounds::maxRecentEntries; ++index)
    QCOMPARE(recent.record(QStringLiteral("app.%1").arg(index)), RecentError::None);
  QCOMPARE(recent.ids().size(), Bounds::maxRecentEntries);

  // Recording one more evicts the oldest (app.0), never refuses the record.
  QCOMPARE(recent.record(QStringLiteral("app.new")), RecentError::None);
  QCOMPARE(recent.ids().size(), Bounds::maxRecentEntries);
  QCOMPARE(recent.ids().first(), QStringLiteral("app.new"));
  QVERIFY(!recent.ids().contains(QStringLiteral("app.0")));
  QCOMPARE(recent.ids().last(), QStringLiteral("app.1"));
}

void LauncherPinnedRecentTest::recentRecordIsIdempotentForKnownIds()
{
  RecentApplications recent;
  QCOMPARE(recent.record(QStringLiteral("app.a")), RecentError::None);
  QCOMPARE(recent.record(QStringLiteral("app.b")), RecentError::None);
  QCOMPARE(recent.record(QStringLiteral("app.a")), RecentError::None);
  QCOMPARE(recent.ids(), (QVector<QString> { QStringLiteral("app.a"),
                                             QStringLiteral("app.b") }));
}

void LauncherPinnedRecentTest::recentRejectsEmptyIdsAndClears()
{
  RecentApplications recent;
  QCOMPARE(recent.record(QString()), RecentError::InvalidId);
  QCOMPARE(recent.record(QStringLiteral("app.a")), RecentError::None);
  recent.clear();
  QVERIFY(recent.ids().isEmpty());
}

void LauncherPinnedRecentTest::hostileIdentityLengthsAreRejectedWithoutRetention()
{
  const QString hostileId(Bounds::maxEntryIdLength + 1, QLatin1Char('x'));
  PinnedApplications pinned;
  RecentApplications recent;

  QCOMPARE(pinned.pin(hostileId), PinError::UnknownId);
  QCOMPARE(recent.record(hostileId), RecentError::InvalidId);
  QVERIFY(pinned.ids().isEmpty());
  QVERIFY(recent.ids().isEmpty());
}

void LauncherPinnedRecentTest::pinnedAndRecentBoundsDifferByDesign()
{
  // AGENT-GUARD context: the two collections have independent ceilings; a
  // regression that merges them would silently change eviction policy.
  QVERIFY(Bounds::maxPinnedEntries != Bounds::maxRecentEntries);
}

QTEST_GUILESS_MAIN(LauncherPinnedRecentTest)
#include "tst_launcher_pinned_recent.moc"
