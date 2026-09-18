// SPDX-License-Identifier: GPL-3.0-or-later
#include "touchedgeactions.h"

#include <QAction>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

class RecordingReserver final : public TouchEdgeReserver {
public:
    void reserve(TouchEdge edge, QAction *action) override { log.append({QStringLiteral("reserve"), touchEdgeName(edge), action}); }
    void unreserve(TouchEdge edge, QAction *action) override { log.append({QStringLiteral("unreserve"), touchEdgeName(edge), action}); }
    struct Entry {
        QString verb;
        QString edge;
        QAction *action;
    };
    QList<Entry> log;
};

} // namespace

class TouchEdgeActionsTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAndSettingsDecoding();
    void unknownActionsBecomeNone();
    void preferencesClampLongPress();
    void gesturesReserveOnlyEdgesWithActions();
    void triggeredActionsAnnounceEdgeAndAction();
};

void TouchEdgeActionsTests::defaultsAndSettingsDecoding()
{
    const TouchEdgeActions defaults;
    QCOMPARE(defaults.left, QStringLiteral("overview"));
    QCOMPARE(defaults.top, QStringLiteral("notifications"));
    QCOMPARE(defaults.right, QStringLiteral("none"));
    QCOMPARE(defaults.bottom, QStringLiteral("task-switcher"));
    QCOMPARE(TouchEdgeActions::settingsKey(TouchEdge::Bottom), QStringLiteral("input.touch.edgeBottom"));
    const TouchEdgeActions decoded = TouchEdgeActions::fromSettingsValues({
        {QStringLiteral("input.touch.edgeLeft"), QStringLiteral("none")},
        {QStringLiteral("input.touch.edgeRight"), QStringLiteral("overview")},
    });
    QCOMPARE(decoded.left, QStringLiteral("none"));
    QCOMPARE(decoded.right, QStringLiteral("overview"));
    QCOMPARE(decoded.top, QStringLiteral("notifications"));
    QCOMPARE(decoded.bottom, QStringLiteral("task-switcher"));
    QVERIFY(decoded != defaults);
    QCOMPARE(TouchPreferences::settingsKeys().size(), 7);
}

void TouchEdgeActionsTests::unknownActionsBecomeNone()
{
    const TouchEdgeActions decoded = TouchEdgeActions::fromSettingsValues({
        {QStringLiteral("input.touch.edgeTop"), QStringLiteral("launch-missiles")},
        {QStringLiteral("input.touch.edgeLeft"), 42},
    });
    QCOMPARE(decoded.top, QStringLiteral("none"));
    QCOMPARE(decoded.left, QStringLiteral("none"));
    QVERIFY(TouchEdgeActions::knownActions().contains(QStringLiteral("none")));
}

void TouchEdgeActionsTests::preferencesClampLongPress()
{
    QCOMPARE(TouchPreferences{}.longPressMs, 500);
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.longPressMs"), 50}}).longPressMs, 200);
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.longPressMs"), 5000}}).longPressMs, 1500);
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.longPressMs"), 750}}).longPressMs, 750);
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.longPressMs"), QStringLiteral("soon")}}).longPressMs, 500);
    QVERIFY(TouchPreferences{}.touchscreenEnabled);
    QVERIFY(!TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.enabled"), false}}).touchscreenEnabled);
    QCOMPARE(TouchPreferences{}.onScreenKeyboard, QStringLiteral("auto"));
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.onScreenKeyboard"), QStringLiteral("off")}}).onScreenKeyboard, QStringLiteral("off"));
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.onScreenKeyboard"), QStringLiteral("sometimes")}}).onScreenKeyboard, QStringLiteral("auto"));
    QCOMPARE(TouchPreferences::fromSettingsValues({{QStringLiteral("input.touch.onScreenKeyboard"), QStringLiteral("on")}}).onScreenKeyboard, QStringLiteral("auto"));
}

void TouchEdgeActionsTests::gesturesReserveOnlyEdgesWithActions()
{
    RecordingReserver reserver;
    {
        TouchEdgeGestures gestures(reserver);
        QVERIFY(reserver.log.isEmpty());
        gestures.apply(TouchEdgeActions{});
        QCOMPARE(reserver.log.size(), 3);
        QStringList edges;
        for (const auto &entry : std::as_const(reserver.log)) {
            QCOMPARE(entry.verb, QStringLiteral("reserve"));
            edges.append(entry.edge);
        }
        QCOMPARE(edges, (QStringList{QStringLiteral("left"), QStringLiteral("top"), QStringLiteral("bottom")}));
        // Re-applying the same mapping reserves nothing twice.
        gestures.apply(TouchEdgeActions{});
        QCOMPARE(reserver.log.size(), 3);
        TouchEdgeActions changed;
        changed.left = QStringLiteral("none");
        changed.right = QStringLiteral("notifications");
        gestures.apply(changed);
        QCOMPARE(reserver.log.size(), 5);
        QCOMPARE(reserver.log.at(3).verb, QStringLiteral("unreserve"));
        QCOMPARE(reserver.log.at(3).edge, QStringLiteral("left"));
        QCOMPARE(reserver.log.at(4).verb, QStringLiteral("reserve"));
        QCOMPARE(reserver.log.at(4).edge, QStringLiteral("right"));
        QCOMPARE(gestures.actions(), changed);
        // Re-arming reserves the three held edges again and releases nothing.
        gestures.rearm();
        QCOMPARE(reserver.log.size(), 8);
        for (int index = 5; index < 8; ++index) {
            QCOMPARE(reserver.log.at(index).verb, QStringLiteral("reserve"));
        }
    }
    // Destruction releases what is still reserved: top, bottom, right.
    QCOMPARE(reserver.log.size(), 11);
    for (int index = 8; index < 11; ++index) {
        QCOMPARE(reserver.log.at(index).verb, QStringLiteral("unreserve"));
    }
}

void TouchEdgeActionsTests::triggeredActionsAnnounceEdgeAndAction()
{
    RecordingReserver reserver;
    TouchEdgeGestures gestures(reserver);
    gestures.apply(TouchEdgeActions{});
    QSignalSpy triggered(&gestures, &TouchEdgeGestures::triggered);
    gestures.actionObject(TouchEdge::Top)->trigger();
    gestures.actionObject(TouchEdge::Right)->trigger();
    gestures.actionObject(TouchEdge::Bottom)->trigger();
    QCOMPARE(triggered.count(), 2);
    QCOMPARE(triggered.at(0).at(0).toString(), QStringLiteral("top"));
    QCOMPARE(triggered.at(0).at(1).toString(), QStringLiteral("notifications"));
    QCOMPARE(triggered.at(1).at(0).toString(), QStringLiteral("bottom"));
    QCOMPARE(triggered.at(1).at(1).toString(), QStringLiteral("task-switcher"));
    QCOMPARE(reserver.log.constFirst().action, gestures.actionObject(TouchEdge::Left));
}

QTEST_MAIN(TouchEdgeActionsTests)
#include "tst_touchedgeactions.moc"
