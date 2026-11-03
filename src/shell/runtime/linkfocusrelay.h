// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qindaqt/compositor/shellwindowidentity.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <optional>

namespace QindaQt::Shell {

// Relays the focused window to org.qindaqt.Link1.Focus.Publish (ADR-0440). Only the shell may do this:
// the authenticated active-window identity (ADR-0063) belongs to the panel owner, and the link service
// accepts Publish only from `qindaqt-shell`.
//
// The fact is {uuid, appId, title, revision}: identity from the compositor snapshot, application id and
// title from the task list. Anything missing, unavailable or unauthenticated publishes "{}" (focus
// unknown), so the service refuses typing instead of trusting a stale fact.
//
// Ownership: borrows nothing but the injected provider. Threading: GUI thread. Error behavior: a failed
// call is retried on the next refresh; the service not running is not an error (no call is made, and
// the service is never D-Bus-activated by this relay). A Denied answer is warned about once.
// AGENT-GUARD: never log the title or app id; they are window contents.
class LinkFocusRelay final : public QObject
{
    Q_OBJECT

public:
    // Returns the JSON to publish right now ("{}" when unknown).
    using Provider = std::function<QString()>;
    // Resolves a compositor window id to its application id and display title.
    using WindowFacts = std::function<bool(const QString &windowId, QString *appId, QString *title)>;

    LinkFocusRelay(QDBusConnection bus, Provider provider, QObject *parent = nullptr);

    // Starts watching the service name and publishes once a second (heartbeat; also notices title changes).
    void start();
    // Call on every focus change; publishes when the fact differs from the last one the service accepted.
    // With `heartbeat` it republishes an unchanged fact too: the service treats a fact older than
    // about 3 s as unknown (FocusStore::kStaleMs), so a hung shell stops being a focus authority.
    void refresh(bool heartbeat = false);

    [[nodiscard]] static QString factJson(
        const std::optional<Compositor::ShellWindowIdentitySnapshot> &identity,
        const WindowFacts &facts);

private:
    void publish(const QString &json);

    QDBusConnection m_bus;
    Provider m_provider;
    QDBusServiceWatcher m_watcher;
    QTimer m_timer;
    QString m_lastAccepted;
    QString m_inFlight;
    bool m_serviceUp = false;
    bool m_warnedDenied = false;
};

} // namespace QindaQt::Shell
