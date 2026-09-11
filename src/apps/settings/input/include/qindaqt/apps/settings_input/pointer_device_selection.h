// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantMap>

#include <qindaqt/apps/settings_input/pointer_device_port.h>

namespace QindaQt::Apps::SettingsInput {

// Presentation state of the selected pointer device: one writable property
// per user-visible row, each paired with an availability truth. The route
// hides a row when its availability property is false, so an unsupported
// control never renders as broken or as silently ignored (ADR-0134).
//
// AGENT-CONTRACT: Every writable property setter dispatches through the
// port and reverts on failure; success alone moves the presented value.
// QML must bind to these properties and never write device state directly.
class PointerDeviceSelection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool speedAvailable READ speedAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool flatProfile READ flatProfile WRITE setFlatProfile NOTIFY
                   profileChanged)
    Q_PROPERTY(bool profileAvailable READ profileAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool naturalScroll READ naturalScroll WRITE setNaturalScroll
                   NOTIFY naturalScrollChanged)
    Q_PROPERTY(bool naturalScrollAvailable READ naturalScrollAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool leftHanded READ leftHanded WRITE setLeftHanded NOTIFY
                   leftHandedChanged)
    Q_PROPERTY(bool leftHandedAvailable READ leftHandedAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(double scrollSpeed READ scrollSpeed WRITE setScrollSpeed NOTIFY
                   scrollSpeedChanged)
    Q_PROPERTY(bool scrollSpeedAvailable READ scrollSpeedAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool middleEmulation READ middleEmulation WRITE
                   setMiddleEmulation NOTIFY middleEmulationChanged)
    Q_PROPERTY(bool middleEmulationAvailable READ middleEmulationAvailable
                   NOTIFY availabilityChanged)
    Q_PROPERTY(bool tapToClick READ tapToClick WRITE setTapToClick NOTIFY
                   tapToClickChanged)
    Q_PROPERTY(bool tapToClickAvailable READ tapToClickAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool tapAndDrag READ tapAndDrag WRITE setTapAndDrag NOTIFY
                   tapAndDragChanged)
    Q_PROPERTY(bool tapAndDragAvailable READ tapAndDragAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(bool disableWhileTyping READ disableWhileTyping WRITE
                   setDisableWhileTyping NOTIFY disableWhileTypingChanged)
    Q_PROPERTY(bool disableWhileTypingAvailable READ
                   disableWhileTypingAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(QString scrollMethod READ scrollMethod WRITE setScrollMethod
                   NOTIFY scrollMethodChanged)
    Q_PROPERTY(bool scrollMethodAvailable READ scrollMethodAvailable NOTIFY
                   availabilityChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    PointerDeviceSelection(const PointerDevicePort &port,
                           QObject *parent = nullptr);

    // Replaces the presented device wholesale. Availability is derived from
    // the snapshot's supports*/property presence and the device kind; a
    // touchpad-only property is unavailable on a plain pointer even when
    // the authority echoes the key.
    void setSnapshot(const PointerDeviceSnapshot &snapshot);

    [[nodiscard]] double speed() const { return m_speed; }
    [[nodiscard]] bool speedAvailable() const { return m_speedAvailable; }
    [[nodiscard]] bool flatProfile() const { return m_flatProfile; }
    [[nodiscard]] bool profileAvailable() const { return m_profileAvailable; }
    [[nodiscard]] bool naturalScroll() const { return m_naturalScroll; }
    [[nodiscard]] bool naturalScrollAvailable() const {
        return m_naturalScrollAvailable;
    }
    [[nodiscard]] bool leftHanded() const { return m_leftHanded; }
    [[nodiscard]] bool leftHandedAvailable() const {
        return m_leftHandedAvailable;
    }
    [[nodiscard]] double scrollSpeed() const { return m_scrollSpeed; }
    [[nodiscard]] bool scrollSpeedAvailable() const {
        return m_scrollSpeedAvailable;
    }
    [[nodiscard]] bool middleEmulation() const { return m_middleEmulation; }
    [[nodiscard]] bool middleEmulationAvailable() const {
        return m_middleEmulationAvailable;
    }
    [[nodiscard]] bool tapToClick() const { return m_tapToClick; }
    [[nodiscard]] bool tapToClickAvailable() const {
        return m_tapToClickAvailable;
    }
    [[nodiscard]] bool tapAndDrag() const { return m_tapAndDrag; }
    [[nodiscard]] bool tapAndDragAvailable() const {
        return m_tapAndDragAvailable;
    }
    [[nodiscard]] bool disableWhileTyping() const {
        return m_disableWhileTyping;
    }
    [[nodiscard]] bool disableWhileTypingAvailable() const {
        return m_disableWhileTypingAvailable;
    }
    [[nodiscard]] QString scrollMethod() const { return m_scrollMethod; }
    [[nodiscard]] bool scrollMethodAvailable() const {
        return m_scrollMethodAvailable;
    }
    [[nodiscard]] QString statusText() const { return m_statusText; }

    void setSpeed(double value);
    void setFlatProfile(bool value);
    void setNaturalScroll(bool value);
    void setLeftHanded(bool value);
    void setScrollSpeed(double value);
    void setMiddleEmulation(bool value);
    void setTapToClick(bool value);
    void setTapAndDrag(bool value);
    void setDisableWhileTyping(bool value);
    void setScrollMethod(const QString &value);

Q_SIGNALS:
    void speedChanged();
    void profileChanged();
    void naturalScrollChanged();
    void leftHandedChanged();
    void scrollSpeedChanged();
    void middleEmulationChanged();
    void tapToClickChanged();
    void tapAndDragChanged();
    void disableWhileTypingChanged();
    void scrollMethodChanged();
    void availabilityChanged();
    void statusTextChanged();

private:
    bool writeBool(const QString &property, bool &member,
                   void (PointerDeviceSelection::*changed)(), bool value,
                   const QString &label);
    bool writeDouble(const QString &property, double &member,
                     void (PointerDeviceSelection::*changed)(), double value,
                     const QString &label);

    const PointerDevicePort &m_port;
    QString m_deviceId;
    QString m_name;
    bool m_touchpad = false;
    QVariantMap m_properties;
    double m_speed = 0.0;
    bool m_flatProfile = false;
    bool m_naturalScroll = false;
    bool m_leftHanded = false;
    double m_scrollSpeed = 1.0;
    bool m_middleEmulation = false;
    bool m_tapToClick = false;
    bool m_tapAndDrag = false;
    bool m_disableWhileTyping = false;
    QString m_scrollMethod = QStringLiteral("two-finger");
    bool m_speedAvailable = false;
    bool m_profileAvailable = false;
    bool m_naturalScrollAvailable = false;
    bool m_leftHandedAvailable = false;
    bool m_scrollSpeedAvailable = false;
    bool m_middleEmulationAvailable = false;
    bool m_tapToClickAvailable = false;
    bool m_tapAndDragAvailable = false;
    bool m_disableWhileTypingAvailable = false;
    bool m_scrollMethodAvailable = false;
    QString m_statusText;
};

} // namespace QindaQt::Apps::SettingsInput
