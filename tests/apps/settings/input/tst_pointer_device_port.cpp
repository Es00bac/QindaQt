// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_device_port.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QTest>

#include "support/fake_kwin_input.h"
#include "support/private_bus.h"

using QindaQt::Apps::SettingsInput::KWinPointerDevicePort;
using QindaQt::Apps::SettingsInput::PointerDeviceSnapshot;
using QindaQt::Tests::FakeKWinInput;
using QindaQt::Tests::PrivateBus;

// A manager whose ListPointers returns a plain string instead of a string
// list, registered under the real manager interface so the port exercises
// its reply-shape guard. Q_OBJECT classes cannot live in function scope.
class MalformedManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDeviceManager")
public Q_SLOTS:
    Q_SCRIPTABLE QString ListPointers() { return QStringLiteral("garbage"); }
    Q_SCRIPTABLE QStringList ListTouch() { return {}; }
    Q_SCRIPTABLE QStringList ListKeyboards() { return {}; }
};

class PointerDevicePortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void failsClosedWithoutAuthority();
    void listsPointersAndTouchpads();
    void writesTypedProperties();
    void rejectsUnknownNamesAndTypes();
    void rejectsAbsentDevices();
    void malformedListReplyFailsClosed();

private:
    QVariantMap mouseSpec(const QString &id) const
    {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QStringLiteral("Fake Mouse")},
            {QStringLiteral("pointer"), true},
            {QStringLiteral("supportsLeftHanded"), true},
            {QStringLiteral("supportsNaturalScroll"), true},
            {QStringLiteral("supportsPointerAcceleration"), true},
            {QStringLiteral("supportsPointerAccelerationProfileFlat"), true},
            {QStringLiteral("supportsPointerAccelerationProfileAdaptive"),
             true},
            {QStringLiteral("supportsMiddleEmulation"), true},
        };
    }
    QVariantMap touchpadSpec(const QString &id) const
    {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QStringLiteral("Fake Touchpad")},
            {QStringLiteral("touchpad"), true},
            {QStringLiteral("supportsTapToClick"), true},
            {QStringLiteral("supportsTapAndDrag"), true},
            {QStringLiteral("supportsDisableWhileTyping"), true},
            {QStringLiteral("supportsScrollTwoFinger"), true},
            {QStringLiteral("supportsScrollEdge"), true},
        };
    }
};

void PointerDevicePortTest::failsClosedWithoutAuthority()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    // No fake service registered on this private bus at all.
    KWinPointerDevicePort port(bus.connection);
    QString error;
    const QList<PointerDeviceSnapshot> devices = port.devices(&error);
    QVERIFY(devices.isEmpty());
    QVERIFY(!error.isEmpty());

    QString writeError;
    QVERIFY(!port.writeProperty(QStringLiteral("event9"),
                                QStringLiteral("naturalScroll"), true,
                                &writeError));
    QVERIFY(!writeError.isEmpty());
}

void PointerDevicePortTest::listsPointersAndTouchpads()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinInput fake;
    fake.addDevice(mouseSpec(QStringLiteral("event5")));
    fake.addDevice(touchpadSpec(QStringLiteral("event9")));
    fake.addDevice(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("event1")},
        {QStringLiteral("name"), QStringLiteral("Power Button")},
        {QStringLiteral("keyboard"), true},
    });
    QVERIFY(fake.publish(bus.connection));

    KWinPointerDevicePort port(bus.connection);
    QString error;
    const QList<PointerDeviceSnapshot> devices = port.devices(&error);
    QCOMPARE(error, QString());
    QCOMPARE(devices.size(), 2);
    const auto byId = [](const QList<PointerDeviceSnapshot> &snapshots,
                         const QString &id) {
        for (const auto &device : snapshots) {
            if (device.deviceId == id) {
                return device;
            }
        }
        return PointerDeviceSnapshot{};
    };
    const PointerDeviceSnapshot mouse = byId(devices, QStringLiteral("event5"));
    QVERIFY(mouse.pointer);
    QVERIFY(!mouse.touchpad);
    QCOMPARE(mouse.name, QStringLiteral("Fake Mouse"));
    QCOMPARE(mouse.properties.value(QStringLiteral("supportsLeftHanded"))
                 .toBool(),
             true);
    const PointerDeviceSnapshot touchpad =
        byId(devices, QStringLiteral("event9"));
    QVERIFY(touchpad.touchpad);
    QCOMPARE(touchpad.properties.value(QStringLiteral("supportsTapToClick"))
                 .toBool(),
             true);
    // A keyboard-only device never becomes a pointer row.
    QVERIFY(byId(devices, QStringLiteral("event1")).deviceId.isEmpty());
}

void PointerDevicePortTest::writesTypedProperties()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinInput fake;
    auto *mouse = fake.addDevice(mouseSpec(QStringLiteral("event5")));
    auto *touchpad = fake.addDevice(touchpadSpec(QStringLiteral("event9")));
    QVERIFY(fake.publish(bus.connection));

    KWinPointerDevicePort port(bus.connection);
    QString error;
    QVERIFY(port.writeProperty(QStringLiteral("event5"),
                               QStringLiteral("naturalScroll"), true,
                               &error));
    QCOMPARE(error, QString());
    QCOMPARE(mouse->writes.size(), 1);
    QCOMPARE(mouse->writes.first().first, QStringLiteral("naturalScroll"));
    QCOMPARE(mouse->writes.first().second.toBool(), true);

    QVERIFY(port.writeProperty(QStringLiteral("event5"),
                               QStringLiteral("pointerAcceleration"), 0.75,
                               &error));
    QCOMPARE(mouse->writes.last().second.toDouble(), 0.75);

    QVERIFY(port.writeProperty(QStringLiteral("event9"),
                               QStringLiteral("tapToClick"), false, &error));
    QCOMPARE(touchpad->writes.first().first, QStringLiteral("tapToClick"));
}

void PointerDevicePortTest::rejectsUnknownNamesAndTypes()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinInput fake;
    fake.addDevice(mouseSpec(QStringLiteral("event5")));
    QVERIFY(fake.publish(bus.connection));

    KWinPointerDevicePort port(bus.connection);
    QString error;
    // Not a member of the closed writable table.
    QVERIFY(!port.writeProperty(QStringLiteral("event5"),
                                QStringLiteral("supportedButtons"), true,
                                &error));
    QVERIFY(error.contains(QStringLiteral("Unknown pointer property")));
    // Boolean property with a double value.
    QVERIFY(!port.writeProperty(QStringLiteral("event5"),
                                QStringLiteral("naturalScroll"), 1.5,
                                &error));
    QVERIFY(!error.isEmpty());
    // Double property with a boolean value.
    QVERIFY(!port.writeProperty(QStringLiteral("event5"),
                                QStringLiteral("scrollFactor"), true,
                                &error));
    QVERIFY(!error.isEmpty());
    // Nothing reached the fake.
    QCOMPARE(fake.deviceAt(0)->writes.size(), 0);
}

void PointerDevicePortTest::rejectsAbsentDevices()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWinInput fake;
    fake.addDevice(mouseSpec(QStringLiteral("event5")));
    QVERIFY(fake.publish(bus.connection));

    KWinPointerDevicePort port(bus.connection);
    QString error;
    QVERIFY(!port.writeProperty(QStringLiteral("event-does-not-exist"),
                                QStringLiteral("naturalScroll"), true,
                                &error));
    QVERIFY(!error.isEmpty());
    // Path traversal attempts are refused before any call is made.
    QVERIFY(!port.writeProperty(QStringLiteral("event5/../../../evil"),
                                QStringLiteral("naturalScroll"), true,
                                &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fake.deviceAt(0)->writes.size(), 0);
}

void PointerDevicePortTest::malformedListReplyFailsClosed()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    MalformedManager malformed;
    QVERIFY(bus.connection.registerService(QStringLiteral("org.kde.KWin")));
    QVERIFY(bus.connection.registerObject(
        QStringLiteral("/org/kde/KWin/InputDevice"), &malformed,
        QDBusConnection::ExportAllContents));

    KWinPointerDevicePort port(bus.connection);
    QString error;
    const QList<PointerDeviceSnapshot> devices = port.devices(&error);
    QVERIFY(devices.isEmpty());
    QVERIFY(error.contains(QStringLiteral("malformed")));
}

QTEST_MAIN(PointerDevicePortTest)
#include "tst_pointer_device_port.moc"
