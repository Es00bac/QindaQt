// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_device_selection.h>

namespace QindaQt::Apps::SettingsInput {
namespace {

constexpr double MinimumSpeed = -1.0;
constexpr double MaximumSpeed = 1.0;
constexpr double MinimumScrollSpeed = 0.1;
constexpr double MaximumScrollSpeed = 5.0;

bool snapshotBool(const QVariantMap &properties, const QString &name,
                  bool fallback = false) {
    const QVariant value = properties.value(name);
    return value.typeId() == QMetaType::Bool ? value.toBool() : fallback;
}

double snapshotDouble(const QVariantMap &properties, const QString &name,
                      double fallback) {
    const QVariant value = properties.value(name);
    return value.typeId() == QMetaType::Double ? value.toDouble()
                                               : fallback;
}

} // namespace

PointerDeviceSelection::PointerDeviceSelection(const PointerDevicePort &port,
                                               QObject *parent)
    : QObject(parent), m_port(port) {}

void PointerDeviceSelection::setSnapshot(
    const PointerDeviceSnapshot &snapshot) {
    // AGENT-GUARD: Availability is the single source for row visibility.
    // Derive it only from capability truth in the snapshot; a property the
    // authority happens to echo without a supports* flag must not render a
    // control the device cannot honor.
    m_deviceId = snapshot.deviceId;
    m_name = snapshot.name;
    m_touchpad = snapshot.touchpad;
    m_properties = snapshot.properties;

    m_speedAvailable =
        snapshotBool(m_properties, QStringLiteral("supportsPointerAcceleration"));
    m_speed = snapshotDouble(m_properties,
                             QStringLiteral("pointerAcceleration"), 0.0);
    m_profileAvailable = snapshotBool(
            m_properties,
            QStringLiteral("supportsPointerAccelerationProfileFlat")) &&
        snapshotBool(m_properties,
                     QStringLiteral(
                         "supportsPointerAccelerationProfileAdaptive"));
    m_flatProfile =
        snapshotBool(m_properties,
                     QStringLiteral("pointerAccelerationProfileFlat"));
    m_naturalScrollAvailable =
        snapshotBool(m_properties, QStringLiteral("supportsNaturalScroll"));
    m_naturalScroll =
        snapshotBool(m_properties, QStringLiteral("naturalScroll"));
    m_leftHandedAvailable =
        snapshotBool(m_properties, QStringLiteral("supportsLeftHanded"));
    m_leftHanded = snapshotBool(m_properties, QStringLiteral("leftHanded"));
    m_scrollSpeedAvailable =
        m_properties.contains(QStringLiteral("scrollFactor"));
    m_scrollSpeed = snapshotDouble(m_properties,
                                   QStringLiteral("scrollFactor"), 1.0);
    m_middleEmulationAvailable = snapshotBool(
        m_properties, QStringLiteral("supportsMiddleEmulation"));
    m_middleEmulation =
        snapshotBool(m_properties, QStringLiteral("middleEmulation"));
    m_tapToClickAvailable = m_touchpad && snapshotBool(
        m_properties, QStringLiteral("supportsTapToClick"));
    m_tapToClick = snapshotBool(m_properties, QStringLiteral("tapToClick"));
    m_tapAndDragAvailable = m_touchpad && snapshotBool(
        m_properties, QStringLiteral("supportsTapAndDrag"));
    m_tapAndDrag = snapshotBool(m_properties, QStringLiteral("tapAndDrag"));
    m_disableWhileTypingAvailable =
        m_touchpad &&
        snapshotBool(m_properties,
                     QStringLiteral("supportsDisableWhileTyping"));
    m_disableWhileTyping =
        snapshotBool(m_properties, QStringLiteral("disableWhileTyping"));
    m_scrollMethodAvailable =
        m_touchpad && snapshotBool(m_properties,
                                   QStringLiteral("supportsScrollTwoFinger")) &&
        snapshotBool(m_properties, QStringLiteral("supportsScrollEdge"));
    m_scrollMethod = snapshotBool(m_properties,
                                  QStringLiteral("scrollTwoFinger"), true)
                         ? QStringLiteral("two-finger")
                         : QStringLiteral("edge");
    m_statusText.clear();
    Q_EMIT availabilityChanged();
    Q_EMIT speedChanged();
    Q_EMIT profileChanged();
    Q_EMIT naturalScrollChanged();
    Q_EMIT leftHandedChanged();
    Q_EMIT scrollSpeedChanged();
    Q_EMIT middleEmulationChanged();
    Q_EMIT tapToClickChanged();
    Q_EMIT tapAndDragChanged();
    Q_EMIT disableWhileTypingChanged();
    Q_EMIT scrollMethodChanged();
    Q_EMIT statusTextChanged();
}

bool PointerDeviceSelection::writeBool(const QString &property, bool &member,
                                       void (PointerDeviceSelection::*changed)(),
                                       bool value, const QString &label) {
    QString error;
    if (!m_port.writeProperty(m_deviceId, property, value, &error)) {
        m_statusText = QStringLiteral("%1: %2").arg(label, error);
        Q_EMIT statusTextChanged();
        return false;
    }
    member = value;
    Q_EMIT statusTextChanged();
    (this->*changed)();
    return true;
}

bool PointerDeviceSelection::writeDouble(const QString &property,
                                         double &member,
                                         void (PointerDeviceSelection::*changed)(),
                                         double value, const QString &label) {
    QString error;
    if (!m_port.writeProperty(m_deviceId, property, value, &error)) {
        m_statusText = QStringLiteral("%1: %2").arg(label, error);
        Q_EMIT statusTextChanged();
        return false;
    }
    member = value;
    Q_EMIT statusTextChanged();
    (this->*changed)();
    return true;
}

void PointerDeviceSelection::setSpeed(double value) {
    const double clamped = qBound(MinimumSpeed, value, MaximumSpeed);
    if (clamped == m_speed) {
        return;
    }
    writeDouble(QStringLiteral("pointerAcceleration"), m_speed,
                &PointerDeviceSelection::speedChanged, clamped,
                tr("Pointer speed could not be changed"));
}

void PointerDeviceSelection::setFlatProfile(bool value) {
    if (value == m_flatProfile) {
        return;
    }
    QString error;
    if (!m_port.writeProperty(m_deviceId,
                              QStringLiteral(
                                  "pointerAccelerationProfileFlat"),
                              value, &error)) {
        m_statusText = tr("Acceleration profile could not be changed: %1")
                           .arg(error);
        Q_EMIT statusTextChanged();
        return;
    }
    if (!m_port.writeProperty(m_deviceId,
                              QStringLiteral(
                                  "pointerAccelerationProfileAdaptive"),
                              !value, &error)) {
        // AGENT-GUARD: The two profile flags are one choice. Restore the
        // flat flag when the adaptive write fails so the device never ends
        // up with both or neither selected.
        m_port.writeProperty(m_deviceId,
                             QStringLiteral(
                                 "pointerAccelerationProfileFlat"),
                             m_flatProfile, nullptr);
        m_statusText = tr("Acceleration profile could not be changed: %1")
                           .arg(error);
        Q_EMIT statusTextChanged();
        return;
    }
    m_flatProfile = value;
    Q_EMIT statusTextChanged();
    Q_EMIT profileChanged();
}

void PointerDeviceSelection::setNaturalScroll(bool value) {
    if (value == m_naturalScroll) {
        return;
    }
    writeBool(QStringLiteral("naturalScroll"), m_naturalScroll,
              &PointerDeviceSelection::naturalScrollChanged, value,
              tr("Natural scrolling could not be changed"));
}

void PointerDeviceSelection::setLeftHanded(bool value) {
    if (value == m_leftHanded) {
        return;
    }
    writeBool(QStringLiteral("leftHanded"), m_leftHanded,
              &PointerDeviceSelection::leftHandedChanged, value,
              tr("Left-handed mode could not be changed"));
}

void PointerDeviceSelection::setScrollSpeed(double value) {
    const double clamped = qBound(MinimumScrollSpeed, value, MaximumScrollSpeed);
    if (clamped == m_scrollSpeed) {
        return;
    }
    writeDouble(QStringLiteral("scrollFactor"), m_scrollSpeed,
                &PointerDeviceSelection::scrollSpeedChanged, clamped,
                tr("Scroll speed could not be changed"));
}

void PointerDeviceSelection::setMiddleEmulation(bool value) {
    if (value == m_middleEmulation) {
        return;
    }
    writeBool(QStringLiteral("middleEmulation"), m_middleEmulation,
              &PointerDeviceSelection::middleEmulationChanged, value,
              tr("Middle-click emulation could not be changed"));
}

void PointerDeviceSelection::setTapToClick(bool value) {
    if (value == m_tapToClick) {
        return;
    }
    writeBool(QStringLiteral("tapToClick"), m_tapToClick,
              &PointerDeviceSelection::tapToClickChanged, value,
              tr("Tap to click could not be changed"));
}

void PointerDeviceSelection::setTapAndDrag(bool value) {
    if (value == m_tapAndDrag) {
        return;
    }
    writeBool(QStringLiteral("tapAndDrag"), m_tapAndDrag,
              &PointerDeviceSelection::tapAndDragChanged, value,
              tr("Tap and drag could not be changed"));
}

void PointerDeviceSelection::setDisableWhileTyping(bool value) {
    if (value == m_disableWhileTyping) {
        return;
    }
    writeBool(QStringLiteral("disableWhileTyping"), m_disableWhileTyping,
              &PointerDeviceSelection::disableWhileTypingChanged, value,
              tr("Disable while typing could not be changed"));
}

void PointerDeviceSelection::setScrollMethod(const QString &value) {
    const bool twoFinger = value != QLatin1String("edge");
    const bool currentlyTwoFinger =
        snapshotBool(m_properties, QStringLiteral("scrollTwoFinger"), true);
    if (twoFinger == currentlyTwoFinger) {
        return;
    }
    QString error;
    if (!m_port.writeProperty(m_deviceId,
                              QStringLiteral("scrollTwoFinger"), twoFinger,
                              &error)) {
        m_statusText = tr("Scroll method could not be changed: %1").arg(error);
        Q_EMIT statusTextChanged();
        return;
    }
    if (!m_port.writeProperty(m_deviceId, QStringLiteral("scrollEdge"),
                              !twoFinger, &error)) {
        m_port.writeProperty(m_deviceId, QStringLiteral("scrollTwoFinger"),
                             currentlyTwoFinger, nullptr);
        m_statusText = tr("Scroll method could not be changed: %1").arg(error);
        Q_EMIT statusTextChanged();
        return;
    }
    m_scrollMethod = twoFinger ? QStringLiteral("two-finger")
                               : QStringLiteral("edge");
    Q_EMIT statusTextChanged();
    Q_EMIT scrollMethodChanged();
}

} // namespace QindaQt::Apps::SettingsInput
