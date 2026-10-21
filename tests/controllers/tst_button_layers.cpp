// SPDX-License-Identifier: GPL-3.0-or-later
#include "button_layers_p.h"
#include <QtTest>
using namespace QindaQt::Controllers;
class ButtonLayersTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void r2KeepsDragAndChangesDpadToNudgesEvenOnSimultaneousPress() {
        const auto p = defaultProfile("dualsense"); ButtonLayers layers;
        const auto edges = layers.sample(p, {{"righttrigger", true}, {"dpup", true}});
        QCOMPARE(edges.size(), 2); QCOMPARE(edges[0].button, "righttrigger");
        QCOMPARE(edges[0].binding.modifiers, QStringList{"Meta"}); QCOMPARE(edges[0].binding.mouseButtons, QStringList{"left"});
        QCOMPARE(edges[1].binding.action, "pointer-up"); QCOMPARE(layers.page(p).id, "move");
        QVERIFY(layers.sample(p, {{"righttrigger", true}, {"dpup", true}}).isEmpty());
        const auto release = layers.sample(p, {{"righttrigger", false}, {"dpup", true}});
        QCOMPARE(release.size(), 2); for (const auto &edge : release) QVERIFY(!edge.down);
        QCOMPARE(layers.page(p).id, "main"); QVERIFY(layers.sample(p, {{"dpup", true}}).isEmpty());
        layers.sample(p, {}); const auto plain = layers.sample(p, {{"dpup", true}});
        QCOMPARE(plain.first().binding.action, "shortcut"); QCOMPARE(plain.first().binding.shortcut, "Ctrl+K");
    }
    void editingLayerOrdersSimultaneousCtrlShiftBeforeLettersAndPreservesShift() {
        auto p = defaultProfile("dualsense"); p.activePage = "navigation"; ButtonLayers layers;
        const auto edges = layers.sample(p, {{"lefttrigger", true}, {"righttrigger", true}, {"dpleft", true}});
        QCOMPARE(edges.size(), 3); QCOMPARE(edges[0].binding.modifiers, QStringList{"Ctrl"});
        QCOMPARE(edges[1].binding.modifiers, QStringList{"Shift"}); QCOMPARE(edges[2].binding.keys, QStringList{"C"});
        const auto release = layers.sample(p, {{"righttrigger", true}, {"dpleft", true}});
        for (const auto &edge : release) QVERIFY(edge.button != "righttrigger");
        QCOMPARE(layers.page(p).id, "navigation");
    }
    void existingDpadHoldDoesNotChangeMeaningAndGyroContactSurvivesLayerChanges() {
        const auto p = defaultProfile("dualsense"); ButtonLayers layers;
        layers.sample(p, {{"dpup", true}, {"touchpad-touch", true}}); QVERIFY(layers.heldAction("hold-gyro"));
        const auto edges = layers.sample(p, {{"dpup", true}, {"touchpad-touch", true}, {"righttrigger", true}});
        QCOMPARE(edges.size(), 2); QVERIFY(layers.heldAction("hold-gyro"));
        QVERIFY(layers.sample(p, {{"dpup", true}, {"touchpad-touch", true}, {"righttrigger", true}}).isEmpty());
        layers.clear(); QCOMPARE(layers.page(p).id, "main"); QVERIFY(!layers.heldAction("hold-gyro"));
    }
};
QTEST_GUILESS_MAIN(ButtonLayersTest)
#include "tst_button_layers.moc"
