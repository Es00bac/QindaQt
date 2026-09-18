// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/kwin_tablet_devices.h>

#include <QRectF>
#include <QSignalSpy>
#include <QTest>

#include "support/fake_kwin_tablets.h"
#include "support/private_bus.h"

using QindaQt::Services::TabletDevices::KWinTabletDevicePort;
using QindaQt::Services::TabletDevices::KWinTabletDeviceWatcher;
using QindaQt::Services::TabletDevices::TabletDeviceSnapshot;
using QindaQt::Tests::FakeKWinTabletManager;
using QindaQt::Tests::PrivateBus;

class TabletDevicePortTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void failsClosedWithoutAuthority();
    void listsOnlyTabletsAndCarriesTheirProperties();
    void writesTypedPropertiesIncludingAreas();
    void rejectsUnknownNamesAndWrongTypes();
    void hotplugSignalsReachTheWatcher();

private:
    static QVariantMap penSpec(const QString &id) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QStringLiteral("Wacom One Pen Display 13 Pen")},
            {QStringLiteral("deviceGroupId"), QStringLiteral("group-wacom")},
            {QStringLiteral("vendor"), 1386u},
            {QStringLiteral("product"), 934u},
            {QStringLiteral("tabletTool"), true},
            {QStringLiteral("tabletPad"), false},
            {QStringLiteral("widthMm"), 294.0},
            {QStringLiteral("heightMm"), 166.0},
            {QStringLiteral("supportsDisableEvents"), true},
            {QStringLiteral("supportsCalibrationMatrix"), true},
            {QStringLiteral("supportsRotation"), true},
            {QStringLiteral("supportsLeftHanded"), true},
            {QStringLiteral("supportsPressureRange"), true},
        };
    }
    static QVariantMap mouseSpec(const QString &id) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QStringLiteral("Plain Mouse")},
            {QStringLiteral("tabletTool"), false},
            {QStringLiteral("tabletPad"), false},
            {QStringLiteral("pointer"), true},
        };
    }
};

void TabletDevicePortTest::failsClosedWithoutAuthority() {
    PrivateBus bus;
    QVERIFY(bus.start());
    // No fake service registered on this private bus at all.
    KWinTabletDevicePort port(bus.connection);
    QString error;
    const QList<TabletDeviceSnapshot> devices = port.devices(&error);
    QVERIFY(devices.isEmpty());
    QVERIFY(!error.isEmpty());

    QString writeError;
    QVERIFY(!port.writeProperty(QStringLiteral("event9"),
                                QStringLiteral("outputName"),
                                QStringLiteral("HDMI-A-1"), &writeError));
    QVERIFY(!writeError.isEmpty());
}

void TabletDevicePortTest::listsOnlyTabletsAndCarriesTheirProperties() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinTabletManager manager;
    manager.addDevice(mouseSpec(QStringLiteral("event3")));
    manager.addDevice(penSpec(QStringLiteral("event19")));
    QVariantMap pad = penSpec(QStringLiteral("event20"));
    pad.insert(QStringLiteral("name"), QStringLiteral("Wacom One Pen Display 13 Pad"));
    pad.insert(QStringLiteral("tabletTool"), false);
    pad.insert(QStringLiteral("tabletPad"), true);
    pad.insert(QStringLiteral("padButtons"), 4u);
    manager.addDevice(pad);
    QVERIFY(manager.publish(bus.connection));

    KWinTabletDevicePort port(bus.connection);
    QString error;
    const QList<TabletDeviceSnapshot> devices = port.devices(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // The plain mouse is not this port's business and must not appear.
    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices.at(0).deviceId, QStringLiteral("event19"));
    QVERIFY(devices.at(0).tabletTool);
    QVERIFY(!devices.at(0).tabletPad);
    QCOMPARE(devices.at(0).deviceGroupId, QStringLiteral("group-wacom"));
    QCOMPARE(devices.at(0).vendorId, 1386u);
    QCOMPARE(devices.at(0).productId, 934u);
    // The `(dd)` size struct arrives flattened, never as a transport type.
    QVERIFY(qFuzzyCompare(devices.at(0).widthMillimeters(), 294.0));
    QVERIFY(qFuzzyCompare(devices.at(0).heightMillimeters(), 166.0));
    QVERIFY(devices.at(0).hasPhysicalSize());
    QVERIFY(devices.at(0).properties.value(QStringLiteral("supportsCalibrationMatrix")).toBool());
    // And the `(dddd)` area struct likewise.
    const QVariantList area =
        devices.at(0).properties.value(QStringLiteral("outputArea")).toList();
    QCOMPARE(area.size(), 4);
    QVERIFY(qFuzzyCompare(area.at(2).toDouble(), 1.0));

    QCOMPARE(devices.at(1).deviceId, QStringLiteral("event20"));
    QVERIFY(devices.at(1).tabletPad);
    QCOMPARE(devices.at(1)
                 .properties.value(QStringLiteral("tabletPadButtonCount"))
                 .toUInt(),
             4u);
}

void TabletDevicePortTest::writesTypedPropertiesIncludingAreas() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinTabletManager manager;
    auto *pen = manager.addDevice(penSpec(QStringLiteral("event19")));
    QVERIFY(manager.publish(bus.connection));

    KWinTabletDevicePort port(bus.connection);
    QString error;
    QVERIFY2(port.writeProperty(QStringLiteral("event19"),
                                QStringLiteral("outputName"),
                                QStringLiteral("HDMI-A-1"), &error),
             qPrintable(error));
    QVERIFY(port.writeProperty(QStringLiteral("event19"),
                               QStringLiteral("rotation"),
                               QVariant::fromValue(90u), &error));
    QVERIFY(port.writeProperty(QStringLiteral("event19"),
                               QStringLiteral("pressureRangeMin"), 0.2, &error));
    // The area goes out as the QRectF KWin's property declares, which is the
    // only encoding it demarshals from the `(dddd)` signature.
    QVERIFY2(port.writeProperty(QStringLiteral("event19"),
                                QStringLiteral("outputArea"),
                                QVariantList{0.1, 0.2, 0.5, 0.25}, &error),
             qPrintable(error));

    QCOMPARE(pen->outputName(), QStringLiteral("HDMI-A-1"));
    QCOMPARE(pen->rotation(), 90u);
    QVERIFY(qFuzzyCompare(pen->pressureRangeMin(), 0.2));
    QCOMPARE(pen->outputArea(), QRectF(0.1, 0.2, 0.5, 0.25));
    QCOMPARE(pen->writes.size(), 4);
}

void TabletDevicePortTest::rejectsUnknownNamesAndWrongTypes() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinTabletManager manager;
    auto *pen = manager.addDevice(penSpec(QStringLiteral("event19")));
    QVERIFY(manager.publish(bus.connection));

    KWinTabletDevicePort port(bus.connection);
    QString error;
    // A name outside the closed writable table never reaches D-Bus.
    QVERIFY(!port.writeProperty(QStringLiteral("event19"),
                                QStringLiteral("pointerAcceleration"), 0.5,
                                &error));
    QVERIFY(!error.isEmpty());
    // Wrong value type for a writable name.
    QVERIFY(!port.writeProperty(QStringLiteral("event19"),
                                QStringLiteral("outputName"), 7, &error));
    // An area outside the unit square is refused before the call.
    QVERIFY(!port.writeProperty(QStringLiteral("event19"),
                                QStringLiteral("outputArea"),
                                QVariantList{0.5, 0.0, 0.8, 1.0}, &error));
    // A path-escaping device id is refused.
    QVERIFY(!port.writeProperty(QStringLiteral("../../evil"),
                                QStringLiteral("outputName"),
                                QStringLiteral("HDMI-A-1"), &error));
    QCOMPARE(pen->writes.size(), 0);
}

void TabletDevicePortTest::hotplugSignalsReachTheWatcher() {
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinTabletManager manager;
    QVERIFY(manager.publish(bus.connection));

    KWinTabletDeviceWatcher watcher(bus.connection);
    QString error;
    QVERIFY2(watcher.start(&error), qPrintable(error));
    QSignalSpy added(&watcher, &KWinTabletDeviceWatcher::deviceAdded);
    QSignalSpy removed(&watcher, &KWinTabletDeviceWatcher::deviceRemoved);

    manager.addDevice(penSpec(QStringLiteral("event19")));
    QTRY_COMPARE(added.size(), 1);
    QCOMPARE(added.at(0).at(0).toString(), QStringLiteral("event19"));

    manager.removeDevice(QStringLiteral("event19"));
    QTRY_COMPARE(removed.size(), 1);
    QCOMPARE(removed.at(0).at(0).toString(), QStringLiteral("event19"));
}

QTEST_MAIN(TabletDevicePortTest)
#include "tst_tablet_device_port.moc"
