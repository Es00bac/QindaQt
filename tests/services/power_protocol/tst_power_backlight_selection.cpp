// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_backlight_selection.h>

#include <QtTest>

using namespace QindaQt::Power;

namespace {

InternalBacklight device(const QString &id, const BacklightKind kind,
                         const BacklightStatus status = BacklightStatus::Ok,
                         const quint32 maximum = 100)
{
    InternalBacklight backlight;
    backlight.handle = {.epoch = 7, .opaqueId = id};
    backlight.deviceName = id;
    backlight.kind = kind;
    backlight.maximum = maximum;
    backlight.observedKnown = maximum > 0 && status != BacklightStatus::Degraded;
    backlight.observed = backlight.observedKnown ? maximum / 2 : 0;
    backlight.status = maximum == 0 ? BacklightStatus::Unavailable : status;
    backlight.reason = maximum == 0                         ? BacklightReason::NoBacklight
        : status == BacklightStatus::Ok                     ? BacklightReason::None
        : status == BacklightStatus::Degraded               ? BacklightReason::DeviceDisappeared
                                                            : BacklightReason::LogindError;
    return backlight;
}

} // namespace

class PowerBacklightSelectionTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void soleWritablePanelIsAdmitted();
    void kernelTypePreferenceSelectsOneTarget_data();
    void kernelTypePreferenceSelectsOneTarget();
    void equallyPreferredDevicesAreAmbiguousInEveryOrder();
    void unusableSelectedPanelNeverFallsBackToAnotherDevice();
    void unusableMaximumIsNeitherTargetNorAmbiguity();
    void unknownTargetAndEmptyInventoryAreRefused();
};

void PowerBacklightSelectionTests::soleWritablePanelIsAdmitted()
{
    const QList<InternalBacklight> devices{device(QStringLiteral("panel"),
                                                  BacklightKind::Raw)};
    QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("panel")),
             InternalBrightnessAdmission::Admitted);
}

void PowerBacklightSelectionTests::kernelTypePreferenceSelectsOneTarget_data()
{
    QTest::addColumn<BacklightKind>("preferred");
    QTest::addColumn<BacklightKind>("other");
    QTest::newRow("firmware-over-platform") << BacklightKind::Firmware
                                            << BacklightKind::Platform;
    QTest::newRow("firmware-over-raw") << BacklightKind::Firmware << BacklightKind::Raw;
    QTest::newRow("platform-over-raw") << BacklightKind::Platform << BacklightKind::Raw;
}

void PowerBacklightSelectionTests::kernelTypePreferenceSelectsOneTarget()
{
    QFETCH(BacklightKind, preferred);
    QFETCH(BacklightKind, other);
    for (const bool preferredFirst : {true, false}) {
        QList<InternalBacklight> devices{device(QStringLiteral("preferred"), preferred),
                                         device(QStringLiteral("other"), other)};
        if (!preferredFirst) {
            devices.swapItemsAt(0, 1);
        }
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("preferred")),
                 InternalBrightnessAdmission::Admitted);
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("other")),
                 InternalBrightnessAdmission::NotSelected);
    }
}

void PowerBacklightSelectionTests::equallyPreferredDevicesAreAmbiguousInEveryOrder()
{
    QList<InternalBacklight> devices{device(QStringLiteral("edp"), BacklightKind::Firmware),
                                     device(QStringLiteral("raw"), BacklightKind::Raw),
                                     device(QStringLiteral("dsi"), BacklightKind::Firmware)};
    for (int rotation = 0; rotation < devices.size(); ++rotation) {
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("edp")),
                 InternalBrightnessAdmission::Ambiguous);
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("dsi")),
                 InternalBrightnessAdmission::Ambiguous);
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("raw")),
                 InternalBrightnessAdmission::NotSelected);
        devices.move(0, devices.size() - 1);
    }

    // Ambiguity is decided before the selected device's own writability, so a
    // read-only twin still makes a writable panel ambiguous.
    devices = {device(QStringLiteral("edp"), BacklightKind::Firmware),
               device(QStringLiteral("dsi"), BacklightKind::Firmware,
                      BacklightStatus::Unavailable)};
    QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("edp")),
             InternalBrightnessAdmission::Ambiguous);
}

void PowerBacklightSelectionTests::unusableSelectedPanelNeverFallsBackToAnotherDevice()
{
    for (const BacklightStatus status :
         {BacklightStatus::Unavailable, BacklightStatus::Degraded}) {
        const QList<InternalBacklight> devices{
            device(QStringLiteral("firmware"), BacklightKind::Firmware, status),
            device(QStringLiteral("raw"), BacklightKind::Raw)};
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("firmware")),
                 InternalBrightnessAdmission::DeviceUnavailable);
        QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("raw")),
                 InternalBrightnessAdmission::NotSelected);
    }

    // Ok status without an observed value is not an admitted control either.
    InternalBacklight unobserved = device(QStringLiteral("firmware"), BacklightKind::Firmware);
    unobserved.observedKnown = false;
    unobserved.observed = 0;
    QCOMPARE(internalBrightnessAdmission({unobserved}, QStringLiteral("firmware")),
             InternalBrightnessAdmission::DeviceUnavailable);
}

void PowerBacklightSelectionTests::unusableMaximumIsNeitherTargetNorAmbiguity()
{
    const QList<InternalBacklight> devices{
        device(QStringLiteral("broken"), BacklightKind::Firmware, BacklightStatus::Ok, 0),
        device(QStringLiteral("panel"), BacklightKind::Raw)};
    QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("broken")),
             InternalBrightnessAdmission::DeviceUnavailable);
    QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("panel")),
             InternalBrightnessAdmission::Admitted);
}

void PowerBacklightSelectionTests::unknownTargetAndEmptyInventoryAreRefused()
{
    const QList<InternalBacklight> devices{device(QStringLiteral("panel"),
                                                  BacklightKind::Firmware)};
    QCOMPARE(internalBrightnessAdmission(devices, QStringLiteral("other")),
             InternalBrightnessAdmission::UnknownDevice);
    QCOMPARE(internalBrightnessAdmission(devices, QString()),
             InternalBrightnessAdmission::UnknownDevice);
    QCOMPARE(internalBrightnessAdmission({}, QStringLiteral("panel")),
             InternalBrightnessAdmission::UnknownDevice);
}

QTEST_GUILESS_MAIN(PowerBacklightSelectionTests)
#include "tst_power_backlight_selection.moc"
