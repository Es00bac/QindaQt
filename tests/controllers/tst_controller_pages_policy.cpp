// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_policy.h"
#include "qindaqt/controllers/profile_store.h"
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Controllers;
class ControllerPagesPolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void arbitraryCombinationsAndTextRoundTrip() {
        auto profile = defaultProfile("dualsense");
        const QJsonObject combo{{"action", "combination"}, {"modifiers", QJsonArray{"Meta", "Ctrl"}},
            {"keys", QJsonArray{"Esc", "Tab", "F24", "+"}}, {"mouseButtons", QJsonArray{"left", "forward"}}};
        const QString text = QString::fromUtf8("Hello\t世界 🌎\nSecond line\n");
        QString reason;
        QVERIFY2(applyPatch({{"bindings", QJsonObject{{"south", combo}, {"west", QJsonObject{{"action", "text"}, {"text", text}}}}}}, profile, reason), qPrintable(reason));
        auto second = activePage(profile); second.id = "work"; second.name = "Work"; second.color = "#AB12EF";
        second.bindings["misc1"] = {"switch-page", {}, {}, {}, {}, {}, "main"};
        profile.pages.append(second); profile.activePage = "work";
        Profile restored; QVERIFY(applyPatch(profileJson(profile), restored, reason)); QCOMPARE(restored, profile);
        QTemporaryDir dir; ProfileStore store(dir.filePath("controllers.json"));
        store.set("pad:test", profile); QVERIFY(store.save(reason));
        ProfileStore after(dir.filePath("controllers.json")); QVERIFY(after.load(reason)); QCOMPARE(after.profile("pad:test"), profile);
        QCOMPARE(activePage(after.profile("pad:test")).bindings["west"].text, text);
        QCOMPARE(QFile::permissions(dir.filePath("controllers.json")) & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther), QFileDevice::Permissions{});
    }
    void versionOneMigrationPreservesEveryBindingAndMotionPreference() {
        QTemporaryDir dir; const auto path = dir.filePath("controllers.json");
        const QJsonObject config{{"enabled", true}, {"pointerStick", "right"}, {"pointerSpeed", 1200}, {"deadzone", .23},
            {"touchpad", false}, {"gyro", true}, {"gyroSpeed", 450}, {"bindings", QJsonObject{
                {"righttrigger", QJsonObject{{"action", "left-click-meta-shift"}, {"shortcut", ""}}},
                {"south", QJsonObject{{"action", "shortcut"}, {"shortcut", "Ctrl+X"}}}}}};
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        const QJsonObject row{{"id", "pad:legacy"}, {"description", QJsonObject{{"name", "DualSense Wireless Controller"}, {"family", "playstation"}}}, {"config", config}};
        file.write(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"profiles", QJsonArray{row}}}).toJson()); file.close();
        QString reason; ProfileStore store(path); QVERIFY2(store.load(reason), qPrintable(reason));
        const auto profile = store.profile("pad:legacy"); QCOMPARE(profile.pages.size(), 1); QCOMPARE(profile.activePage, "main");
        QCOMPARE(profile.pointerStick, "right"); QCOMPARE(profile.pointerSpeed, 1200.0); QCOMPARE(profile.deadzone, .23);
        QVERIFY(!profile.touchpad); QVERIFY(profile.gyro); QCOMPARE(profile.gyroSpeed, 450.0);
        QCOMPARE(activePage(profile).bindings["righttrigger"].action, "left-click-meta-shift");
        QCOMPARE(activePage(profile).bindings["south"].shortcut, "Ctrl+X");
        QVERIFY(store.save(reason)); QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value("schemaVersion").toInt(), 2);
        ProfileStore after(path); QVERIFY(after.load(reason)); QCOMPARE(after.profile("pad:legacy"), profile);
    }
    void invalidCombinationsAreAtomic_data() {
        QTest::addColumn<QJsonObject>("binding");
        QTest::newRow("empty") << QJsonObject{{"action", "combination"}, {"keys", QJsonArray{}}};
        QTest::newRow("unknown-mod") << QJsonObject{{"action", "combination"}, {"modifiers", QJsonArray{"Super"}}, {"keys", QJsonArray{}}};
        QTest::newRow("duplicate") << QJsonObject{{"action", "combination"}, {"keys", QJsonArray{"A", "A"}}};
        QTest::newRow("chord-in-key") << QJsonObject{{"action", "combination"}, {"keys", QJsonArray{"Ctrl+K"}}};
        QTest::newRow("sequence") << QJsonObject{{"action", "combination"}, {"keys", QJsonArray{"A, B"}}};
        QTest::newRow("mouse") << QJsonObject{{"action", "combination"}, {"keys", QJsonArray{}}, {"mouseButtons", QJsonArray{"wheel"}}};
        QTest::newRow("text-type") << QJsonObject{{"action", "text"}, {"text", 42}};
        QTest::newRow("text-length") << QJsonObject{{"action", "text"}, {"text", QString(MaxTextBytes + 1, 'x')}};
        QTest::newRow("text-control") << QJsonObject{{"action", "text"}, {"text", QString(QChar(0))}};
        QTest::newRow("text-surrogate") << QJsonObject{{"action", "text"}, {"text", QString(QChar(0xd800))}};
        QTest::newRow("invisible-command") << QJsonObject{{"action", "none"}, {"command", "anything"}};
    }
    void invalidCombinationsAreAtomic() {
        QFETCH(QJsonObject, binding); auto p = defaultProfile(); const auto before = p; QString reason;
        QVERIFY(!applyPatch({{"enabled", false}, {"bindings", QJsonObject{{"south", binding}}}}, p, reason)); QCOMPARE(p, before);
    }
    void pageReferencesLimitsAndNamesAreAtomic() {
        auto p = defaultProfile(); const auto before = p; QString reason;
        QVERIFY(!applyPatch({{"activePage", "missing"}, {"bindings", QJsonObject{}}}, p, reason)); QCOMPARE(p, before);
        QVERIFY(!applyPatch({{"pages", QJsonArray{}}}, p, reason)); QCOMPARE(p, before);
        auto a = pageJson(activePage(p)); auto b = a;
        QVERIFY(!applyPatch({{"pages", QJsonArray{a, b}}}, p, reason)); QCOMPARE(p, before);
        b["id"] = "work"; b["color"] = "#GG0000";
        QVERIFY(!applyPatch({{"pages", QJsonArray{a, b}}}, p, reason)); QCOMPARE(p, before);
        b["color"] = "#ff0000"; b["name"] = "\n";
        QVERIFY(!applyPatch({{"pages", QJsonArray{a, b}}}, p, reason)); QCOMPARE(p, before);
        b["name"] = "Work";
        QVERIFY(applyPatch({{"pages", QJsonArray{a, b}}, {"activePage", "work"}}, p, reason));
        QCOMPARE(activePage(p).color, "#FF0000");
        const auto good = p;
        QVERIFY(!applyPatch({{"bindings", QJsonObject{{"south", QJsonObject{{"action", "switch-page"}, {"page", "missing"}}}}}}, p, reason)); QCOMPARE(p, good);
        QJsonArray many;
        for (int i = 0; i <= MaxPages; ++i) { b["id"] = QString::number(i); many.append(b); }
        QVERIFY(!applyPatch({{"pages", many}}, p, reason)); QCOMPARE(p, good);
    }
    void modifierOnlyAndMetaLeftClickAreRepresentable() {
        auto p = defaultProfile("dualsense"); const auto r2 = activePage(p).bindings["righttrigger"];
        QCOMPARE(r2.action, "combination"); QCOMPARE(r2.modifiers, QStringList{"Meta"}); QCOMPARE(r2.mouseButtons, QStringList{"left"}); QVERIFY(r2.keys.isEmpty());
        QString reason;
        QVERIFY(applyPatch({{"bindings", QJsonObject{{"south", QJsonObject{{"action", "combination"}, {"modifiers", QJsonArray{"Meta", "Shift"}}, {"keys", QJsonArray{}}}}}}}, p, reason));
        QCOMPARE(activePage(p).bindings["south"].modifiers, (QStringList{"Shift", "Meta"}));
    }
};
QTEST_MAIN(ControllerPagesPolicyTest)
#include "tst_controller_pages_policy.moc"
