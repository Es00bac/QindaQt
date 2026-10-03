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
        QCOMPARE(model.selected().value("config").toMap().value("bindings").toMap().value("touchpad").toMap().value("action").toString(), "left-click");
        Q_EMIT port.completed(true, "ok"); QVERIFY(!model.busy());
    }
    void steamCanStillConfigure() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot(true));
        QVERIFY(model.statusText().contains("Steam")); QVERIFY(model.setOption("gyro", true));
        QCOMPARE(port.lastPatch.value("gyro").toBool(), true);
    }
    void lostOwnerDropsAuthority() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        Q_EMIT port.unavailable(); QVERIFY(!model.available()); QVERIFY(!model.setBinding("back", "dictate"));
        QVERIFY(model.controllers().isEmpty());
    }
    void allFamilyButtonsAndActionsReachable() {
        FakePort port; ControllersModel model(port); Q_EMIT port.snapshotReceived(snapshot());
        bool touch = false, dictate = false, gyro = false;
        for (const auto &row : model.buttons()) touch |= row.toMap().value("id").toString() == "touchpad";
        for (const auto &row : model.actions()) dictate |= row.toMap().value("value").toString() == "dictate";
        gyro = model.setOption("gyro", true);
        QVERIFY(touch); QVERIFY(dictate); QVERIFY(gyro);
    }
};
QTEST_MAIN(ControllerSettingsTest)
#include "tst_controller_settings.moc"
