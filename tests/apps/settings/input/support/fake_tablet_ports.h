// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QList>
#include <QRectF>
#include <QVariant>

#include <optional>

namespace QindaQt::Tests {

// In-memory tablet authority for the Settings route rows: the same contract
// as the KWin port, with every write recorded and every refusal scriptable.
// An accepted write moves the scripted device too, as KWin's state moves, so
// a model that re-reads the device sees what it wrote.
class FakeTabletPort final
    : public Services::TabletDevices::TabletDevicePort {
public:
    mutable QList<Services::TabletDevices::TabletDeviceSnapshot> scripted;
    QString listError;
    mutable QList<std::tuple<QString, QString, QVariant>> writes;
    mutable QString refuse;

    [[nodiscard]] QList<Services::TabletDevices::TabletDeviceSnapshot>
    devices(QString *error) const override {
        if (error != nullptr) {
            *error = listError;
        }
        return listError.isEmpty()
                   ? scripted
                   : QList<Services::TabletDevices::TabletDeviceSnapshot>{};
    }

    [[nodiscard]] bool
    device(const QString &deviceId,
           Services::TabletDevices::TabletDeviceSnapshot *snapshot,
           QString *error) const override {
        Q_UNUSED(error)
        for (const auto &candidate : scripted) {
            if (candidate.deviceId == deviceId) {
                *snapshot = candidate;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool writeProperty(const QString &deviceId,
                                     const QString &property,
                                     const QVariant &value,
                                     QString *error) const override {
        if (property == refuse) {
            if (error != nullptr) {
                *error = QStringLiteral("authority refused %1").arg(property);
            }
            return false;
        }
        writes.append({deviceId, property, value});
        for (auto &candidate : scripted) {
            if (candidate.deviceId == deviceId) {
                candidate.properties.insert(property, value);
            }
        }
        return true;
    }
};

class FakeTabletOutputs final
    : public Services::TabletDevices::TabletOutputInventory {
public:
    QList<Services::TabletDevices::TabletOutputCandidate> scripted;
    [[nodiscard]] QList<Services::TabletDevices::TabletOutputCandidate>
    outputs() const override {
        return scripted;
    }
    void publish(
        QList<Services::TabletDevices::TabletOutputCandidate> next) {
        scripted = std::move(next);
        Q_EMIT outputsChanged();
    }
};

// An in-memory ledger, so a row can assert what the route recorded without
// a Settings1 owner.
class FakeTabletMappingStore final
    : public Services::TabletDevices::TabletMappingStore {
    Q_OBJECT
public:
    Services::TabletDevices::TabletMappingLedger held;
    bool loaded = true;
    bool saveFails = false;
    int saves = 0;

    [[nodiscard]] bool isLoaded() const override { return loaded; }
    [[nodiscard]] Services::TabletDevices::TabletMappingLedger
    ledger() const override {
        return held;
    }
    bool save(const Services::TabletDevices::TabletMappingLedger &ledger)
        override {
        ++saves;
        if (saveFails) {
            return false;
        }
        held = ledger;
        return true;
    }
};

// A Wacom One pen display as KWin 6.6.6 + libinput 1.31 present it. libinput
// offers no tablet area for an INPUT_PROP_DIRECT tablet and no rotation for
// any tablet, so `supportsInputArea` and `supportsRotation` are false; that
// false `supportsInputArea` is also what classifies it as a pen display
// (ADR-0285). Every other capability is true, so a row that hides a control
// is hiding it for a reason the test states.
inline Services::TabletDevices::TabletDeviceSnapshot
fakeWacomPen(const QString &id = QStringLiteral("event19")) {
    Services::TabletDevices::TabletDeviceSnapshot pen;
    pen.deviceId = id;
    pen.name = QStringLiteral("Wacom One Pen Display 13 Pen");
    // A pointer-address hash, as KWin actually supplies.
    pen.deviceGroupId = QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=");
    pen.vendorId = 1386;
    pen.productId = 934;
    pen.tabletTool = true;
    pen.properties = QVariantMap{
        {QStringLiteral("enabled"), true},
        {QStringLiteral("outputName"), QStringLiteral("HDMI-A-1")},
        {QStringLiteral("mapToWorkspace"), false},
        {QStringLiteral("outputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("inputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("calibrationMatrix"),
         QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1")},
        {QStringLiteral("defaultCalibrationMatrix"),
         QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1")},
        {QStringLiteral("pressureCurve"), QStringLiteral("0,0;1,1;")},
        {QStringLiteral("defaultPressureCurve"), QStringLiteral("0,0;1,1;")},
        {QStringLiteral("pressureRangeMin"), 0.0},
        {QStringLiteral("pressureRangeMax"), 1.0},
        {QStringLiteral("rotation"), 0u},
        {QStringLiteral("orientationDBus"), 0},
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("tabletToolIsRelative"), false},
        {QStringLiteral("size"), QVariantList{294.0, 166.0}},
        {QStringLiteral("supportsDisableEvents"), true},
        {QStringLiteral("supportsInputArea"), false},
        {QStringLiteral("supportsCalibrationMatrix"), true},
        {QStringLiteral("supportsPressureRange"), true},
        {QStringLiteral("supportsRotation"), false},
        {QStringLiteral("supportsLeftHanded"), true},
    };
    return pen;
}

// The owner's desk tablet, a Wacom Bamboo Connect CTL-470, exactly as KWin
// 6.6.6 reported it on qinda-top (2026-09-28, libinput without libwacom):
// no rotation, but a calibration matrix KWin folds its orientation into, and
// a tablet area, which is libinput's verdict that it is not a screen.
inline Services::TabletDevices::TabletDeviceSnapshot
fakeBambooPen(const QString &outputName = QString()) {
    Services::TabletDevices::TabletDeviceSnapshot pen;
    pen.deviceId = QStringLiteral("event3");
    pen.name = QStringLiteral("Wacom Bamboo Connect Pen");
    pen.deviceGroupId = QStringLiteral("Zm9vYmFyYmF6cXV4MTIzNDU2Nzg5MA==");
    pen.vendorId = 1386;
    pen.productId = 221;
    pen.tabletTool = true;
    pen.properties = QVariantMap{
        {QStringLiteral("enabled"), true},
        {QStringLiteral("outputName"), outputName},
        {QStringLiteral("mapToWorkspace"), false},
        {QStringLiteral("outputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("inputArea"), QVariantList{0.0, 0.0, 1.0, 1.0}},
        {QStringLiteral("calibrationMatrix"),
         QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1")},
        {QStringLiteral("defaultCalibrationMatrix"),
         QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1")},
        {QStringLiteral("pressureCurve"), QStringLiteral("0,0;1,1;")},
        {QStringLiteral("defaultPressureCurve"), QStringLiteral("0,0;1,1;")},
        {QStringLiteral("pressureRangeMin"), 0.0},
        {QStringLiteral("pressureRangeMax"), 1.0},
        {QStringLiteral("rotation"), 0u},
        {QStringLiteral("orientationDBus"), 0},
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("tabletToolIsRelative"), false},
        {QStringLiteral("size"), QVariantList{147.2, 92.0}},
        {QStringLiteral("supportsDisableEvents"), true},
        {QStringLiteral("supportsInputArea"), true},
        {QStringLiteral("supportsCalibrationMatrix"), true},
        {QStringLiteral("supportsPressureRange"), true},
        {QStringLiteral("supportsRotation"), false},
        {QStringLiteral("supportsLeftHanded"), true},
    };
    return pen;
}

// A plain monitor with the rotation Display1 reports and its logical
// (already rotated) geometry.
inline Services::TabletDevices::TabletOutputCandidate
fakeMonitor(const QString &connector,
            std::optional<Services::TabletDevices::Rotation> rotation,
            const QRectF &geometry = QRectF(0, 0, 1920, 1080)) {
    Services::TabletDevices::TabletOutputCandidate output{
        connector, QStringLiteral("Dell Inc."), QStringLiteral("U2720Q"),
        QStringLiteral("U2720Q"), false, true};
    output.rotation = rotation;
    output.logicalGeometry = geometry;
    return output;
}

inline Services::TabletDevices::TabletDeviceSnapshot
fakeWacomPad(const QString &id = QStringLiteral("event20")) {
    Services::TabletDevices::TabletDeviceSnapshot pad;
    pad.deviceId = id;
    pad.name = QStringLiteral("Wacom One Pen Display 13 Pad");
    pad.deviceGroupId = QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=");
    pad.vendorId = 1386;
    pad.productId = 934;
    pad.tabletPad = true;
    pad.properties = QVariantMap{
        {QStringLiteral("tabletPadButtonCount"), 4u},
        {QStringLiteral("tabletPadRingCount"), 1u},
        {QStringLiteral("tabletPadStripCount"), 0xFFFFFFFFU},
        {QStringLiteral("tabletPadDialCount"), 0xFFFFFFFFU},
    };
    return pad;
}

inline Services::TabletDevices::TabletOutputCandidate
fakePenDisplayOutput() {
    return Services::TabletDevices::TabletOutputCandidate{
        QStringLiteral("HDMI-A-1"), QStringLiteral("Wacom Technology Corp."),
        QStringLiteral("Wacom One 13"), QStringLiteral("Wacom One 13"), false,
        true};
}

inline Services::TabletDevices::TabletOutputCandidate fakeLaptopPanel() {
    return Services::TabletDevices::TabletOutputCandidate{
        QStringLiteral("eDP-1"), QStringLiteral("Lenovo Group Limited"),
        QStringLiteral("eDP-1-0x9052"), QStringLiteral("eDP-1-0x9052"), true,
        true};
}

} // namespace QindaQt::Tests
