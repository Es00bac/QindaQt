// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_devices_model.h>

#include <QSignalSpy>
#include <QTest>

using QindaQt::Apps::SettingsInput::PointerDeviceSnapshot;
using QindaQt::Apps::SettingsInput::PointerDevicePort;
using QindaQt::Apps::SettingsInput::PointerDeviceSelection;
using QindaQt::Apps::SettingsInput::PointerDevicesModel;

namespace {

// In-process fake port: scripted devices plus a write script that fails
// selected properties, so the model's revert path is testable.
class FakePointerPort final : public PointerDevicePort
{
public:
    QList<PointerDeviceSnapshot> scripted;
    bool authorityPresent = true;
    QString nextWriteFailure;
    QList<QPair<QString, QString>> writtenProperties;

    QList<PointerDeviceSnapshot> devices(QString *error) const override
    {
        if (!authorityPresent) {
            if (error != nullptr) {
                *error = QStringLiteral("authority absent");
            }
            return {};
        }
        return scripted;
    }

    bool writeProperty(const QString &deviceId, const QString &property,
                       const QVariant &value, QString *error) const override
    {
        if (!nextWriteFailure.isEmpty()) {
            if (error != nullptr) {
                *error = nextWriteFailure;
            }
            return false;
        }
        mutableWritten().append({deviceId, property});
        Q_UNUSED(value);
        return true;
    }

    QList<QPair<QString, QString>> &mutableWritten() const
    {
        return m_written;
    }

private:
    mutable QList<QPair<QString, QString>> m_written;
};

PointerDeviceSnapshot mouseSnapshot()
{
    PointerDeviceSnapshot snapshot;
    snapshot.deviceId = QStringLiteral("event5");
    snapshot.name = QStringLiteral("Fake Mouse");
    snapshot.pointer = true;
    snapshot.properties = QVariantMap{
        {QStringLiteral("pointerAcceleration"), 0.0},
        {QStringLiteral("pointerAccelerationProfileFlat"), false},
        {QStringLiteral("pointerAccelerationProfileAdaptive"), true},
        {QStringLiteral("supportsPointerAcceleration"), true},
        {QStringLiteral("supportsPointerAccelerationProfileFlat"), true},
        {QStringLiteral("supportsPointerAccelerationProfileAdaptive"), true},
        {QStringLiteral("supportsNaturalScroll"), true},
        {QStringLiteral("supportsLeftHanded"), true},
        {QStringLiteral("supportsMiddleEmulation"), true},
        {QStringLiteral("naturalScroll"), false},
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("middleEmulation"), false},
        {QStringLiteral("scrollFactor"), 1.0},
    };
    return snapshot;
}

PointerDeviceSnapshot touchpadSnapshot()
{
    PointerDeviceSnapshot snapshot;
    snapshot.deviceId = QStringLiteral("event9");
    snapshot.name = QStringLiteral("Fake Touchpad");
    snapshot.touchpad = true;
    snapshot.properties = QVariantMap{
        {QStringLiteral("supportsTapToClick"), true},
        {QStringLiteral("tapToClick"), true},
        {QStringLiteral("supportsDisableWhileTyping"), false},
    };
    return snapshot;
}

} // namespace

class PointerDevicesModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void authorityLossIsDegradedNotEmpty();
    void emptyListWithoutErrorMeansNoDevices();
    void rowsExposeIdentityAndKind();
    void selectionExposesAvailabilityTruth();
    void selectedIndexTracksSelectionAcrossRefresh();
    void unsupportedControlIsHiddenNotDisabled();
    void failedWriteRevertsAndReports();
    void successKeepsThePresentedValue();

private:
    FakePointerPort m_port;
};

void PointerDevicesModelTest::authorityLossIsDegradedNotEmpty()
{
    FakePointerPort port;
    port.authorityPresent = false;
    PointerDevicesModel model(port);
    model.refresh();
    QVERIFY(!model.available());
    QCOMPARE(model.count(), 0);
}

void PointerDevicesModelTest::emptyListWithoutErrorMeansNoDevices()
{
    FakePointerPort port;
    PointerDevicesModel model(port);
    model.refresh();
    QVERIFY(model.available());
    QVERIFY(model.empty());
}

void PointerDevicesModelTest::rowsExposeIdentityAndKind()
{
    FakePointerPort port;
    port.scripted.append(mouseSnapshot());
    port.scripted.append(touchpadSnapshot());
    PointerDevicesModel model(port);
    model.refresh();
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.index(0, 0).data(PointerDevicesModel::DeviceIdRole),
             QStringLiteral("event5"));
    QCOMPARE(model.index(0, 0).data(PointerDevicesModel::LabelRole),
             QStringLiteral("Fake Mouse"));
    QCOMPARE(model.index(0, 0).data(PointerDevicesModel::IsTouchpadRole),
             false);
    QCOMPARE(model.index(1, 0).data(PointerDevicesModel::IsTouchpadRole),
             true);
}

void PointerDevicesModelTest::selectionExposesAvailabilityTruth()
{
    FakePointerPort port;
    port.scripted.append(touchpadSnapshot());
    PointerDevicesModel model(port);
    model.refresh();
    // The first row is selected automatically.
    QVERIFY(model.selection() != nullptr);
    // Supports flag true -> visible.
    QVERIFY(model.selection()->tapToClickAvailable());
    // Supports flag false -> hidden even though a live value rode along.
    QVERIFY(!model.selection()->disableWhileTypingAvailable());
    // Touchpad-only rows are unavailable on a plain pointer.
    port.scripted.clear();
    port.scripted.append(mouseSnapshot());
    PointerDevicesModel mouseModel(port);
    mouseModel.refresh();
    QVERIFY(!mouseModel.selection()->tapToClickAvailable());
}

void PointerDevicesModelTest::selectedIndexTracksSelectionAcrossRefresh()
{
    // The route's device picker binds to selectedIndex: the model must
    // publish it on the automatic first selection, on explicit select(), and
    // when a refresh drops the selected row.
    FakePointerPort port;
    port.scripted.append(mouseSnapshot());
    port.scripted.append(touchpadSnapshot());
    PointerDevicesModel model(port);
    QCOMPARE(model.selectedIndex(), -1);
    model.refresh();
    QCOMPARE(model.selectedIndex(), 0);

    model.select(1);
    QCOMPARE(model.selectedIndex(), 1);

    // The selected touchpad disappears; the selection falls back to row 0.
    QSignalSpy spy(&model, &PointerDevicesModel::selectedIndexChanged);
    QVERIFY(spy.isValid());
    port.scripted.removeAt(1);
    model.refresh();
    QCOMPARE(model.selectedIndex(), 0);
    QCOMPARE(spy.count(), 1);

    // Every device vanishes: no selection at all.
    port.scripted.clear();
    model.refresh();
    QCOMPARE(model.selectedIndex(), -1);
}

void PointerDevicesModelTest::unsupportedControlIsHiddenNotDisabled()
{
    // The mouse snapshot lacks tapToClick support truth entirely.
    FakePointerPort port;
    port.scripted.append(mouseSnapshot());
    PointerDevicesModel model(port);
    model.refresh();
    QVERIFY(!model.selection()->tapToClickAvailable());
}

void PointerDevicesModelTest::failedWriteRevertsAndReports()
{
    FakePointerPort port;
    port.scripted.append(mouseSnapshot());
    PointerDevicesModel model(port);
    model.refresh();
    PointerDeviceSelection *selection = model.selection();
    port.nextWriteFailure = QStringLiteral("authority refused");
    selection->setNaturalScroll(true);
    QCOMPARE(selection->naturalScroll(), false);
    QVERIFY(selection->statusText().contains(QStringLiteral("refused")));
    QCOMPARE(port.mutableWritten().size(), 0);
}

void PointerDevicesModelTest::successKeepsThePresentedValue()
{
    FakePointerPort port;
    port.scripted.append(mouseSnapshot());
    PointerDevicesModel model(port);
    model.refresh();
    PointerDeviceSelection *selection = model.selection();
    selection->setNaturalScroll(true);
    QCOMPARE(selection->naturalScroll(), true);
    QVERIFY(selection->statusText().isEmpty());
    QCOMPARE(port.mutableWritten().size(), 1);
    QCOMPARE(port.mutableWritten().first().second,
             QStringLiteral("naturalScroll"));

    // A failed speed write keeps the old speed.
    port.nextWriteFailure = QStringLiteral("nope");
    const double before = selection->speed();
    selection->setSpeed(before + 0.5);
    QCOMPARE(selection->speed(), before);
}

QTEST_MAIN(PointerDevicesModelTest)
#include "tst_pointer_devices_model.moc"
