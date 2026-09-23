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
    mutable QList<PointerDeviceSnapshot> scripted;
    bool authorityPresent = true;
    mutable int reads = 0;
    int observationStarts = 0;
    void setObserving(bool active) override {
        if (active) ++observationStarts;
    }
    QString nextWriteFailure;
    QString failProperty;
    bool ignoreWrites = false;
    QString ignoreProperty;
    QList<QPair<QString, QString>> writtenProperties;
    void notifyInventory() { Q_EMIT inventoryChanged(); }
    void notifyOwnerChange() { Q_EMIT authorityChanged(); }

    QList<PointerDeviceSnapshot> devices(QString *error) const override
    {
        ++reads;
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
        if (!nextWriteFailure.isEmpty() &&
            (failProperty.isEmpty() || failProperty == property)) {
            if (error != nullptr) {
                *error = nextWriteFailure;
            }
            return false;
        }
        for (auto &snapshot : scripted) {
            if (snapshot.deviceId != deviceId) continue;
            if (!ignoreWrites && ignoreProperty != property)
                snapshot.properties.insert(property, value);
            mutableWritten().append({deviceId, property});
            return true;
        }
        if (error) *error = QStringLiteral("device absent");
        return false;
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
        // Real KWin reports tap capability as an integer finger count, not a
        // boolean supports* flag (verified against the live qinda-top
        // touchpad, event4: tapFingerCount == 3).
        {QStringLiteral("tapFingerCount"), 3},
        {QStringLiteral("tapToClick"), true},
        {QStringLiteral("supportsDisableWhileTyping"), false},
    };
    return snapshot;
}

class DeferredWritePort final : public PointerDevicePort {
public:
    QList<PointerDeviceSnapshot> scripted;
    mutable WriteReply pending;
    QList<PointerDeviceSnapshot> devices(QString *error) const override {
        if (error) error->clear();
        return scripted;
    }
    bool writeProperty(const QString &, const QString &, const QVariant &,
                       QString *) const override { return false; }
    void requestWrite(QObject *, const QString &,
                      const QList<QPair<QString, QVariant>> &,
                      WriteReply reply) const override {
        pending = std::move(reply);
    }
    void complete(const PointerDeviceSnapshot &snapshot) {
        WriteResult result;
        result.applied = true;
        result.snapshot = snapshot;
        auto reply = std::move(pending);
        reply(std::move(result));
    }
    void replaceOwner() { Q_EMIT authorityChanged(); }
};

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
    void switchingDevicesUsesConfirmedReadback();
    void refreshPreservesSelectedIdentityAcrossReorder();
    void unplugClearsCapabilitiesAndSelection();
    void externalChangeRefreshesActiveSelection();
    void ownerReplacementFencesAndClearsSelection();
    void acceptedButIgnoredWriteReportsAndRestores();
    void lateReplyNeverPaintsDifferentSelectionOrOwner();
    void constructionIsLazyAndHiddenTabDoesNotPoll();
    void rejectedSecondPropertyRestoresFirst();
    void ignoredSecondPropertyRestoresFirst();

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
    // tapFingerCount > 0 -> tap rows visible.
    QVERIFY(model.selection()->tapToClickAvailable());
    QVERIFY(model.selection()->tapAndDragAvailable());
    // Supports flag false -> hidden even though a live value rode along.
    QVERIFY(!model.selection()->disableWhileTypingAvailable());

    // A touchpad reporting tapFingerCount == 0 cannot tap at all.
    PointerDeviceSnapshot noTap = touchpadSnapshot();
    noTap.properties[QStringLiteral("tapFingerCount")] = 0;
    port.scripted.clear();
    port.scripted.append(noTap);
    PointerDevicesModel noTapModel(port);
    noTapModel.refresh();
    QVERIFY(!noTapModel.selection()->tapToClickAvailable());
    QVERIFY(!noTapModel.selection()->tapAndDragAvailable());

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

void PointerDevicesModelTest::switchingDevicesUsesConfirmedReadback()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot(), touchpadSnapshot()};
    PointerDevicesModel model(port);
    model.refresh();
    model.selection()->setSpeed(0.65);
    QCOMPARE(model.selection()->speed(), 0.65);
    model.select(1);
    model.select(0);
    QCOMPARE(model.selection()->speed(), 0.65);
    QCOMPARE(port.scripted.at(0).properties.value(
                 QStringLiteral("pointerAcceleration")).toDouble(), 0.65);
}

void PointerDevicesModelTest::refreshPreservesSelectedIdentityAcrossReorder()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot(), touchpadSnapshot()};
    PointerDevicesModel model(port);
    model.refresh();
    model.select(1);
    std::swap(port.scripted[0], port.scripted[1]);
    model.refresh();
    QCOMPARE(model.selectedIndex(), 0);
    QVERIFY(model.selection()->tapToClickAvailable());
}

void PointerDevicesModelTest::unplugClearsCapabilitiesAndSelection()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    PointerDevicesModel model(port);
    model.refresh();
    QVERIFY(model.selection()->speedAvailable());
    port.scripted.clear();
    model.refresh();
    QCOMPARE(model.selectedIndex(), -1);
    QVERIFY(!model.selection()->speedAvailable());
    QVERIFY(!model.selection()->naturalScrollAvailable());
}

void PointerDevicesModelTest::externalChangeRefreshesActiveSelection()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    PointerDevicesModel model(port);
    model.setActive(true);
    port.scripted[0].properties.insert(QStringLiteral("naturalScroll"), true);
    port.notifyInventory();
    QVERIFY(model.selection()->naturalScroll());
}

void PointerDevicesModelTest::ownerReplacementFencesAndClearsSelection()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    PointerDevicesModel model(port);
    model.refresh();
    port.notifyOwnerChange();
    QCOMPARE(model.selectedIndex(), -1);
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.available());
    QVERIFY(!model.selection()->speedAvailable());
}

void PointerDevicesModelTest::acceptedButIgnoredWriteReportsAndRestores()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    port.ignoreWrites = true;
    PointerDevicesModel model(port);
    model.refresh();
    model.selection()->setNaturalScroll(true);
    QVERIFY(!model.selection()->naturalScroll());
    QVERIFY(model.selection()->statusText().contains(
        QStringLiteral("did not retain")));
}

void PointerDevicesModelTest::lateReplyNeverPaintsDifferentSelectionOrOwner()
{
    DeferredWritePort port;
    port.scripted = {mouseSnapshot(), touchpadSnapshot()};
    PointerDevicesModel model(port);
    model.setActive(true);
    model.selection()->setSpeed(0.65);
    QVERIFY(model.selection()->busy());
    model.select(1);
    PointerDeviceSnapshot changed = mouseSnapshot();
    changed.properties.insert(QStringLiteral("pointerAcceleration"), 0.65);
    port.scripted[0] = changed;
    port.complete(changed);
    QVERIFY(model.selection()->tapToClickAvailable());
    model.select(0);
    QCOMPARE(model.selection()->speed(), 0.65);

    model.selection()->setSpeed(0.3);
    QVERIFY(model.selection()->busy());
    const PointerDeviceSnapshot oldOwner = changed;
    port.scripted = {touchpadSnapshot()};
    port.replaceOwner();
    QCOMPARE(model.count(), 1);
    QVERIFY(model.selection()->tapToClickAvailable());
    port.complete(oldOwner);
    QCOMPARE(model.count(), 1);
    QVERIFY(model.selection()->tapToClickAvailable());
    QCOMPARE(model.index(0, 0).data(PointerDevicesModel::DeviceIdRole),
             QStringLiteral("event9"));
}

void PointerDevicesModelTest::constructionIsLazyAndHiddenTabDoesNotPoll()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    PointerDevicesModel model(port);
    QCOMPARE(port.reads, 0);
    QCOMPARE(port.observationStarts, 0);
    model.setActive(true);
    QCOMPARE(port.reads, 1);
    QCOMPARE(port.observationStarts, 1);
    model.setActive(false);
    port.notifyInventory();
    QCOMPARE(port.reads, 1);
    port.notifyOwnerChange();
    QCOMPARE(port.reads, 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.available());
    QVERIFY(!model.selection()->speedAvailable());
}

void PointerDevicesModelTest::rejectedSecondPropertyRestoresFirst()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    port.nextWriteFailure = QStringLiteral("refused");
    port.failProperty = QStringLiteral(
        "pointerAccelerationProfileAdaptive");
    PointerDevicesModel model(port);
    model.refresh();
    model.selection()->setFlatProfile(true);
    QVERIFY(!model.selection()->flatProfile());
    QVERIFY(model.selection()->statusText().contains(
        QStringLiteral("refused")));
    const auto &properties = port.scripted.first().properties;
    QCOMPARE(properties.value(QStringLiteral(
                 "pointerAccelerationProfileFlat")).toBool(), false);
    QCOMPARE(properties.value(QStringLiteral(
                 "pointerAccelerationProfileAdaptive")).toBool(), true);
}

void PointerDevicesModelTest::ignoredSecondPropertyRestoresFirst()
{
    FakePointerPort port;
    port.scripted = {mouseSnapshot()};
    port.ignoreProperty = QStringLiteral(
        "pointerAccelerationProfileAdaptive");
    PointerDevicesModel model(port);
    model.refresh();
    model.selection()->setFlatProfile(true);
    QVERIFY(!model.selection()->flatProfile());
    QVERIFY(model.selection()->statusText().contains(
        QStringLiteral("did not retain")));
    const auto &properties = port.scripted.first().properties;
    QCOMPARE(properties.value(QStringLiteral(
                 "pointerAccelerationProfileFlat")).toBool(), false);
    QCOMPARE(properties.value(QStringLiteral(
                 "pointerAccelerationProfileAdaptive")).toBool(), true);
}

QTEST_MAIN(PointerDevicesModelTest)
#include "tst_pointer_devices_model.moc"
