// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

#include <functional>

namespace QindaQt::Services::NotificationPresentation {
struct PresentationNotification;
}

namespace QindaQt::Services::NotificationPresentationPolicy {

class NotificationInterruptionPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool doNotDisturbEnabled READ doNotDisturbEnabled
                   WRITE setDoNotDisturbEnabled NOTIFY doNotDisturbEnabledChanged)

public:
    // Minutes since local midnight, 0..1439. Injected so the quiet-hours
    // window is testable without waiting for 22:00; the default reads the
    // local wall clock.
    //
    // AGENT-CONTRACT: the clock is called from allowsPopup(), which is
    // noexcept. It must not throw, and a value outside 0..1439 is read as
    // "not in the window" rather than clamped.
    using MinuteOfDayClock = std::function<int()>;

    // AGENT-CONTRACT: quiet hours are evaluated when a notification is
    // admitted and whenever the policy changes -- the policy owns no timer,
    // so a popup already on screen when the window opens is not retroactively
    // withdrawn. It expires on its own schedule as it always did.
    struct QuietHours final {
        bool enabled = false;
        // Both are minutes since midnight. A window whose ends are equal is
        // empty, never "all day": that is the reading a user who set the same
        // time twice expects, and it is the one that cannot silence a machine
        // forever by accident.
        int startMinutes = 22 * 60;
        int endMinutes = 7 * 60;

        [[nodiscard]] bool operator==(const QuietHours &) const = default;
    };

    // AGENT-CONTRACT: The policy owns session-volatile state only. Like every
    // QObject, it is confined to its affinity thread; callers own and retain
    // any persistence or cross-thread synchronization outside this module.
    explicit NotificationInterruptionPolicy(QObject *parent = nullptr);

    [[nodiscard]] bool doNotDisturbEnabled() const noexcept;
    void setDoNotDisturbEnabled(bool enabled);

    [[nodiscard]] QuietHours quietHours() const noexcept;
    // Out-of-range minutes are refused as a whole: a half-applied window would
    // quiet the machine at a time the user never chose.
    void setQuietHours(const QuietHours &quietHours);
    void setClock(MinuteOfDayClock clock);
    // True when the configured window contains the clock's current minute.
    [[nodiscard]] bool quietHoursActive() const;
    // Whether this minute-of-day falls inside the window, wrap included.
    [[nodiscard]] static bool windowContains(const QuietHours &quietHours, int minuteOfDay) noexcept;

    // The notification is borrowed only for this call. Admission is total and
    // cannot fail: DND admits only protocol-valid critical urgency (2), while
    // the disabled state deliberately admits every value.
    [[nodiscard]] bool allowsPopup(
        const NotificationPresentation::PresentationNotification &notification) const
        noexcept;

Q_SIGNALS:
    void doNotDisturbEnabledChanged(bool enabled);
    void quietHoursChanged();

private:
    bool m_doNotDisturbEnabled = false;
    QuietHours m_quietHours;
    MinuteOfDayClock m_clock;
};

} // namespace QindaQt::Services::NotificationPresentationPolicy
