// SPDX-License-Identifier: GPL-3.0-or-later
#include "misc_policy.h"
#include "print_policy.h"
#include <QtTest>
using namespace QindaQt::Services::Portal;
class MiscPolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void usbSubsetIsBoundToOfferedAccess() {
        const UsbDevices offered{{"one", QVariantMap{{"properties", QVariantMap{{"ID_MODEL", "Fixture"}}}}, QVariantMap{{"writable", true}}}, {"two", {}, {}}};
        QVERIFY(usbDevicesFrame(offered));
        const auto result = usbResults(offered, QJsonObject{{"devices", QJsonArray{"two"}}}); QVERIFY(result); QCOMPARE(result->size(), 1); QCOMPARE(result->first().first, QStringLiteral("two")); QCOMPARE(result->first().second.value("writable"), QVariant(false));
        QVERIFY(!usbResults(offered, QJsonObject{{"devices", QJsonArray{"foreign"}}}));
        QVERIFY(!usbResults(offered, QJsonObject{{"devices", QJsonArray{"one", "one"}}}));
        QVERIFY(!usbDevicesFrame(UsbDevices{{"same", {}, {}}, {"same", {}, {}}}));
        QVERIFY(!usbDevicesFrame(UsbDevices{{"bad", {}, QVariantMap{{"writable", QStringLiteral("true")}}}}));
    }
    void launcherPreservesSerializedIconAndEditablePolicy() {
        const QByteArray bytes("fixture-icon"); const QDBusVariant icon(QVariant::fromValue(LauncherIcon{"bytes", QDBusVariant(bytes)}));
        const auto frame = launcherFrame("org.test.App", {}, "Fixture", icon, QVariantMap{{"editable_name", false}}); QVERIFY(frame);
        const auto result = launcherResults(*frame, QJsonObject{{"name", "Fixture"}}); QVERIFY(result);
        const auto returned = launcherIcon(result->value("icon").value<QDBusVariant>()); QVERIFY(returned); QCOMPARE(returned->data.variant().toByteArray(), bytes);
        QVERIFY(!launcherResults(*frame, QJsonObject{{"name", "Changed"}}));
        QVERIFY(!launcherFrame({}, {}, "Fixture", icon, QVariantMap{{"launcher_type", 4U}}));
        QVERIFY(!launcherFrame({}, {}, "Fixture", icon, QVariantMap{{"editable_icon", "yes"}}));
        QVERIFY(noninteractiveLauncherAllowed("org.gnome.Software")); QVERIFY(!noninteractiveLauncherAllowed("org.test.App")); QVERIFY(!noninteractiveLauncherAllowed({}));
    }
    void strictKnownTypesAndBounds() {
        QVERIFY(!miscFrame("account", {}, "x11:123", "Info", {}));
        QVERIFY(!miscFrame("account", {}, {}, "Info", QVariantMap{{"modal", "true"}}));
        QVERIFY(!validPrintMaps(QVariantMap{{"n-copies", "0"}}, {}));
        QVERIFY(!validPrintMaps(QVariantMap{{"resolution", 600U}}, {}));
        QVERIFY(!validPrintMaps({}, QVariantMap{{"Width", -1.0}}));
        QVERIFY(validPrintMaps(QVariantMap{{"n-copies", "2"}}, QVariantMap{{"Width", 210.0}, {"Height", 297.0}}));
    }
};
QTEST_GUILESS_MAIN(MiscPolicyTest)
#include "tst_misc_policy.moc"
