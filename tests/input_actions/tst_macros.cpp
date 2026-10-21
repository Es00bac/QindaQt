// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/input_actions/input_actions.h>
#include <qindaqt/input_actions/macro_runner.h>
#include <qindaqt/input_actions/macro_store.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::InputActions;
namespace {
Macro sample() {
    Macro m; m.id = "example"; m.name = "Example";
    MacroStep press; press.kind = "press"; press.binding = {"combination", {}, {"Meta"}, {}, {"left"}};
    MacroStep wait; wait.kind = "wait"; wait.milliseconds = 30;
    MacroStep move; move.kind = "move"; move.x = 10;
    MacroStep release; release.kind = "release";
    MacroStep text; text.kind = "text"; text.binding = {"text", {}, {}, {}, {}, QString::fromUtf8("Hello\n世界 🌎")};
    m.steps = {press, wait, move, release, text}; return m;
}
class Output final : public MacroOutput {
public:
    QObject first, second; QObject *focus = &first;
    bool permitted = true, accepts = true; bool followNavigation = false;
    QSet<QString> held; QMap<QString, TextState> texts;
    QStringList events; QList<QPointF> motions;
    QObject *target() const override { return focus; }
    bool allowed() const override { return permitted; }
    bool invoke(const QString &token, const Binding &binding, bool down) override {
        if (down && !accepts) return false;
        events << (down ? "down:" + binding.action : "up");
        if (down && binding.action == "text") texts[token] = Pending;
        if (down) held.insert(token); else held.remove(token);
        if (!down && followNavigation) focus = &second;
        return true;
    }
    void motion(QPointF point) override { motions.append(point); }
    void scroll(QPointF) override {}
    void reset(const QString &prefix) override { for (const auto &token : held.values()) if (token.startsWith(prefix + '/')) held.remove(token); }
    TextState textState(const QString &token) const override { return texts.value(token, Failed); }
};
}
class MacrosTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void codecAndOwnerOnlyStoreRoundTrip() {
        auto m = sample(); m.shortcut = "Meta+F13"; m.mouseButton = "back"; m.mouseModifiers = {"Ctrl"};
        Macro restored; QString reason; QVERIFY(readMacro(macroJson(m), restored, reason)); QCOMPARE(restored, m);
        QTemporaryDir dir; const auto path = dir.filePath("input-macros.json"); MacroStore store(path);
        QVERIFY(store.load(reason)); QVERIFY(store.set(m, reason));
        MacroStore after(path); QVERIFY(after.load(reason)); QCOMPARE(after.value(m.id), m);
        QCOMPARE(QFile::permissions(path) & (QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ReadGroup | QFileDevice::WriteGroup), QFileDevice::Permissions{});
        QVERIFY(after.remove(m.id, reason)); QVERIFY(!after.contains(m.id));
    }
    void malformedSequenceNeverAdoptsPartialMacro_data() {
        QTest::addColumn<QJsonArray>("steps");
        QTest::newRow("unknown") << QJsonArray{QJsonObject{{"kind", "execute"}, {"command", "anything"}}};
        QTest::newRow("release-without-hold") << QJsonArray{QJsonObject{{"kind", "release"}, {"slot", 0}}};
        QTest::newRow("unreleased") << QJsonArray{stepJson(sample().steps.first())};
        QTest::newRow("long-wait") << QJsonArray{QJsonObject{{"kind", "wait"}, {"milliseconds", 10001}}};
        QTest::newRow("fractional-wait") << QJsonArray{QJsonObject{{"kind", "wait"}, {"milliseconds", 1.5}}};
        QTest::newRow("recursive") << QJsonArray{QJsonObject{{"kind", "tap"}, {"milliseconds", 40}, {"binding", QJsonObject{{"action", "macro"}, {"macro", "other"}}}}};
        QTest::newRow("layer") << QJsonArray{QJsonObject{{"kind", "tap"}, {"milliseconds", 40}, {"binding", QJsonObject{{"action", "hold-page"}, {"page", "move"}}}}};
        QTest::newRow("infinite-move") << QJsonArray{QJsonObject{{"kind", "move"}, {"x", 4001}, {"y", 0}}};
    }
    void malformedSequenceNeverAdoptsPartialMacro() {
        QFETCH(QJsonArray, steps); const auto before = sample(); auto macro = before; auto json = macroJson(before); json["steps"] = steps;
        QString reason; QVERIFY(!readMacro(json, macro, reason)); QCOMPARE(macro, before);
    }
    void failedSaveAndBadLoadPreserveTheLibrary() {
        QTemporaryDir dir; const auto path = dir.filePath("input-macros.json"); MacroStore store(path); QString reason;
        QVERIFY(store.set(sample(), reason)); QVERIFY(QFile::remove(path)); QVERIFY(QDir().mkdir(path));
        auto changed = sample(); changed.name = "Changed"; QVERIFY(!store.set(changed, reason)); QCOMPARE(store.value(changed.id).name, "Example");
        QVERIFY(QDir().rmdir(path)); QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{broken"); f.close();
        QVERIFY(!store.load(reason)); QCOMPARE(store.value(changed.id).name, "Example");
    }
    void orderedHoldsTextWaitsAndMotionUseOneRunner() {
        Output output; MacroRunner runner(output); QVERIFY(runner.start("pad/1/button", sample()));
        QTRY_VERIFY(!output.texts.isEmpty()); QVERIFY(runner.running()); QVERIFY(output.held.isEmpty());
        QCOMPARE(output.motions, QList<QPointF>{QPointF(10, 0)});
        for (auto &state : output.texts) state = MacroOutput::Complete;
        QTRY_VERIFY(!runner.running());
        QCOMPARE(output.events, (QStringList{"down:combination", "up", "down:text", "up"}));
    }
    void suppressionDuringLongWaitReleasesWithoutWaitingForTheStep() {
        Output output; MacroRunner runner(output); auto m = sample(); m.steps[1].milliseconds = 10000;
        QVERIFY(runner.start("pad/1/button", m)); QTRY_VERIFY(!output.held.isEmpty()); output.permitted = false;
        QTRY_VERIFY_WITH_TIMEOUT(!runner.running(), 100); QVERIFY(output.held.isEmpty());
    }
    void focusChangeCancelAndDestructionReleaseOwnedHolds() {
        Output output; auto m = sample(); m.steps[1].milliseconds = 10000;
        { MacroRunner runner(output); QVERIFY(runner.start("pad/1/button", m)); QTRY_VERIFY(!output.held.isEmpty()); output.focus = &output.second;
          QTRY_VERIFY(!runner.running()); QVERIFY(output.held.isEmpty());
          QVERIFY(runner.start("pad/1/button", m)); QTRY_VERIFY(!output.held.isEmpty()); runner.cancel("other"); QVERIFY(runner.running()); runner.cancel("pad/1"); QVERIFY(!runner.running()); QVERIFY(output.held.isEmpty());
          QVERIFY(runner.start("keyboard/example", m)); QTRY_VERIFY(!output.held.isEmpty()); }
        QVERIFY(output.held.isEmpty());
    }
    void failedStepStopsBeforeFollowingTextAndReleasesPriorHolds() {
        Output output; MacroRunner runner(output); auto m = sample();
        MacroStep tap; tap.binding = {"shortcut", "K"}; m.steps.insert(2, tap); m.steps[1].milliseconds = 200;
        QVERIFY(runner.start("pad/1/button", m)); QTRY_VERIFY(!output.held.isEmpty());
        output.accepts = false;
        QTRY_VERIFY(!runner.running()); QVERIFY(output.held.isEmpty()); QVERIFY(output.texts.isEmpty());
    }
    void navigationCanAdoptItsIntendedNewWindowAndFinishText() {
        Output output; output.followNavigation = true; MacroRunner runner(output); auto m = sample();
        MacroStep next; next.binding = {"next-window"}; next.milliseconds = 10; m.steps = {next, m.steps.last()};
        QVERIFY(runner.start("keyboard/example", m)); QTRY_VERIFY(!output.texts.isEmpty());
        for (auto &state : output.texts) state = MacroOutput::Complete;
        QTRY_VERIFY(!runner.running()); QCOMPARE(output.focus, &output.second);
    }
};
QTEST_GUILESS_MAIN(MacrosTest)
#include "tst_macros.moc"
