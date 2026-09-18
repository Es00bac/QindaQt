// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace QindaQt::Apps::SettingsInput {

// Presentation state of the selected tablet: one writable property per
// user-visible control, each paired with an availability truth taken from
// the device's own `supports*` flags. A control whose availability is false
// is hidden, never rendered disabled-but-visible (ADR-0134).
//
// AGENT-CONTRACT: Every setter dispatches through the port and reverts on
// failure; success alone moves the presented value. QML binds to these
// properties and never writes device state directly.
//
// AGENT-GUARD: `mapMode` and `outputName` are one decision. Writing them in
// the wrong order hands the pen back to the active screen for a frame, so
// both go through applyMapping(), never through two independent setters.
class TabletDeviceSelection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceChanged)
    Q_PROPERTY(QString name READ name NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceGroupId READ deviceGroupId NOTIFY deviceChanged)
    Q_PROPERTY(bool hasPen READ hasPen NOTIFY deviceChanged)
    Q_PROPERTY(bool hasPad READ hasPad NOTIFY deviceChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

    // Enable device
    Q_PROPERTY(bool deviceEnabled READ deviceEnabled WRITE setDeviceEnabled
                   NOTIFY deviceEnabledChanged)
    Q_PROPERTY(bool deviceEnabledAvailable READ deviceEnabledAvailable NOTIFY
                   availabilityChanged)

    // Map to: "follow" | "output" | "workspace"
    Q_PROPERTY(QString mapMode READ mapMode NOTIFY mappingChanged)
    Q_PROPERTY(QString outputName READ outputName NOTIFY mappingChanged)
    Q_PROPERTY(QStringList outputNames READ outputNames NOTIFY outputsChanged)
    Q_PROPERTY(QStringList outputLabels READ outputLabels NOTIFY outputsChanged)

    // Area: the mapped rectangle of the output, as four normalized doubles.
    Q_PROPERTY(QVariantList outputArea READ outputArea NOTIFY areaChanged)
    Q_PROPERTY(bool outputAreaAvailable READ outputAreaAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(QVariantList inputArea READ inputArea NOTIFY areaChanged)
    Q_PROPERTY(bool inputAreaAvailable READ inputAreaAvailable NOTIFY
                   availabilityChanged)
    // True when the tablet reports a physical size, so "keep the tablet's
    // proportions" can be computed rather than guessed.
    Q_PROPERTY(bool aspectRatioAvailable READ aspectRatioAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(double tabletAspectRatio READ tabletAspectRatio NOTIFY
                   deviceChanged)

    // Orientation
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY
                   rotationChanged)
    Q_PROPERTY(bool rotationAvailable READ rotationAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool leftHanded READ leftHanded WRITE setLeftHanded NOTIFY
                   leftHandedChanged)
    Q_PROPERTY(bool leftHandedAvailable READ leftHandedAvailable NOTIFY
                   availabilityChanged)

    // Calibration
    Q_PROPERTY(QString calibrationMatrix READ calibrationMatrix NOTIFY
                   calibrationChanged)
    Q_PROPERTY(bool calibrationAvailable READ calibrationAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool calibrated READ calibrated NOTIFY calibrationChanged)

    // Pressure
    Q_PROPERTY(QString pressureCurve READ pressureCurve NOTIFY pressureChanged)
    Q_PROPERTY(bool pressureCurveAvailable READ pressureCurveAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(double pressureRangeMin READ pressureRangeMin WRITE
                   setPressureRangeMin NOTIFY pressureChanged)
    Q_PROPERTY(double pressureRangeMax READ pressureRangeMax WRITE
                   setPressureRangeMax NOTIFY pressureChanged)
    Q_PROPERTY(bool pressureRangeAvailable READ pressureRangeAvailable NOTIFY
                   availabilityChanged)

    // Pen mode
    Q_PROPERTY(bool relativeMode READ relativeMode WRITE setRelativeMode NOTIFY
                   relativeModeChanged)
    Q_PROPERTY(bool relativeModeAvailable READ relativeModeAvailable NOTIFY
                   availabilityChanged)

    // Pad hardware counts; -1 means the authority reports none.
    Q_PROPERTY(int padButtonCount READ padButtonCount NOTIFY deviceChanged)
    Q_PROPERTY(int padRingCount READ padRingCount NOTIFY deviceChanged)
    Q_PROPERTY(int padStripCount READ padStripCount NOTIFY deviceChanged)
    Q_PROPERTY(int padDialCount READ padDialCount NOTIFY deviceChanged)

public:
    // AGENT-CONTRACT: `store` is how a choice made here survives. Writing
    // only to KWin would let the session policy re-decide the mapping a
    // moment later and silently revert it, which reads to the user as
    // "Settings does nothing". A null store is the degraded case: the route
    // still drives KWin and says the choice could not be remembered.
    TabletDeviceSelection(
        const Services::TabletDevices::TabletDevicePort &port,
        const Services::TabletDevices::TabletOutputInventory &outputs,
        Services::TabletDevices::TabletMappingStore *store,
        QObject *parent = nullptr);

    // Replaces the presented device group wholesale. `pen` is the tool
    // device (the one every control writes to); `pad` may be absent.
    void setGroup(const Services::TabletDevices::TabletDeviceSnapshot &pen,
                  bool hasPen,
                  const Services::TabletDevices::TabletDeviceSnapshot &pad,
                  bool hasPad);
    void clear();
    // Re-reads the output list; the route calls this when screens change.
    void refreshOutputs();

    [[nodiscard]] QString deviceId() const { return m_penId; }
    [[nodiscard]] QString name() const { return m_name; }
    [[nodiscard]] QString deviceGroupId() const { return m_groupId; }
    [[nodiscard]] bool hasPen() const { return m_hasPen; }
    [[nodiscard]] bool hasPad() const { return m_hasPad; }
    [[nodiscard]] QString statusText() const { return m_statusText; }

    [[nodiscard]] bool deviceEnabled() const { return m_enabled; }
    [[nodiscard]] bool deviceEnabledAvailable() const {
        return m_enabledAvailable;
    }
    [[nodiscard]] QString mapMode() const { return m_mapMode; }
    [[nodiscard]] QString outputName() const { return m_outputName; }
    [[nodiscard]] QStringList outputNames() const { return m_outputNames; }
    [[nodiscard]] QStringList outputLabels() const { return m_outputLabels; }
    [[nodiscard]] QVariantList outputArea() const { return m_outputArea; }
    [[nodiscard]] bool outputAreaAvailable() const {
        return m_outputAreaAvailable;
    }
    [[nodiscard]] QVariantList inputArea() const { return m_inputArea; }
    [[nodiscard]] bool inputAreaAvailable() const {
        return m_inputAreaAvailable;
    }
    [[nodiscard]] bool aspectRatioAvailable() const {
        return m_outputAreaAvailable && m_aspectRatio > 0.0;
    }
    [[nodiscard]] double tabletAspectRatio() const { return m_aspectRatio; }
    [[nodiscard]] int rotation() const { return m_rotation; }
    [[nodiscard]] bool rotationAvailable() const { return m_rotationAvailable; }
    [[nodiscard]] bool leftHanded() const { return m_leftHanded; }
    [[nodiscard]] bool leftHandedAvailable() const {
        return m_leftHandedAvailable;
    }
    [[nodiscard]] QString calibrationMatrix() const { return m_calibration; }
    [[nodiscard]] bool calibrationAvailable() const {
        return m_calibrationAvailable;
    }
    [[nodiscard]] bool calibrated() const {
        return m_calibrationAvailable && m_calibration != m_defaultCalibration;
    }
    [[nodiscard]] QString pressureCurve() const { return m_pressureCurve; }
    [[nodiscard]] bool pressureCurveAvailable() const {
        return m_pressureCurveAvailable;
    }
    [[nodiscard]] double pressureRangeMin() const { return m_pressureMin; }
    [[nodiscard]] double pressureRangeMax() const { return m_pressureMax; }
    [[nodiscard]] bool pressureRangeAvailable() const {
        return m_pressureRangeAvailable;
    }
    [[nodiscard]] bool relativeMode() const { return m_relative; }
    [[nodiscard]] bool relativeModeAvailable() const {
        return m_relativeAvailable;
    }
    [[nodiscard]] int padButtonCount() const { return m_padButtons; }
    [[nodiscard]] int padRingCount() const { return m_padRings; }
    [[nodiscard]] int padStripCount() const { return m_padStrips; }
    [[nodiscard]] int padDialCount() const { return m_padDials; }

    void setDeviceEnabled(bool value);
    void setRotation(int value);
    void setLeftHanded(bool value);
    void setPressureRangeMin(double value);
    void setPressureRangeMax(double value);
    void setRelativeMode(bool value);

    // Map to. `mode` is one of "follow", "output", "workspace"; `output` is
    // read only for "output". Returns false and leaves the presented value
    // alone when the authority refuses.
    Q_INVOKABLE bool applyMapping(const QString &mode,
                                  const QString &output = {});
    // Area. `x, y, width, height` are normalized to the mapped surface.
    Q_INVOKABLE bool applyOutputArea(double x, double y, double width,
                                     double height);
    Q_INVOKABLE bool applyInputArea(double x, double y, double width,
                                    double height);
    // Fit the whole screen, or letterbox to the tablet's own proportions
    // inside the current output. Returns false when the tablet reports no
    // physical size, because a computed ratio would then be a guess.
    Q_INVOKABLE bool fitWholeScreen();
    Q_INVOKABLE bool keepTabletProportions(double outputWidth,
                                           double outputHeight);
    // Calibration. `matrix` is KWin's 16-value comma-separated row-major
    // string; the wizard computes it from four measured targets.
    Q_INVOKABLE bool applyCalibrationMatrix(const QString &matrix);
    Q_INVOKABLE bool resetCalibration();
    // Pressure curve as KWin's "x,y;x,y;" points.
    Q_INVOKABLE bool applyPressureCurve(const QString &curve);
    Q_INVOKABLE bool resetPressure();
    // Every control of this device back to the authority's own defaults.
    Q_INVOKABLE bool resetDevice();

    // Computes the calibration matrix that maps four measured touch points
    // to the four target points they were drawn at. Returns an empty string
    // when the measurements are degenerate; the wizard then asks again
    // instead of writing a matrix that would make the pen unusable.
    [[nodiscard]] Q_INVOKABLE static QString
    calibrationMatrixFor(const QVariantList &measured,
                         const QVariantList &targets);

Q_SIGNALS:
    void deviceChanged();
    void deviceEnabledChanged();
    void mappingChanged();
    void outputsChanged();
    void areaChanged();
    void rotationChanged();
    void leftHandedChanged();
    void calibrationChanged();
    void pressureChanged();
    void relativeModeChanged();
    void availabilityChanged();
    void statusTextChanged();

private:
    bool write(const QString &deviceId, const QString &property,
               const QVariant &value, const QString &label);
    void setStatusText(const QString &text);
    [[nodiscard]] QVariant penProperty(const QString &name) const;

    const Services::TabletDevices::TabletDevicePort &m_port;
    const Services::TabletDevices::TabletOutputInventory &m_outputs;
    Services::TabletDevices::TabletMappingStore *m_store = nullptr;

    // The stable identity the ledger is keyed on, never KWin's deviceGroupId.
    QString m_identity;
    QString m_penId;
    QString m_padId;
    QString m_name;
    QString m_groupId;
    bool m_hasPen = false;
    bool m_hasPad = false;
    QVariantMap m_penProperties;
    QVariantMap m_padProperties;

    bool m_enabled = true;
    bool m_enabledAvailable = false;
    QString m_mapMode = QStringLiteral("follow");
    QString m_outputName;
    QStringList m_outputNames;
    QStringList m_outputLabels;
    QVariantList m_outputArea;
    QVariantList m_inputArea;
    bool m_outputAreaAvailable = false;
    bool m_inputAreaAvailable = false;
    double m_aspectRatio = 0.0;
    int m_rotation = 0;
    bool m_rotationAvailable = false;
    bool m_leftHanded = false;
    bool m_leftHandedAvailable = false;
    QString m_calibration;
    QString m_defaultCalibration;
    bool m_calibrationAvailable = false;
    QString m_pressureCurve;
    QString m_defaultPressureCurve;
    bool m_pressureCurveAvailable = false;
    double m_pressureMin = 0.0;
    double m_pressureMax = 1.0;
    bool m_pressureRangeAvailable = false;
    bool m_relative = false;
    bool m_relativeAvailable = false;
    int m_padButtons = -1;
    int m_padRings = -1;
    int m_padStrips = -1;
    int m_padDials = -1;
    QString m_statusText;
};

} // namespace QindaQt::Apps::SettingsInput
