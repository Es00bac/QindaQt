// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_policy.h"
#include "qindaqt/controllers/profile_store.h"
#include "qindaqt/controllers/game_priority.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
using namespace QindaQt::Controllers;
class ControllerPolicyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void physicalIdentitySurvivesTransportChange() {
        const auto usb = controllerProfileId("usb-guid", "0C-27-56-59-E8-33", 0x054c, 0x0ce6, "playstation");
        QCOMPARE(usb, controllerProfileId("bt-guid", "0c:27:56:59:e8:33", 0x054c, 0x0ce6, "playstation"));
        QVERIFY(usb != controllerProfileId("bt-guid", "0c:27:56:59:e8:34", 0x054c, 0x0ce6, "playstation"));
        QVERIFY(controllerProfileId("usb-guid", {}, 0x054c, 0x0ce6, "playstation")
            != controllerProfileId("bt-guid", {}, 0x054c, 0x0ce6, "playstation"));
    }
    void defaultsAndRoundTrip() {
        const auto p = defaultProfile();
        QCOMPARE(p.bindings.value("back").action, "dictate");
        QVERIFY(!p.gyro);
        auto restored = Profile{}; QString reason;
        QVERIFY(applyPatch(profileJson(p), restored, reason)); QCOMPARE(restored, p);
        QTemporaryDir dir; QVERIFY(dir.isValid());
        ProfileStore store(dir.filePath("config/controllers.json"));
        auto changed = p; changed.bindings["touchpad"] = {"dictate", {}}; changed.gyro = true;
        store.set("pad:test", changed, {{"id", "pad:test"}, {"name", "Test pad"}});
        QVERIFY(store.save(reason));
        ProfileStore readback(dir.filePath("config/controllers.json")); QVERIFY(readback.load(reason));
        QCOMPARE(readback.profile("pad:test"), changed); QCOMPARE(readback.ids().size(), 6);
        QCOMPARE(readback.description("pad:test").value("name").toString(), "Test pad");
    }
    void dualSenseDefaultsMatchRequestedLayout() {
        const auto p = defaultProfile("dualsense");
        const QMap<QString, Binding> expected{
            {"back", {"back", {}}}, {"dpup", {"shortcut", "Ctrl+K"}},
            {"dpleft", {"shortcut", "Ctrl+C"}}, {"dpright", {"shortcut", "Ctrl+J"}},
            {"dpdown", {"accept", {}}}, {"leftstick", {"previous-window", {}}},
            {"guide", {"launcher", {}}}, {"start", {"back", {}}},
            {"north", {"maximize", {}}}, {"east", {"overview", {}}},
            {"west", {"voice-command", {}}}, {"south", {"dictate", {}}},
            {"rightstick", {"next-window", {}}}, {"misc1", {"none", {}}},
            {"touchpad", {"toggle-gyro", {}}}, {"rightshoulder", {"left-click", {}}},
            {"righttrigger", {"left-click-meta-shift", {}}},
            {"leftshoulder", {"right-click", {}}}, {"lefttrigger", {"right-click-meta-shift", {}}}};
        for (auto it = expected.begin(); it != expected.end(); ++it) QCOMPARE(p.bindings.value(it.key()), it.value());
        Profile restored; QString reason;
        QVERIFY(applyPatch(profileJson(p), restored, reason)); QCOMPARE(restored, p);
        for (const auto &family : QStringList{"xbox", "playstation", "nintendo", "generic"})
            QCOMPARE(defaultProfile(family), defaultProfile());
        QCOMPARE(controllerProfileId("usb", "0C-27-56-59-E8-33", 0x054c, 0x0ce6, "dualsense"),
                 controllerProfileId("bt", "0c:27:56:59:e8:33", 0x054c, 0x0ce6, "playstation"));
    }
    void legacyDualSenseProfileRetainsPreferencesAndGetsCorrectFamily() {
        QTemporaryDir dir; QString reason;
        auto saved = defaultProfile(); saved.pointerStick = "right"; saved.gyro = true;
        ProfileStore before(dir.filePath("controllers.json"));
        before.set("pad:legacy", saved, {{"name", "DualSense Wireless Controller"}, {"family", "playstation"}});
        QVERIFY(before.save(reason));
        ProfileStore after(dir.filePath("controllers.json")); QVERIFY(after.load(reason));
        QCOMPARE(after.profile("pad:legacy"), saved);
        QCOMPARE(after.description("pad:legacy").value("family").toString(), "dualsense");
        QCOMPARE(after.profile("pad:new", "dualsense"), defaultProfile("dualsense"));
    }
    void rejectsAtomicMalformedPatches() {
        auto p = defaultProfile(); const auto before = p; QString reason;
        QVERIFY(!applyPatch({{"enabled", false}, {"pointerSpeed", -1}}, p, reason)); QCOMPARE(p, before);
        QVERIFY(!applyPatch({{"bindings", QJsonObject{{"unknown", QJsonObject{{"action", "dictate"}}}}}}, p, reason));
        QVERIFY(!applyPatch({{"bindings", QJsonObject{{"back", QJsonObject{{"action", "shell-command"}}}}}}, p, reason));
        QVERIFY(!applyPatch({{"bindings", QJsonObject{{"back", QJsonObject{{"action", "shortcut"}, {"shortcut", "Ctrl+A, Ctrl+B"}}}}}}, p, reason));
        QVERIFY(applyPatch({{"bindings", QJsonObject{{"back", QJsonObject{{"action", "shortcut"}, {"shortcut", "Insert"}}}}}}, p, reason));
        QCOMPARE(p.bindings.value("back").shortcut, "Ins");
    }
    void motionBoundsAndNeutral() {
        QCOMPARE(stickMotion({0.1, 0}, 0.18, 1000, 0.016), QPointF{});
        QCOMPARE(stickMotion({0, 0}, 0.18, 1000, 0.016), QPointF{});
        QVERIFY(stickMotion({1, 0}, 0.18, 1000, 0.016).x() <= 16.01);
        QVERIFY(stickMotion({1, 0}, 0.18, 1000, 4).x() <= 50.01);
        QCOMPARE(gyroMotion({0.01, 0.01}, 650, 0.016), QPointF{});
        QCOMPARE(gyroMotion({std::numeric_limits<double>::quiet_NaN(), 0}, 650, 0.016), QPointF{});
        QVERIFY(gyroMotion({1, 0}, 650, 0.016).x() > 10);
    }
    void gamePriorityIncludesPreexistingOpens() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto write = [](QString path, QByteArray data) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size()); };
        QDir().mkpath(dir.filePath("42/fd")); write(dir.filePath("42/comm"), "steam\n");
        QDir().mkpath(dir.filePath("43/fd")); write(dir.filePath("43/comm"), "game\n");
        const QString device = dir.filePath("event17"); write(device, ""); QVERIFY(QFile::link(device, dir.filePath("43/fd/5")));
        const auto state = inspectGamePriority({device}, dir.path(), 99);
        QVERIFY(state.steam); QVERIFY(state.busyPaths.contains(device));
        const auto ignoreSelf = inspectGamePriority({device}, dir.path(), 43);
        QVERIFY(ignoreSelf.steam); QVERIFY(ignoreSelf.busyPaths.isEmpty());
    }
    void gamePriorityReadsDescriptorLinksWithoutFollowingTargets() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath("42/fd")));
        QFile comm(dir.filePath("42/comm")); QVERIFY(comm.open(QIODevice::WriteOnly));
        comm.write("game\n"); comm.close();
        // Proc FD links can name missing/deleted/unreachable targets. Their
        // text, not a target stat or canonicalization, identifies the consumer.
        const auto device = dir.filePath("unavailable/event17");
        QVERIFY(QFile::link(device, dir.filePath("42/fd/5")));
        QVERIFY(QFile::link("socket:[123]", dir.filePath("42/fd/6")));
        const auto state = inspectGamePriority({device}, dir.path(), 99);
        QCOMPARE(state.busyPaths, QSet<QString>{device});
        QVERIFY(!state.steam);
    }
};
QTEST_MAIN(ControllerPolicyTest)
#include "tst_controller_policy.moc"
