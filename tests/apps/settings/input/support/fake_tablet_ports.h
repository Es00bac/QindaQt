// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QList>
#include <QVariant>

namespace QindaQt::Tests {

// In-memory tablet authority for the Settings route rows: the same contract
// as the KWin port, with every write recorded and every refusal scriptable.
class FakeTabletPort final
    : public Services::TabletDevices::TabletDevicePort {
public:
    QList<Services::TabletDevices::TabletDeviceSnapshot> scripted;
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

// A Wacom pen as KWin 6.6.6 presents it: every capability flag true, so a
// row that hides a control is hiding it for a reason the test states.
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
        {QStringLiteral("leftHanded"), false},
        {QStringLiteral("tabletToolIsRelative"), false},
        {QStringLiteral("size"), QVariantList{294.0, 166.0}},
        {QStringLiteral("supportsDisableEvents"), true},
        {QStringLiteral("supportsInputArea"), true},
        {QStringLiteral("supportsCalibrationMatrix"), true},
        {QStringLiteral("supportsPressureRange"), true},
        {QStringLiteral("supportsRotation"), true},
        {QStringLiteral("supportsLeftHanded"), true},
    };
    return pen;
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
