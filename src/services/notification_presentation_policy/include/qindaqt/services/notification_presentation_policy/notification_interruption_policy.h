// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Services::NotificationPresentation {
struct PresentationNotification;
}

namespace QindaQt::Services::NotificationPresentationPolicy {

class NotificationInterruptionPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool doNotDisturbEnabled READ doNotDisturbEnabled
                   WRITE setDoNotDisturbEnabled NOTIFY doNotDisturbEnabledChanged)

public:
    // AGENT-CONTRACT: The policy owns session-volatile state only. Like every
    // QObject, it is confined to its affinity thread; callers own and retain
    // any persistence or cross-thread synchronization outside this module.
    explicit NotificationInterruptionPolicy(QObject *parent = nullptr);

    [[nodiscard]] bool doNotDisturbEnabled() const noexcept;
    void setDoNotDisturbEnabled(bool enabled);

    // The notification is borrowed only for this call. Admission is total and
    // cannot fail: DND admits only protocol-valid critical urgency (2), while
    // the disabled state deliberately admits every value.
    [[nodiscard]] bool allowsPopup(
        const NotificationPresentation::PresentationNotification &notification) const
        noexcept;

Q_SIGNALS:
    void doNotDisturbEnabledChanged(bool enabled);

private:
    bool m_doNotDisturbEnabled = false;
};

} // namespace QindaQt::Services::NotificationPresentationPolicy
