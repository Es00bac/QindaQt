// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0384: the producer repairs the transient window conditions ADR-0237
// measured per window instead of voiding the batch, and reports each repair.

#include "shellvisibilitysnapshot_fixture.h"

#include <QSet>
#include <QtTest>

using namespace QindaQt::Compositor;
using namespace QindaQt::Compositor::VisibilityFixtures;

class ShellVisibilityDegradationTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void excludesAStrandedWindowAndKeepsTheBatch();
    void republishesAWindowOutsideItsOutputOnTheOutputItOverlaps();
    void reassignsAWindowNamingAnOutputThatIsGone();
    void clearsActiveOnAHiddenOrMinimizedWindow();
    void publishesAMixedBatchWithOneDegradationPerCause();
};

// The live failure (2026-10-16): after an HDMI unplug one window stayed at
// 2560x1408+1536+32 while the only output was eDP-1 at 1536x864+0+0, and
// every sample was rejected -- auto-hide and the dock stopped for the session.
void ShellVisibilityDegradationTest::excludesAStrandedWindowAndKeepsTheBatch()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    ShellVisibilitySnapshotCandidate candidate{
        .scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")},
        .outputs = {{QStringLiteral("eDP-1"), QRect(0, 0, 1536, 864), 1.25}},
        .windows = {{.id = QStringLiteral("stranded"),
                     .outputId = QStringLiteral("eDP-1"),
                     .frameGeometry = QRect(1536, 32, 2560, 1408),
                     .workspaceIds = {QStringLiteral("workspace-1")},
                     .active = true},
                    {.id = QStringLiteral("visible"),
                     .outputId = QStringLiteral("eDP-1"),
                     .frameGeometry = QRect(10, 10, 800, 600),
                     .workspaceIds = {QStringLiteral("workspace-1")}}},
    };
    QString error;
    QVector<ShellVisibilityDegradation> degradations;
    QCOMPARE(store.publish(candidate, &error, &degradations),
             ShellVisibilityPublishResult::Published);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(store.available());
    QVERIFY(publishedWindow(store, QStringLiteral("stranded")).isEmpty());
    QVERIFY(!publishedWindow(store, QStringLiteral("visible")).isEmpty());
    QCOMPARE(degradations.size(), 1);
    QCOMPARE(degradations[0].windowId, QStringLiteral("stranded"));
    QCOMPARE(degradations[0].kind, ShellVisibilityDegradationKind::StrandedExcluded);
    QVERIFY2(degradations[0].message.contains(QStringLiteral("'stranded'")),
             qPrintable(degradations[0].message));
    QVERIFY2(degradations[0].message.contains(QStringLiteral("2560x1408+1536+32")),
             qPrintable(degradations[0].message));

    // Once it is brought back it is published again, at a new revision.
    candidate.windows[0].frameGeometry = QRect(0, 32, 1536, 832);
    QCOMPARE(store.publish(candidate, &error, &degradations),
             ShellVisibilityPublishResult::Published);
    QVERIFY(degradations.isEmpty());
    QCOMPARE(store.revision(), quint64(2));
    QVERIFY(publishedWindow(store, QStringLiteral("stranded"))
                .value(QStringLiteral("active")).toBool());
}

void ShellVisibilityDegradationTest::republishesAWindowOutsideItsOutputOnTheOutputItOverlaps()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    // window-a claims DP-1 but now lies on DP-2 (a drag the output assignment
    // has not caught up with).
    candidate.windows[1].frameGeometry = QRect(2100, 100, 600, 400);
    QVector<ShellVisibilityDegradation> degradations;
    QCOMPARE(store.publish(candidate, nullptr, &degradations),
             ShellVisibilityPublishResult::Published);
    QCOMPARE(publishedWindow(store, QStringLiteral("window-a"))
                 .value(QStringLiteral("outputId")).toString(),
             QStringLiteral("DP-2"));
    QCOMPARE(degradations.size(), 1);
    QCOMPARE(degradations[0].kind, ShellVisibilityDegradationKind::OutsideOutputReassigned);
    QVERIFY(degradations[0].message.contains(QStringLiteral("'DP-1'")));
    QVERIFY(degradations[0].message.contains(QStringLiteral("'DP-2'")));

    // A window straddling both outputs goes to the one it overlaps most.
    candidate.windows[1].outputId = QStringLiteral("HDMI-9");
    candidate.windows[1].frameGeometry = QRect(1800, 0, 400, 300);
    QCOMPARE(store.publish(candidate, nullptr, &degradations),
             ShellVisibilityPublishResult::Published);
    QCOMPARE(publishedWindow(store, QStringLiteral("window-a"))
                 .value(QStringLiteral("outputId")).toString(),
             QStringLiteral("DP-2"));
}

void ShellVisibilityDegradationTest::reassignsAWindowNamingAnOutputThatIsGone()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    candidate.windows[1].outputId = QStringLiteral("HDMI-9");
    QVector<ShellVisibilityDegradation> degradations;
    QCOMPARE(store.publish(candidate, nullptr, &degradations),
             ShellVisibilityPublishResult::Published);
    QCOMPARE(publishedWindow(store, QStringLiteral("window-a"))
                 .value(QStringLiteral("outputId")).toString(),
             QStringLiteral("DP-1"));
    QCOMPARE(degradations.size(), 1);
    QCOMPARE(degradations[0].kind, ShellVisibilityDegradationKind::UnknownOutputReassigned);
    QVERIFY(degradations[0].message.contains(QStringLiteral("'HDMI-9'")));

    // Gone output and no overlap anywhere: left out, not voided.
    candidate.windows[1].frameGeometry = QRect(9000, 9000, 10, 10);
    QCOMPARE(store.publish(candidate, nullptr, &degradations),
             ShellVisibilityPublishResult::Published);
    QVERIFY(publishedWindow(store, QStringLiteral("window-a")).isEmpty());
    QCOMPARE(degradations.size(), 1);
    QCOMPARE(degradations[0].kind, ShellVisibilityDegradationKind::StrandedExcluded);
}

void ShellVisibilityDegradationTest::clearsActiveOnAHiddenOrMinimizedWindow()
{
    for (const bool minimized : {true, false}) {
        ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
        auto candidate = validCandidate();
        candidate.windows[1].minimized = minimized;
        candidate.windows[1].hidden = !minimized;
        QVector<ShellVisibilityDegradation> degradations;
        QString error;
        QCOMPARE(store.publish(candidate, &error, &degradations),
                 ShellVisibilityPublishResult::Published);
        const auto window = publishedWindow(store, QStringLiteral("window-a"));
        QVERIFY(!window.isEmpty());
        QVERIFY(!window.value(QStringLiteral("active")).toBool());
        QCOMPARE(window.value(QStringLiteral("minimized")).toBool(), minimized);
        QCOMPARE(window.value(QStringLiteral("hidden")).toBool(), !minimized);
        QCOMPARE(degradations.size(), 1);
        QCOMPARE(degradations[0].kind,
                 ShellVisibilityDegradationKind::ActiveWhileHiddenCleared);
        QVERIFY(degradations[0].message.contains(
            minimized ? QStringLiteral("minimized") : QStringLiteral("hidden")));
    }

    // Clearing a hidden member's focus leaves room for the real active
    // window: two "active" windows are no longer a conflict when one of them
    // is hidden.
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    candidate.windows[0].active = true;
    candidate.windows[0].hidden = true;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QVERIFY(publishedWindow(store, QStringLiteral("window-a"))
                .value(QStringLiteral("active")).toBool());
    QVERIFY(!publishedWindow(store, QStringLiteral("window-b"))
                 .value(QStringLiteral("active")).toBool());
}

void ShellVisibilityDegradationTest::publishesAMixedBatchWithOneDegradationPerCause()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    candidate.windows.append({.id = QStringLiteral("stranded"),
                              .outputId = QStringLiteral("DP-1"),
                              .frameGeometry = QRect(-5000, -5000, 100, 100),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows.append({.id = QStringLiteral("moved"),
                              .outputId = QStringLiteral("DP-1"),
                              .frameGeometry = QRect(3000, 500, 200, 200),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows.append({.id = QStringLiteral("gone-output"),
                              .outputId = QStringLiteral("HDMI-A-1"),
                              .frameGeometry = QRect(100, 100, 200, 200),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows[1].minimized = true; // window-a: active while minimized
    QString error;
    QVector<ShellVisibilityDegradation> degradations;
    QCOMPARE(store.publish(candidate, &error, &degradations),
             ShellVisibilityPublishResult::Published);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(degradations.size(), 4);
    QSet<int> kinds;
    QSet<QString> messages;
    for (const auto &degradation : degradations) {
        kinds.insert(int(degradation.kind));
        messages.insert(degradation.message);
        QVERIFY2(degradation.message.contains(QStringLiteral("'%1'").arg(degradation.windowId)),
                 qPrintable(degradation.message));
    }
    QCOMPARE(kinds.size(), 4);
    QCOMPARE(messages.size(), 4);
    const auto windows = parse(store.snapshotJson()).value(QStringLiteral("windows")).toArray();
    QCOMPARE(windows.size(), 4); // window-a, window-b, moved, gone-output
    QCOMPARE(publishedWindow(store, QStringLiteral("moved"))
                 .value(QStringLiteral("outputId")).toString(),
             QStringLiteral("DP-2"));
    QCOMPARE(publishedWindow(store, QStringLiteral("gone-output"))
                 .value(QStringLiteral("outputId")).toString(),
             QStringLiteral("DP-1"));

    // The same degraded input is an unchanged generation, so a persistent
    // degradation cannot churn revisions; the report repeats for the caller
    // to deduplicate.
    QCOMPARE(store.publish(candidate, &error, &degradations),
             ShellVisibilityPublishResult::Unchanged);
    QCOMPARE(degradations.size(), 4);
    QCOMPARE(store.revision(), quint64(1));

    // A structural fault still voids the batch, and reports no degradations.
    candidate.windows.append(candidate.windows[0]);
    QCOMPARE(store.publish(candidate, &error, &degradations),
             ShellVisibilityPublishResult::Rejected);
    QVERIFY(degradations.isEmpty());
    QCOMPARE(store.revision(), quint64(1));
}

QTEST_GUILESS_MAIN(ShellVisibilityDegradationTest)
#include "tst_shellvisibilitydegradation.moc"
