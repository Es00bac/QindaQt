// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controllers_model.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QtTest>
using namespace QindaQt::Apps::SettingsInput;
class FakePort final : public ControllerPort {
public:
    void refresh() override { ++refreshes; }
    void apply(const QString &id, const QJsonObject &patch, quint64 revision) override { lastId = id; lastPatch = patch; lastRevision = revision; }
    void reset(const QString &id, quint64 revision) override { lastId = id; lastRevision = revision; }
    int refreshes = 0; QString lastId; QJsonObject lastPatch; quint64 lastRevision = 0;
};
class ControllerSettingsTest : public QObject {
    Q_OBJECT
private:
    QJsonObject snapshot(bool steam = false) {
        return {{"schemaVersion", 1}, {"revision", "7"}, {"steam", steam},
            {"controllers", QJsonArray{QJsonObject{{"id", "default:playstation"}, {"name", "PlayStation defaults"},
                {"family", "playstation"}, {"template", true}, {"config", QindaQt::Controllers::profileJson(QindaQt::Controllers::defaultProfile())}}}}};
    }
private Q_SLOTS:
    void connectedProfileIsSelectedAndExplicitDefaultsStaySelected() {
        FakePort port; ControllersModel model(port);
        auto data = snapshot();
        auto rows = data.value("controllers").toArray();
        auto pad = rows[0].toObject(); pad["id"] = "pad:connected"; pad["template"] = false;
        pad["connected"] = true; rows.append(pad); data["controllers"] = rows;
        Q_EMIT port.snapshotReceived(data);
        QCOMPARE(model.selectedId(), "pad:connected");
        QVERIFY(model.controllers().last().toMap().value("displayName").toString().contains("connected"));
        model.select("default:playstation"); Q_EMIT port.snapshotReceived(data);
        QCOMPARE(model.selectedId(), "default:playstation");
        model.select("pad:connected"); pad["connected"] = false; rows[1] = pad;
        pad["id"] = "pad:reconnected"; pad["connected"] = true; rows.append(pad);
        data["controllers"] = rows; data["revision"] = "8";
        Q_EMIT port.snapshotReceived(data); QCOMPARE(model.selectedId(), "pad:reconnected");
    }
    void bindingWaitsForReadback() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        QVERIFY(model.available()); QCOMPARE(model.selectedId(), "default:playstation");
        QVERIFY(model.setBinding("touchpad", "dictate")); QVERIFY(model.busy());
        QCOMPARE(port.lastId, "default:playstation"); QCOMPARE(port.lastRevision, quint64(7));
        QCOMPARE(port.lastPatch.value("bindings").toObject().value("touchpad").toObject().value("action").toString(), "dictate");
        QCOMPARE(model.currentPage().value("bindings").toMap().value("touchpad").toMap().value("action").toString(), "left-click");
        Q_EMIT port.completed(true, "ok"); QVERIFY(!model.busy());
    }
    void steamCanStillConfigure() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot(true));
        QVERIFY(model.statusText().contains("Steam")); QVERIFY(model.setOption("gyro", true));
        QCOMPARE(port.lastPatch.value("gyro").toBool(), true);
    }
    // ADR-0428: "game" now means the app being used reads the pad directly; the
    // optional additive `consumer` field names it without a schema change.
    void gameReasonDescribesTheFocusedAppAndNamesItsConsumerSafely() {
        FakePort port; ControllersModel model(port);
        const auto publish = [&](const QJsonValue &consumer) {
            auto data = snapshot(); data["schemaVersion"] = 2; data["ready"] = true;
            auto row = data.value("controllers").toArray().first().toObject();
            row["id"] = "pad:connected"; row["template"] = false; row["connected"] = true; row["reason"] = "game";
            row["futureField"] = "ignored";
            if (!consumer.isUndefined()) row["consumer"] = consumer;
            data["controllers"] = QJsonArray{row}; Q_EMIT port.snapshotReceived(data);
        };
        publish(QJsonValue::Undefined);
        QVERIFY(model.available()); QCOMPARE(model.selectedId(), "pad:connected");
        QVERIFY2(model.statusText().contains("reading this controller directly"), qPrintable(model.statusText()));
        QVERIFY(model.statusText().contains("while it's focused"));
        QVERIFY(!model.statusText().contains("A game or another app"));
        publish("mygame");
        QVERIFY2(model.statusText().startsWith("mygame is reading"), qPrintable(model.statusText()));
        publish("<b>x</b>\u202e" + QString(80, 'y'));
        QVERIFY(model.available());
        QVERIFY(!model.statusText().contains('<')); QVERIFY(!model.statusText().contains(QChar(0x202e)));
        QVERIFY(model.statusText().size() < 140);
        publish(42);
        QVERIFY2(model.statusText().startsWith("The app you're using"), qPrintable(model.statusText()));
    }
    void lostOwnerDropsAuthority() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        Q_EMIT port.unavailable(); QVERIFY(!model.available()); QVERIFY(!model.setBinding("back", "dictate"));
        QVERIFY(model.controllers().isEmpty());
    }
    void pageCreationAndCombinationEditingUseConfirmedReadback() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        const auto before = model.currentPage();
        QVERIFY(model.addPage("Work", "#A83DFF")); QCOMPARE(model.pages().size(), 1); QVERIFY(model.busy());
        const auto patch = port.lastPatch; const auto id = patch.value("pages").toArray().last().toObject().value("id").toString();
        QVERIFY(!patch.contains("activePage"));
        QVERIFY(!id.isEmpty()); QCOMPARE(patch.value("pages").toArray().size(), 2);
        auto data = snapshot(); auto row = data.value("controllers").toArray().first().toObject();
        auto profile = QindaQt::Controllers::defaultProfile(); QString reason;
        QVERIFY(QindaQt::Controllers::applyPatch(patch, profile, reason)); row["config"] = QindaQt::Controllers::profileJson(profile);
        data["controllers"] = QJsonArray{row}; data["revision"] = "8"; Q_EMIT port.snapshotReceived(data); Q_EMIT port.completed(true, "ok");
        QCOMPARE(model.currentPage().value("name").toString(), "Work"); QCOMPARE(model.currentPage().value("color").toString(), "#A83DFF");
        const QVariantMap combo{{"action", "combination"}, {"modifiers", QStringList{"Meta"}}, {"keys", QStringList{}}, {"mouseButtons", QStringList{"left"}}};
        QVERIFY(!model.saveBinding("righttrigger", combo, model.selectedId(), "wrong-page"));
        QVERIFY(model.saveBinding("righttrigger", combo, model.selectedId(), id));
        QCOMPARE(port.lastPatch.value("pages").toArray().last().toObject().value("bindings").toObject().value("righttrigger").toObject().value("modifiers").toArray(), QJsonArray{"Meta"});
        QVERIFY(!port.lastPatch.contains("activePage"));
        QCOMPARE(model.bindingSummary(combo), "Meta + Left click");
        QCOMPARE(model.currentPage().value("bindings").toMap(), before.value("bindings").toMap());
    }
    void profileSwitcherAndDeletionKeepReferencesValid() {
        FakePort port; ControllersModel model(port); auto profile = QindaQt::Controllers::defaultProfile();
        auto page = QindaQt::Controllers::activePage(profile); page.id = "work"; page.name = "Work";
        profile.pages.append(page); profile.activePage = "work";
        profile.pages[0].bindings["north"] = {"switch-page", {}, {}, {}, {}, {}, "work"};
        auto data = snapshot(); auto row = data.value("controllers").toArray().first().toObject(); row["config"] = QindaQt::Controllers::profileJson(profile);
        data["controllers"] = QJsonArray{row}; Q_EMIT port.snapshotReceived(data);
        QVERIFY(model.setPageSwitcher("misc1")); QString reason;
        QVERIFY(QindaQt::Controllers::applyPatch(port.lastPatch, profile, reason));
        for (const auto &page : profile.pages) QCOMPARE(page.bindings["misc1"].action, "next-page");
        row["config"] = QindaQt::Controllers::profileJson(profile); data["controllers"] = QJsonArray{row}; data["revision"] = "8";
        Q_EMIT port.snapshotReceived(data); Q_EMIT port.completed(true, "ok"); QCOMPARE(model.pageSwitcher(), "misc1");
        QVERIFY(model.removePage()); QVERIFY(QindaQt::Controllers::applyPatch(port.lastPatch, profile, reason));
        QCOMPARE(profile.pages.size(), 1); QCOMPARE(profile.activePage, "main");
        QCOMPARE(QindaQt::Controllers::activePage(profile).bindings["north"].action, "none");
    }
    void editingALayerDoesNotActivateItAndBindingPatchesAddressThatLayer() {
        FakePort port; ControllersModel model(port); auto data = snapshot(); auto row = data.value("controllers").toArray().first().toObject();
        row["config"] = QindaQt::Controllers::profileJson(QindaQt::Controllers::defaultProfile("dualsense")); data["controllers"] = QJsonArray{row}; Q_EMIT port.snapshotReceived(data);
        model.selectPage("move"); QCOMPARE(model.currentPage().value("id").toString(), "move");
        QCOMPARE(model.selected().value("config").toMap().value("activePage").toString(), "main"); QVERIFY(port.lastPatch.isEmpty());
        QVERIFY(model.setBinding("north", "accept")); QVERIFY(port.lastPatch.contains("pages")); QVERIFY(!port.lastPatch.contains("activePage"));
        auto profile = QindaQt::Controllers::defaultProfile("dualsense"); QString reason; QVERIFY(QindaQt::Controllers::applyPatch(port.lastPatch, profile, reason));
        QCOMPARE(profile.activePage, "main"); QCOMPARE(profile.pages[2].bindings["north"].action, "accept");
        QCOMPARE(profile.pages[0].bindings["north"].action, "maximize");
    }
    void allFamilyButtonsAndActionsReachable() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        bool touch = false, dictate = false, gyro = false;
        for (const auto &row : model.buttons()) touch |= row.toMap().value("id").toString() == "touchpad";
        for (const auto &row : model.actions()) dictate |= row.toMap().value("value").toString() == "dictate";
        gyro = model.setOption("gyro", true);
        QVERIFY(touch); QVERIFY(dictate); QVERIFY(gyro);
    }
    void dualSenseLabelsAndActionsReachable() {
        FakePort port; ControllersModel model(port);
        auto data = snapshot(); auto row = data.value("controllers").toArray().first().toObject();
        row["id"] = "default:dualsense"; row["family"] = "dualsense"; row["name"] = "DualSense defaults";
        row["config"] = QindaQt::Controllers::profileJson(QindaQt::Controllers::defaultProfile("dualsense"));
        data["controllers"] = QJsonArray{row}; Q_EMIT port.snapshotReceived(data);
        QMap<QString, QVariantMap> buttons;
        for (const auto &entry : model.buttons()) buttons[entry.toMap().value("id").toString()] = entry.toMap();
        QCOMPARE(buttons["south"].value("label").toString(), "Cross (south)");
        QCOMPARE(buttons["touchpad"].value("action").toString(), "left-click");
        QCOMPARE(buttons["west"].value("action").toString(), "voice-command");
        QCOMPARE(buttons["dpup"].value("shortcut").toString(), "Ctrl+K");
        QSet<QString> actions;
        for (const auto &entry : model.actions()) {
            QVERIFY(!entry.toMap().value("label").toString().isEmpty());
            actions.insert(entry.toMap().value("value").toString());
        }
        for (const auto &action : QStringList{"voice-command", "toggle-gyro", "left-click-meta-shift", "right-click-meta-shift"})
            QVERIFY(actions.contains(action));
    }
};
QTEST_MAIN(ControllerSettingsTest)
#include "tst_controller_settings.moc"
