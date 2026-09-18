// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtDBus/QDBusConnection>

#include <QHash>
#include <QObject>
#include <QString>

namespace QindaQt::Session::DesktopControls {

// The one notification a pen display earns: it announces itself, offers the
// settings it needs, and then never asks again.
//
// AGENT-CONTRACT: Action keys are part of the user-visible contract with the
// notification host; `setup`, `internal` and `dismiss` are the exact keys
// the ActionInvoked signal carries back. Each device group owns at most one
// live notification id, so a re-announced tablet replaces its own popup
// instead of stacking a second one.
class TabletArrivalNotifier final : public QObject {
    Q_OBJECT

public:
    explicit TabletArrivalNotifier(QDBusConnection connection,
                                   QObject *parent = nullptr);
    ~TabletArrivalNotifier() override;

    TabletArrivalNotifier(const TabletArrivalNotifier &) = delete;
    TabletArrivalNotifier &operator=(const TabletArrivalNotifier &) = delete;

    // Subscribes to ActionInvoked. Returns false with a diagnostic when the
    // signal cannot be observed; announcements still post, their buttons
    // just do nothing, which is honest but degraded.
    bool start(QString *error = nullptr);

    // `outputName` empty means the tablet has no screen of its own; the text
    // then says so instead of naming a screen that was never chosen.
    void announce(const QString &deviceGroupId, const QString &deviceName,
                  const QString &outputName);

    [[nodiscard]] static QString setupActionKey();
    [[nodiscard]] static QString useInternalActionKey();
    [[nodiscard]] static QString dismissActionKey();

    // Body text for one announcement, exported so a row can assert the exact
    // sentence a user reads without a notification host.
    [[nodiscard]] static QString bodyText(const QString &outputName);

Q_SIGNALS:
    // "Set up pen display": open Settings at Pen & tablet with this group
    // selected.
    void setupRequested(const QString &deviceGroupId);
    // "Use this screen instead": map the tablet back to the active screen.
    void useActiveScreenRequested(const QString &deviceGroupId);
    // The host accepted an announcement and gave it this id. Emitted after
    // the asynchronous Notify reply lands, so a caller (and a test row) can
    // tell "asked to show" from "actually shown".
    void announcementShown(const QString &deviceGroupId,
                           quint32 notificationId);
    void notificationFailed(const QString &message);

private Q_SLOTS:
    void handleActionInvoked(quint32 notificationId, const QString &actionKey);

private:
    QDBusConnection m_connection;
    // notification id -> device group, so a late action still names the
    // tablet it belongs to.
    QHash<quint32, QString> m_liveNotifications;
    // device group -> notification id, so a re-announcement replaces.
    QHash<QString, quint32> m_idsByGroup;
    bool m_started = false;
};

} // namespace QindaQt::Session::DesktopControls
