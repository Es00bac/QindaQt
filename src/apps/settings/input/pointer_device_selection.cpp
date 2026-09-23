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

// AGENT-GUARD: KWin's real InputDevice interface has no boolean
// supportsTapToClick/supportsTapAndDrag property; tap capability is the
// integer tapFingerCount (0 = the device cannot tap at all). Both tap-to-click
// and tap-and-drag share this one capability signal.
bool snapshotHasTapCapability(const QVariantMap &properties) {
    const QVariant value = properties.value(QStringLiteral("tapFingerCount"));
    return value.canConvert<int>() && value.toInt() > 0;
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
    ++m_generation;
    const bool wasBusy = m_busy;
    m_busy = false;
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
    m_tapToClickAvailable = m_touchpad && snapshotHasTapCapability(m_properties);
    m_tapToClick = snapshotBool(m_properties, QStringLiteral("tapToClick"));
    m_tapAndDragAvailable = m_touchpad && snapshotHasTapCapability(m_properties);
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
    // Complete the state replacement before releasing deferred refreshes.
    if (wasBusy) Q_EMIT busyChanged();
}

void PointerDeviceSelection::apply(
    const QList<QPair<QString, QVariant>> &changes,
    const QString &failureLabel) {
    if (m_deviceId.isEmpty() || m_busy) return;
    m_busy = true;
    Q_EMIT busyChanged();
    const quint64 generation = ++m_generation;
    const QString id = m_deviceId;
    m_port.requestWrite(this, id, changes,
                        [this, generation, id, failureLabel](
                            PointerDevicePort::WriteResult result) {
        // AGENT-GUARD: A reply for selected A must never paint B, and a
        // reply from a removed/old-owner device must never restore controls.
        if (generation != m_generation || id != m_deviceId) {
            Q_EMIT refreshRequested();
            return;
        }
        if (result.snapshot) {
            Q_EMIT confirmedSnapshot(*result.snapshot);
            setSnapshot(*result.snapshot);
        } else {
            m_busy = false;
            Q_EMIT busyChanged();
            Q_EMIT refreshRequested();
        }
        if (!result.applied) {
            m_statusText = failureLabel + QStringLiteral(": ") +
                           (result.error.isEmpty()
                                ? tr("The input authority refused the change")
                                : result.error);
            Q_EMIT statusTextChanged();
        }
    });
}

void PointerDeviceSelection::setSpeed(double value) {
    const double clamped = qBound(MinimumSpeed, value, MaximumSpeed);
    if (clamped != m_speed) {
        apply({{QStringLiteral("pointerAcceleration"), clamped}},
              tr("Pointer speed could not be changed"));
    }
}

void PointerDeviceSelection::setFlatProfile(bool value) {
    if (value != m_flatProfile) {
        apply({{QStringLiteral("pointerAccelerationProfileFlat"), value},
               {QStringLiteral("pointerAccelerationProfileAdaptive"), !value}},
              tr("Acceleration profile could not be changed"));
    }
}

void PointerDeviceSelection::setNaturalScroll(bool value) {
    if (value != m_naturalScroll) {
        apply({{QStringLiteral("naturalScroll"), value}},
              tr("Natural scrolling could not be changed"));
    }
}

void PointerDeviceSelection::setLeftHanded(bool value) {
    if (value != m_leftHanded) {
        apply({{QStringLiteral("leftHanded"), value}},
              tr("Left-handed mode could not be changed"));
    }
}

void PointerDeviceSelection::setScrollSpeed(double value) {
    const double clamped = qBound(MinimumScrollSpeed, value, MaximumScrollSpeed);
    if (clamped != m_scrollSpeed) {
        apply({{QStringLiteral("scrollFactor"), clamped}},
              tr("Scroll speed could not be changed"));
    }
}

void PointerDeviceSelection::setMiddleEmulation(bool value) {
    if (value != m_middleEmulation) {
        apply({{QStringLiteral("middleEmulation"), value}},
              tr("Middle-click emulation could not be changed"));
    }
}

void PointerDeviceSelection::setTapToClick(bool value) {
    if (value != m_tapToClick) {
        apply({{QStringLiteral("tapToClick"), value}},
              tr("Tap to click could not be changed"));
    }
}

void PointerDeviceSelection::setTapAndDrag(bool value) {
    if (value != m_tapAndDrag) {
        apply({{QStringLiteral("tapAndDrag"), value}},
              tr("Tap and drag could not be changed"));
    }
}

void PointerDeviceSelection::setDisableWhileTyping(bool value) {
    if (value != m_disableWhileTyping) {
        apply({{QStringLiteral("disableWhileTyping"), value}},
              tr("Disable while typing could not be changed"));
    }
}

void PointerDeviceSelection::setScrollMethod(const QString &value) {
    const bool twoFinger = value != QLatin1String("edge");
    const bool currentlyTwoFinger =
        snapshotBool(m_properties, QStringLiteral("scrollTwoFinger"), true);
    if (twoFinger != currentlyTwoFinger) {
        apply({{QStringLiteral("scrollTwoFinger"), twoFinger},
               {QStringLiteral("scrollEdge"), !twoFinger}},
              tr("Scroll method could not be changed"));
    }
}

} // namespace QindaQt::Apps::SettingsInput
