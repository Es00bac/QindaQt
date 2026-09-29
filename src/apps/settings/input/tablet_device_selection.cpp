// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_device_selection.h>

#include <qindaqt/apps/settings_input/tablet_surface.h>
#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <QPointF>

namespace QindaQt::Apps::SettingsInput {

using Services::TabletDevices::TabletDeviceSnapshot;
using Services::TabletDevices::TabletMapChoice;
using Services::TabletDevices::TabletOutputCandidate;

namespace {

bool flag(const QVariantMap &properties, const char *name) {
    const QVariant value = properties.value(QLatin1String(name));
    return value.typeId() == QMetaType::Bool && value.toBool();
}

TabletMapChoice choiceFor(const QString &mapMode) {
    if (mapMode == QLatin1String("workspace")) {
        return TabletMapChoice::EntireWorkspace;
    }
    return mapMode == QLatin1String("output")
               ? TabletMapChoice::NamedOutput
               : TabletMapChoice::FollowActiveScreen;
}

// KWin publishes an absent pad count as the unsigned -1; -1 is the honest
// presentation ("the authority reports none"), not zero controls.
int countOf(const QVariantMap &properties, const char *name) {
    const QVariant value = properties.value(QLatin1String(name));
    if (!value.isValid()) {
        return -1;
    }
    const qulonglong raw = value.toULongLong();
    constexpr qulonglong Unknown = 0xFFFFFFFFULL;
    if (raw >= Unknown) {
        return -1;
    }
    return int(raw);
}

} // namespace

TabletDeviceSelection::TabletDeviceSelection(
    const Services::TabletDevices::TabletDevicePort &port,
    const Services::TabletDevices::TabletOutputInventory &outputs,
    Services::TabletDevices::TabletMappingStore *store, QObject *parent)
    : QObject(parent), m_port(port), m_outputs(outputs), m_store(store),
      m_placement(new TabletPlacementModel(port, outputs, store, this)) {
    // Which rows exist depends on the kind of tablet the placement decided.
    connect(m_placement, &TabletPlacementModel::placementChanged, this,
            &TabletDeviceSelection::availabilityChanged);
    connect(m_placement, &TabletPlacementModel::statusReported, this,
            [this](const QString &text) { setStatusText(text); });
    refreshOutputs();
}

void TabletDeviceSelection::refreshOutputs() {
    QStringList names;
    QStringList labels;
    const QList<TabletOutputCandidate> candidates = m_outputs.outputs();
    for (const TabletOutputCandidate &output : candidates) {
        if (output.connectorName.isEmpty()) {
            continue;
        }
        names.append(output.connectorName);
        labels.append(tabletOutputLabel(output));
    }
    // A screen that turned or moved changes the compensation and the shape
    // the area editor draws even when the list of names did not change.
    m_placement->refresh();
    if (names == m_outputNames && labels == m_outputLabels) {
        return;
    }
    m_outputNames = names;
    m_outputLabels = labels;
    Q_EMIT outputsChanged();
}

QVariant TabletDeviceSelection::penProperty(const QString &name) const {
    return m_penProperties.value(name);
}

void TabletDeviceSelection::clear() {
    m_identity.clear();
    m_penId.clear();
    m_padId.clear();
    m_name.clear();
    m_groupId.clear();
    m_hasPen = false;
    m_hasPad = false;
    m_penProperties.clear();
    m_padProperties.clear();
    m_enabledAvailable = false;
    m_leftHandedAvailable = false;
    m_calibrationAvailable = false;
    m_pressureCurveAvailable = false;
    m_pressureRangeAvailable = false;
    m_relativeAvailable = false;
    m_padButtons = m_padRings = m_padStrips = m_padDials = -1;
    m_placement->clear();
    Q_EMIT deviceChanged();
    Q_EMIT availabilityChanged();
    Q_EMIT mappingChanged();
}

void TabletDeviceSelection::setGroup(const TabletDeviceSnapshot &pen,
                                     bool hasPen,
                                     const TabletDeviceSnapshot &pad,
                                     bool hasPad) {
    m_hasPen = hasPen;
    m_hasPad = hasPad;
    m_penId = hasPen ? pen.deviceId : QString();
    m_padId = hasPad ? pad.deviceId : QString();
    m_penProperties = hasPen ? pen.properties : QVariantMap();
    m_padProperties = hasPad ? pad.properties : QVariantMap();
    m_name = Services::TabletDevices::tabletBaseName(hasPen ? pen.name
                                                             : pad.name);
    m_identity = Services::TabletDevices::tabletIdentity(hasPen ? pen : pad);
    m_groupId = m_identity;

    m_enabled = !m_penProperties.contains(QStringLiteral("enabled")) ||
                flag(m_penProperties, "enabled");
    m_enabledAvailable = hasPen && flag(m_penProperties, "supportsDisableEvents");

    const QString outputName =
        penProperty(QStringLiteral("outputName")).toString();
    const bool workspace = flag(m_penProperties, "mapToWorkspace");
    m_outputName = outputName;
    m_mapMode = workspace         ? QStringLiteral("workspace")
                : outputName.isEmpty() ? QStringLiteral("follow")
                                       : QStringLiteral("output");

    // AGENT-NOTE: `outputArea` exists on every device that can be mapped;
    // `supportsInputArea` gates libinput's area on the tablet surface and is
    // also libinput's verdict that the tablet is not a screen. Both areas and
    // the rotation are presented by `placement` in the frames the user sees.
    m_leftHanded = flag(m_penProperties, "leftHanded");
    m_leftHandedAvailable = hasPen && flag(m_penProperties, "supportsLeftHanded");

    m_calibration = penProperty(QStringLiteral("calibrationMatrix")).toString();
    m_defaultCalibration =
        penProperty(QStringLiteral("defaultCalibrationMatrix")).toString();
    m_calibrationAvailable =
        hasPen && flag(m_penProperties, "supportsCalibrationMatrix");

    m_pressureCurve = penProperty(QStringLiteral("pressureCurve")).toString();
    m_defaultPressureCurve =
        penProperty(QStringLiteral("defaultPressureCurve")).toString();
    // A pressure curve exists for every tablet tool; KWin gates it on the
    // device being a tool, not on a supports* flag.
    m_pressureCurveAvailable = hasPen && !m_pressureCurve.isEmpty();
    m_pressureMin = penProperty(QStringLiteral("pressureRangeMin")).toDouble();
    m_pressureMax = m_penProperties.contains(QStringLiteral("pressureRangeMax"))
                        ? penProperty(QStringLiteral("pressureRangeMax"))
                              .toDouble()
                        : 1.0;
    m_pressureRangeAvailable =
        hasPen && flag(m_penProperties, "supportsPressureRange");

    m_relative = flag(m_penProperties, "tabletToolIsRelative");
    m_relativeAvailable = hasPen;

    const QVariantMap &padSource = hasPad ? m_padProperties : m_penProperties;
    m_padButtons = countOf(padSource, "tabletPadButtonCount");
    m_padRings = countOf(padSource, "tabletPadRingCount");
    m_padStrips = countOf(padSource, "tabletPadStripCount");
    m_padDials = countOf(padSource, "tabletPadDialCount");

    m_statusText.clear();
    m_placement->setDevice(pen, hasPen, m_identity, m_name,
                           choiceFor(m_mapMode), m_outputName);
    Q_EMIT deviceChanged();
    Q_EMIT availabilityChanged();
    Q_EMIT deviceEnabledChanged();
    Q_EMIT mappingChanged();
    Q_EMIT leftHandedChanged();
    Q_EMIT calibrationChanged();
    Q_EMIT pressureChanged();
    Q_EMIT relativeModeChanged();
    Q_EMIT statusTextChanged();
}

void TabletDeviceSelection::setStatusText(const QString &text) {
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    Q_EMIT statusTextChanged();
}

bool TabletDeviceSelection::write(const QString &deviceId,
                                  const QString &property,
                                  const QVariant &value,
                                  const QString &label) {
    if (deviceId.isEmpty()) {
        setStatusText(tr("No tablet is selected."));
        return false;
    }
    QString error;
    if (!m_port.writeProperty(deviceId, property, value, &error)) {
        setStatusText(tr("%1 could not be changed: %2").arg(label, error));
        return false;
    }
    m_penProperties.insert(property, value);
    setStatusText(QString());
    return true;
}

void TabletDeviceSelection::setDeviceEnabled(bool value) {
    if (value == m_enabled) {
        return;
    }
    if (!write(m_penId, QStringLiteral("enabled"), value, tr("Enable device"))) {
        return;
    }
    if (!m_padId.isEmpty()) {
        QString padError;
        // A pad left enabled while its pen is off would keep sending button
        // presses from a tablet the user just switched off.
        (void)m_port.writeProperty(m_padId, QStringLiteral("enabled"), value,
                                   &padError);
    }
    m_enabled = value;
    Q_EMIT deviceEnabledChanged();
}

void TabletDeviceSelection::setLeftHanded(bool value) {
    if (value == m_leftHanded) {
        return;
    }
    if (!write(m_penId, QStringLiteral("leftHanded"), value,
               tr("Left-handed"))) {
        return;
    }
    m_leftHanded = value;
    Q_EMIT leftHandedChanged();
}

void TabletDeviceSelection::setPressureRangeMin(double value) {
    if (qFuzzyCompare(value, m_pressureMin)) {
        return;
    }
    if (!write(m_penId, QStringLiteral("pressureRangeMin"), value,
               tr("Tip threshold"))) {
        return;
    }
    m_pressureMin = value;
    Q_EMIT pressureChanged();
}

void TabletDeviceSelection::setPressureRangeMax(double value) {
    if (qFuzzyCompare(value, m_pressureMax)) {
        return;
    }
    if (!write(m_penId, QStringLiteral("pressureRangeMax"), value,
               tr("Maximum pressure"))) {
        return;
    }
    m_pressureMax = value;
    Q_EMIT pressureChanged();
}

void TabletDeviceSelection::setRelativeMode(bool value) {
    if (value == m_relative) {
        return;
    }
    if (!write(m_penId, QStringLiteral("tabletToolIsRelative"), value,
               tr("Pen mode"))) {
        return;
    }
    m_relative = value;
    Q_EMIT relativeModeChanged();
}

bool TabletDeviceSelection::applyMapping(const QString &mode,
                                         const QString &output) {
    const bool workspace = mode == QLatin1String("workspace");
    const QString wantedOutput =
        mode == QLatin1String("output") ? output : QString();
    if (mode == QLatin1String("output") && wantedOutput.isEmpty()) {
        setStatusText(tr("Choose a screen for the tablet first."));
        return false;
    }
    // AGENT-GUARD: Clear the workspace flag before naming an output and set
    // it after; the other order leaves the pen spanning every screen for a
    // frame, which reads as the mapping having failed.
    if (!workspace && flag(m_penProperties, "mapToWorkspace") &&
        !write(m_penId, QStringLiteral("mapToWorkspace"), false,
               tr("Map to"))) {
        return false;
    }
    if (wantedOutput != m_outputName &&
        !write(m_penId, QStringLiteral("outputName"), wantedOutput,
               tr("Map to"))) {
        return false;
    }
    if (workspace && !flag(m_penProperties, "mapToWorkspace") &&
        !write(m_penId, QStringLiteral("mapToWorkspace"), true, tr("Map to"))) {
        return false;
    }
    m_outputName = wantedOutput;
    m_mapMode = workspace                  ? QStringLiteral("workspace")
                : wantedOutput.isEmpty()   ? QStringLiteral("follow")
                                           : QStringLiteral("output");
    // AGENT-GUARD: KWin obeying is not the same as the desktop remembering.
    // Without this the session policy re-decides on its next pass and the
    // user's choice silently reverts.
    const Services::TabletDevices::TabletMapChoice recorded =
        workspace ? Services::TabletDevices::TabletMapChoice::EntireWorkspace
        : wantedOutput.isEmpty()
            ? Services::TabletDevices::TabletMapChoice::FollowActiveScreen
            : Services::TabletDevices::TabletMapChoice::NamedOutput;
    if (m_store == nullptr ||
        !m_store->recordChoice(m_identity, recorded, wantedOutput, true,
                               m_name)) {
        setStatusText(tr("The tablet now uses that screen, but the choice "
                         "could not be remembered for next time."));
    }
    Q_EMIT mappingChanged();
    // A different screen is a different rotation to compensate (ADR-0285).
    m_placement->applyMapping(recorded, wantedOutput);
    return true;
}

bool TabletDeviceSelection::applyCalibrationMatrix(const QString &matrix) {
    if (matrix.isEmpty()) {
        setStatusText(tr("Those calibration points are too close together. "
                         "Try again, touching each target exactly."));
        return false;
    }
    if (!write(m_penId, QStringLiteral("calibrationMatrix"), matrix,
               tr("Calibration"))) {
        return false;
    }
    m_calibration = matrix;
    Q_EMIT calibrationChanged();
    return true;
}

bool TabletDeviceSelection::resetCalibration() {
    const QString target =
        m_defaultCalibration.isEmpty()
            ? Services::TabletDevices::identityCalibrationMatrix()
            : m_defaultCalibration;
    return applyCalibrationMatrix(target);
}

bool TabletDeviceSelection::applyPressureCurve(const QString &curve) {
    if (!Services::TabletDevices::isValidPressureCurve(curve)) {
        setStatusText(tr("That pressure curve is not usable."));
        return false;
    }
    if (!write(m_penId, QStringLiteral("pressureCurve"), curve,
               tr("Pressure"))) {
        return false;
    }
    m_pressureCurve = curve;
    Q_EMIT pressureChanged();
    return true;
}

bool TabletDeviceSelection::resetPressure() {
    bool ok = true;
    if (!m_defaultPressureCurve.isEmpty()) {
        ok = applyPressureCurve(m_defaultPressureCurve) && ok;
    }
    if (m_pressureRangeAvailable) {
        const QVariant min =
            penProperty(QStringLiteral("defaultPressureRangeMin"));
        const QVariant max =
            penProperty(QStringLiteral("defaultPressureRangeMax"));
        setPressureRangeMin(min.isValid() ? min.toDouble() : 0.0);
        setPressureRangeMax(max.isValid() ? max.toDouble() : 1.0);
    }
    return ok;
}

bool TabletDeviceSelection::resetDevice() {
    bool ok = true;
    if (m_calibrationAvailable) {
        ok = resetCalibration() && ok;
    }
    ok = resetPressure() && ok;
    if (m_leftHandedAvailable) {
        setLeftHanded(false);
    }
    // Upright on its screen with the whole tablet on the whole surface; a
    // pen display also loses any stale rotation of its own.
    ok = m_placement->reset() && ok;
    setRelativeMode(false);
    return ok;
}

QString TabletDeviceSelection::calibrationMatrixFor(
    const QVariantList &measured, const QVariantList &targets) {
    const auto toPoints = [](const QVariantList &source) {
        QList<QPointF> points;
        for (const QVariant &entry : source) {
            if (entry.canConvert<QPointF>()) {
                points.append(entry.toPointF());
                continue;
            }
            if (entry.typeId() == QMetaType::QVariantList) {
                const QVariantList pair = entry.toList();
                if (pair.size() == 2) {
                    points.append(
                        QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
                    continue;
                }
            }
            if (entry.typeId() == QMetaType::QVariantMap) {
                const QVariantMap pair = entry.toMap();
                if (pair.contains(QStringLiteral("x")) &&
                    pair.contains(QStringLiteral("y"))) {
                    points.append(
                        QPointF(pair.value(QStringLiteral("x")).toDouble(),
                                pair.value(QStringLiteral("y")).toDouble()));
                    continue;
                }
            }
            return QList<QPointF>{};
        }
        return points;
    };
    return Services::TabletDevices::calibrationMatrixFor(toPoints(measured),
                                                         toPoints(targets));
}

} // namespace QindaQt::Apps::SettingsInput
