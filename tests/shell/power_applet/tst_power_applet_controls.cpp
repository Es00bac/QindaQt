// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/power_applet/power_applet_presentation.h>

#include <qindaqt/services/brightness_model/brightness_types.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <QtTest>

#include <utility>

using namespace QindaQt::Shell::PowerApplet;

namespace {

constexpr quint64 kEpoch = 7;

Power::Handle handle(const QString &id) {
  return {.epoch = kEpoch, .opaqueId = id};
}

Power::Snapshot readySnapshot() {
  Power::Snapshot snapshot;
  snapshot.epoch = kEpoch;
  snapshot.revision = 42;
  snapshot.availability = Power::Availability::Ready;
  snapshot.capabilities.setFlag(Power::Capability::Supplies);
  snapshot.composite.present = true;
  snapshot.composite.sourceCount = 1;
  snapshot.composite.percentageKnown = true;
  snapshot.composite.percentage = 63.0;
  return snapshot;
}

Brightness::DisplayControl displayControl(const QString &stableId) {
  Brightness::DisplayControl value;
  value.stableId = stableId;
  value.availability = Brightness::ControlAvailability::Available;
  value.currentKnown = true;
  value.rawCurrent = 400;
  value.rawMaximum = 1000;
  value.normalizedCurrent = 4000;
  return value;
}

Brightness::KeyboardControl keyboardControl(const QString &id) {
  Brightness::KeyboardControl value;
  value.handle = handle(id);
  value.name = QStringLiteral("Lenovo kbd");
  value.availability = Brightness::ControlAvailability::Available;
  value.currentKnown = true;
  value.rawCurrent = 2;
  value.rawMaximum = 4;
  value.normalizedCurrent = 5000;
  value.canSet = true;
  return value;
}

BrightnessView composedView() {
  BrightnessView view;
  view.ownerAvailable = true;
  view.model.displays.append(displayControl(QStringLiteral("dp-1")));
  view.model.keyboards.append(keyboardControl(QStringLiteral("kb0")));
  return view;
}

} // namespace

class PowerAppletControlRowTests final : public QObject {
    Q_OBJECT

private slots:
    void brightnessRowsFollowCompositionOwnerFence();
    void controlRowsCarryAccessibilityIdentity();
    void composedRowsSortDeterministically();
};

void PowerAppletControlRowTests::brightnessRowsFollowCompositionOwnerFence()
{
    const Power::Snapshot snapshot = readySnapshot();
    const PowerAppletModel composed =
        projectPowerApplet(snapshot, true, composedView());
    QCOMPARE(composed.displayControls.size(), 1);
    QCOMPARE(composed.keyboardControls.size(), 1);
    const BrightnessControlRow &display = composed.displayControls.at(0);
    QCOMPARE(display.lane, ControlLane::Display);
    QCOMPARE(display.controlId, QStringLiteral("dp-1"));
    QCOMPARE(display.availability, RowAvailability::Available);
    QVERIFY(display.currentKnown);
    QCOMPARE(display.normalizedCurrent, 4000U);
    // Power1 v1 defines no display-set operation; display rows are never
    // presented as directly adjustable.
    QVERIFY(!display.adjustable);
    const BrightnessControlRow &keyboard = composed.keyboardControls.at(0);
    QCOMPARE(keyboard.lane, ControlLane::Keyboard);
    QCOMPARE(keyboard.controlId, QStringLiteral("kb0"));
    QVERIFY(keyboard.adjustable);
    QVERIFY(keyboard.accessibleName.contains(QStringLiteral("50%")));

    BrightnessView degradedView;
    degradedView.ownerAvailable = true;
    Brightness::KeyboardControl degraded = keyboardControl(QStringLiteral("kb0"));
    degraded.availability = Brightness::ControlAvailability::Degraded;
    degraded.reason = Brightness::ControlReason::ProviderDegraded;
    degraded.canSet = true;
    degradedView.model.keyboards.append(degraded);
    const PowerAppletModel degradedModel =
        projectPowerApplet(snapshot, true, degradedView);
    const BrightnessControlRow &degradedRow =
        degradedModel.keyboardControls.at(0);
    QCOMPARE(degradedRow.availability, RowAvailability::Degraded);
    QCOMPARE(degradedRow.unavailableReason,
             QStringLiteral("Brightness provider is degraded"));
    QVERIFY(!degradedRow.adjustable);

    // Without the composition owner the snapshot fallback keeps identity but
    // fails closed: unavailable, valueless, non-adjustable.
    Power::Snapshot devices = readySnapshot();
    devices.capabilities.setFlag(Power::Capability::KeyboardBacklight);
    devices.capabilities.setFlag(Power::Capability::InternalBacklight);
    Power::KeyboardBacklight kb;
    kb.handle = handle(QStringLiteral("kb0"));
    kb.name = QStringLiteral("Lenovo kbd");
    kb.valueKnown = true;
    kb.value = 2;
    kb.maximum = 4;
    kb.normalized = 9000;
    kb.canSet = true;
    devices.keyboardBacklights.append(kb);
    Power::InternalBacklight panel;
    panel.handle = handle(QStringLiteral("panel"));
    panel.deviceName = QStringLiteral("eDP-1");
    panel.observedKnown = true;
    panel.observed = 100;
    panel.maximum = 1000;
    devices.internalBacklights.append(panel);
    const PowerAppletModel fallback = projectPowerApplet(devices, true, {});
    QCOMPARE(fallback.keyboardControls.size(), 1);
    QCOMPARE(fallback.displayControls.size(), 1);
    const BrightnessControlRow &fallbackKeyboard =
        fallback.keyboardControls.at(0);
    QCOMPARE(fallbackKeyboard.availability, RowAvailability::Unavailable);
    QCOMPARE(fallbackKeyboard.controlId, QStringLiteral("kb0"));
    QCOMPARE(fallbackKeyboard.name, QStringLiteral("Lenovo kbd"));
    QVERIFY(!fallbackKeyboard.currentKnown);
    QVERIFY(!fallbackKeyboard.adjustable);
    const BrightnessControlRow &fallbackDisplay =
        fallback.displayControls.at(0);
    QCOMPARE(fallbackDisplay.controlId, QStringLiteral("panel"));
    QVERIFY(!fallbackDisplay.adjustable);
    QVERIFY(!fallbackDisplay.currentKnown);

    // Without the capability gate no identity is invented either.
    const PowerAppletModel bare = projectPowerApplet(snapshot, true, {});
    QVERIFY(bare.keyboardControls.isEmpty());
    QVERIFY(bare.displayControls.isEmpty());
}

void PowerAppletControlRowTests::controlRowsCarryAccessibilityIdentity()
{
    const Power::Snapshot snapshot = readySnapshot();
    const PowerAppletModel composed =
        projectPowerApplet(snapshot, true, composedView());
    for (const BrightnessControlRow &row :
         std::as_const(composed.displayControls)) {
        QVERIFY(!row.accessibleName.isEmpty());
        QVERIFY(!row.accessibleDescription.isEmpty());
    }
    for (const BrightnessControlRow &row :
         std::as_const(composed.keyboardControls)) {
        QVERIFY(!row.accessibleName.isEmpty());
        QVERIFY(!row.accessibleDescription.isEmpty());
    }

    // A hostile out-of-vocabulary reason still renders a complete row with
    // honest fallback wording instead of crashing or leaking a raw number.
    BrightnessView hostileView;
    hostileView.ownerAvailable = true;
    Brightness::DisplayControl hostile = displayControl(QStringLiteral("dp-1"));
    hostile.availability = Brightness::ControlAvailability::Degraded;
    hostile.reason = static_cast<Brightness::ControlReason>(99U);
    hostileView.model.displays.append(hostile);
    const PowerAppletModel hostileModel =
        projectPowerApplet(snapshot, true, hostileView);
    QCOMPARE(hostileModel.displayControls.size(), 1);
    const BrightnessControlRow &hostileRow =
        hostileModel.displayControls.at(0);
    QCOMPARE(hostileRow.availability, RowAvailability::Degraded);
    QCOMPARE(hostileRow.unavailableReason,
             QStringLiteral("Brightness control is unavailable"));
    QVERIFY(!hostileRow.accessibleDescription.isEmpty());

    // Hostile availability raws fail closed, too.
    BrightnessView hostileAvailability;
    hostileAvailability.ownerAvailable = true;
    Brightness::KeyboardControl hostileKeyboard =
        keyboardControl(QStringLiteral("kb0"));
    hostileKeyboard.availability =
        static_cast<Brightness::ControlAvailability>(55U);
    hostileAvailability.model.keyboards.append(hostileKeyboard);
    const PowerAppletModel unavailableKeyboard =
        projectPowerApplet(snapshot, true, hostileAvailability);
    QCOMPARE(unavailableKeyboard.keyboardControls.at(0).availability,
             RowAvailability::Unavailable);
    QVERIFY(!unavailableKeyboard.keyboardControls.at(0).adjustable);
}

void PowerAppletControlRowTests::composedRowsSortDeterministically()
{
    BrightnessView view;
    view.ownerAvailable = true;
    view.model.keyboards.append(keyboardControl(QStringLiteral("kb-b")));
    view.model.keyboards.append(keyboardControl(QStringLiteral("kb-a")));
    view.model.displays.append(displayControl(QStringLiteral("dp-2")));
    view.model.displays.append(displayControl(QStringLiteral("dp-1")));

    const PowerAppletModel model = projectPowerApplet(readySnapshot(), true,
                                                      view);
    QCOMPARE(model.keyboardControls.at(0).controlId,
             QStringLiteral("kb-a"));
    QCOMPARE(model.keyboardControls.at(1).controlId,
             QStringLiteral("kb-b"));
    QCOMPARE(model.displayControls.at(0).controlId,
             QStringLiteral("dp-1"));
    QCOMPARE(model.displayControls.at(1).controlId,
             QStringLiteral("dp-2"));
}

QTEST_GUILESS_MAIN(PowerAppletControlRowTests)
#include "tst_power_applet_controls.moc"
