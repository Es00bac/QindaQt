// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtDBus/QDBusConnection>

#include <QObject>
#include <QString>

namespace QindaQt::Session::DesktopControls {

// Visible media-key feedback through the resident org.freedesktop.Notifications
// host. Each category reuses one server-side notification id so repeated key
// presses update one popup in place instead of stacking. Transport failures
// reset the category id and are reported through notificationFailed only.
class FreedesktopFeedbackNotifier final : public QObject {
    Q_OBJECT

public:
    explicit FreedesktopFeedbackNotifier(QDBusConnection connection,
                                         QObject *parent = nullptr);
    ~FreedesktopFeedbackNotifier() override;

    FreedesktopFeedbackNotifier(const FreedesktopFeedbackNotifier &) = delete;
    FreedesktopFeedbackNotifier &operator=(const FreedesktopFeedbackNotifier &) = delete;

    void showVolume(int percent, bool muted);
    void showBrightness(int percent);
    void showNotice(const QString &summary, const QString &body, const QString &iconName);
    // Battery warnings get their own replaces-id (never collides with
    // volume/brightness/notice), a "critical" urgency hint for `critical`
    // (desktop notification hosts must not auto-dismiss or DND-hide those,
    // per the freedesktop spec), and expire_timeout 0 (stays until the user
    // dismisses it or the level changes) instead of the 1.2 s OSD flash used
    // for key feedback.
    void showBattery(const QString &summary, const QString &body,
                     const QString &iconName, bool critical);

Q_SIGNALS:
    void notificationFailed(const QString &message);

private:
    void notify(const QString &category, const QString &iconName,
                const QString &summary, const QString &body,
                int expireMilliseconds, quint8 urgency);
    [[nodiscard]] static QString volumeIcon(int percent, bool muted);

    QDBusConnection m_connection;
    quint32 m_volumeNotificationId = 0;
    quint32 m_brightnessNotificationId = 0;
    quint32 m_noticeNotificationId = 0;
    quint32 m_batteryNotificationId = 0;
};

} // namespace QindaQt::Session::DesktopControls
