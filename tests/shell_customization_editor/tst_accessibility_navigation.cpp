// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/shell_customization_editor/accessibility_identity.h"
#include "qindaqt/shell_customization_editor/keyboard_navigation.h"

#include <QtTest/QtTest>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

namespace {

DropTarget targetIn(const QString &panelId, const QString &zone)
{
    DropTarget target;
    target.panelId = panelId;
    target.zone = zone;
    return target;
}

} // namespace

class AccessibilityNavigationTest final : public QObject
{
    Q_OBJECT

private slots:
    void slotStepsStayInsideTheCurrentZone() const
    {
        const Profiles::LayoutProfile source = profile();

        DropTarget current = targetIn(QStringLiteral("bar"), QStringLiteral("start"));
        current.beforeAppletId = QStringLiteral("launcher-instance");

        const auto next = nextSlotInPanel(source, current);
        QVERIFY(next.has_value());
        // The end-zone clock is not a sibling of the start-zone launcher.
        QVERIFY(!next->beforeAppletId.has_value());
        QVERIFY(!nextSlotInPanel(source, *next).has_value());

        const auto back = previousSlotInPanel(source, *next);
        QVERIFY(back.has_value());
        QCOMPARE(back->beforeAppletId.value_or(QString()), QStringLiteral("launcher-instance"));

        DropTarget mismatched = targetIn(QStringLiteral("bar"), QStringLiteral("start"));
        mismatched.beforeAppletId = QStringLiteral("clock-instance");
        QVERIFY(!nextSlotInPanel(source, mismatched).has_value());
        QVERIFY(!previousSlotInPanel(source, mismatched).has_value());
    }

    void zoneStepsStayInsideTheSchemaV1Vocabulary() const
    {
        const Profiles::LayoutProfile source = profile();

        const auto toCenter = nextZoneInPanel(source, targetIn(QStringLiteral("bar"), QStringLiteral("start")));
        QVERIFY(toCenter.has_value());
        QCOMPARE(toCenter->zone, QStringLiteral("center"));

        const auto toEnd = nextZoneInPanel(source, *toCenter);
        QVERIFY(toEnd.has_value());
        QCOMPARE(toEnd->zone, QStringLiteral("end"));

        // "end" is the last offered zone; there is no fill or desktop step.
        QVERIFY(!nextZoneInPanel(source, *toEnd).has_value());

        const auto backToCenter = previousZoneInPanel(source, *toEnd);
        QCOMPARE(backToCenter->zone, QStringLiteral("center"));

        QVERIFY(!previousZoneInPanel(source, targetIn(QStringLiteral("bar"), QStringLiteral("start")))
                     .has_value());
    }

    void panelStepsAppendAtTheNeighborEnd() const
    {
        const Profiles::LayoutProfile source = profile();

        DropTarget current = targetIn(QStringLiteral("bar"), QStringLiteral("end"));
        current.beforeAppletId = QStringLiteral("clock-instance");

        const auto next = nextPanelTarget(source, current);
        QVERIFY(next.has_value());
        QCOMPARE(next->panelId, QStringLiteral("dock"));
        QCOMPARE(next->zone, QStringLiteral("end"));
        QVERIFY(!next->beforeAppletId.has_value());

        QVERIFY(!nextPanelTarget(source, *next).has_value());

        const auto back = previousPanelTarget(source, *next);
        QVERIFY(back.has_value());
        QCOMPARE(back->panelId, QStringLiteral("bar"));
    }

    void edgeStepsCycleThroughTheFourEdges() const
    {
        const Profiles::LayoutProfile source = profile();

        const auto stepOnce = steppedPanelEdge(source, QStringLiteral("bar"), 1);
        QCOMPARE(static_cast<int>(stepOnce.edge),
                 static_cast<int>(Profiles::Edge::Right));
        QCOMPARE(stepOnce.outputId, QStringLiteral("*"));
        QCOMPARE(static_cast<int>(stepOnce.alignment),
                 static_cast<int>(Profiles::Alignment::Fill));

        const auto stepBack = steppedPanelEdge(source, QStringLiteral("bar"), -1);
        QCOMPARE(static_cast<int>(stepBack.edge),
                 static_cast<int>(Profiles::Edge::Left));
    }

    void moveDescriptionsFollowTheParityWording() const
    {
        const Profiles::LayoutProfile source = profile();

        const DropTarget target{QStringLiteral("bar"), QStringLiteral("end"),
                                QStringLiteral("clock-instance")};
        const QString accepted =
            describeMove(source, QStringLiteral("clock"), target, true);
        QCOMPARE(accepted,
                 QStringLiteral("Move clock to Top panel, end zone, position 1 of 2 — accepted"));

        const QString rejected = describeMove(source, QStringLiteral("clock"), target, false,
                                              QStringLiteral("applet does not support vertical placement"));
        QCOMPARE(rejected,
                 QStringLiteral("Move clock to Top panel, end zone, position 1 of 2 — rejected: "
                                "applet does not support vertical placement"));
    }

    void announcementsCoalesceToOneLatestTuple() const
    {
        AnnouncementCenter center;
        QVERIFY(!center.hasPending());

        center.announce({AnnouncementKind::Polite, QStringLiteral("first")});
        center.announce({AnnouncementKind::Polite, QStringLiteral("second")});
        center.announce({AnnouncementKind::Assertive, QStringLiteral("rejected")});
        QVERIFY(center.hasPending());

        const auto drained = center.drain();
        QCOMPARE(drained.size(), 1);
        QCOMPARE(drained.first().kind, AnnouncementKind::Assertive);
        QCOMPARE(drained.first().message, QStringLiteral("rejected"));
        QVERIFY(!center.hasPending());
        QVERIFY(center.drain().isEmpty());

        center.announce({AnnouncementKind::Assertive, QStringLiteral("old rejection")});
        center.announce({AnnouncementKind::Polite, QStringLiteral("latest acceptance")});
        const auto latest = center.drain();
        QCOMPARE(latest.size(), 1);
        QCOMPARE(latest.first().kind, AnnouncementKind::Polite);
        QCOMPARE(latest.first().message, QStringLiteral("latest acceptance"));
    }

    void rejectionAnnouncementsAreAssertiveAndAcceptancePolite() const
    {
        const Profiles::LayoutProfile source = profile();
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"), {}};

        const auto accepted = moveAnnouncement(source, QStringLiteral("clock"), target, true);
        QCOMPARE(accepted.kind, AnnouncementKind::Polite);
        QVERIFY(accepted.message.endsWith(QStringLiteral("— accepted")));

        const auto rejected =
            moveAnnouncement(source, QStringLiteral("clock"), target, false, QStringLiteral("no"));
        QCOMPARE(rejected.kind, AnnouncementKind::Assertive);
        QVERIFY(rejected.message.contains(QStringLiteral("— rejected: no")));
    }

    void panelNamesCarryTheOutputOnlyWhenConcrete() const
    {
        Profiles::PanelSpec wildcard = profile().panels.first();
        QCOMPARE(panelDisplayName(wildcard), QStringLiteral("Top panel"));

        wildcard.output = QStringLiteral("primary");
        QCOMPARE(panelDisplayName(wildcard), QStringLiteral("Top panel on primary"));
    }
};

QTEST_MAIN(AccessibilityNavigationTest)
#include "tst_accessibility_navigation.moc"
