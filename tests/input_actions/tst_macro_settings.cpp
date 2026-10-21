// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/input_macros_model.h>
#include <qindaqt/input_actions/input_actions.h>
#include <QtTest>
using namespace QindaQt::Apps::SettingsInput;
class Port final : public JsonInputPort {
public:
    QJsonObject patch; QString id; quint64 revision = 0;
    void refresh() override {}
    void apply(const QString &i, const QJsonObject &p, quint64 r) override { id = i; patch = p; revision = r; }
    void reset(const QString &i, quint64 r) override { id = i; revision = r; }
};
class MacroSettingsTest final : public QObject {
    Q_OBJECT
    QJsonObject snapshot(const QJsonObject &macro = {}) {
        QJsonArray rows;
        if (!macro.isEmpty()) rows.append(QJsonObject{{"config", macro}, {"shortcutStatus", "ready"}, {"actualShortcuts", QJsonArray{"Meta+F13", "Meta+F14"}}});
        return {{"schemaVersion", 1}, {"revision", "9"}, {"ready", true}, {"macros", rows}, {"running", ""}};
    }
private Q_SLOTS:
    void presetSaveWaitsForAuthoritativeReadbackAndCarriesExplicitShortcutIntent() {
        Port port; InputMacrosModel model(port); Q_EMIT port.snapshotReceived(snapshot()); QVERIFY(model.available());
        const auto value = model.create("copy-paste"); QVERIFY(model.macroError(value).isEmpty());
        QVERIFY(model.save(value, "", false)); QVERIFY(model.busy()); QVERIFY(model.macros().isEmpty());
        QCOMPARE(port.revision, quint64(9)); QCOMPARE(port.patch.value("assignShortcut").toBool(), false);
        QCOMPARE(port.patch.value("config").toObject().value("steps").toArray().size(), 3);
        Q_EMIT port.snapshotReceived(snapshot(port.patch.value("config").toObject())); Q_EMIT port.completed(true, "ok");
        QVERIFY(!model.busy()); QCOMPARE(model.macros().size(), 1);
        const auto actual = model.macros().first().toMap().value("actualShortcuts").toList(); QCOMPARE(actual.size(), 2);
        auto edited = value; edited["shortcut"] = "Meta+F15";
        const auto before = model.fingerprint(value.value("id").toString());
        QVERIFY(!model.save(edited, "stale", true)); QVERIFY(model.save(edited, before, true)); QVERIFY(port.patch.value("assignShortcut").toBool());
    }
    void ownerLossRejectsEditsAndInvalidRowsDropAuthority() {
        Port port; InputMacrosModel model(port); Q_EMIT port.snapshotReceived(snapshot()); const auto value = model.create("drag");
        QVERIFY(model.macroError(value).isEmpty()); Q_EMIT port.unavailable(); QVERIFY(!model.available()); QVERIFY(!model.save(value, ""));
        auto bad = snapshot(); bad["macros"] = QJsonArray{QJsonObject{{"config", QJsonObject{{"id", "broken"}}}}}; Q_EMIT port.snapshotReceived(bad); QVERIFY(!model.available());
    }
    void duplicateMouseTriggersAreReportedBeforeSubmission() {
        Port port; InputMacrosModel model(port); Q_EMIT port.snapshotReceived(snapshot()); auto a = model.create(); a["mouseButton"] = "back"; a["mouseModifiers"] = QStringList{"Meta"};
        Q_EMIT port.snapshotReceived(snapshot(QJsonObject::fromVariantMap(a))); auto b = model.create("drag"); b["mouseButton"] = "back"; b["mouseModifiers"] = QStringList{"Meta"};
        QVERIFY(model.macroError(b).contains("mouse trigger")); QVERIFY(!model.save(b, ""));
    }
};
QTEST_MAIN(MacroSettingsTest)
#include "tst_macro_settings.moc"
