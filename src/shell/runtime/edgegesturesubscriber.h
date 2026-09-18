// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <functional>

namespace QindaQt::Shell {

// What each touch-edge action opens, as callables the runtime supplies.
// Pure dispatch, so the mapping is testable without a bus.
struct EdgeGestureHandlers {
    std::function<void()> overview;
    std::function<void()> notifications;
    std::function<void()> taskSwitcher;
};

// Returns true when `action` named a handler that was invoked.
[[nodiscard]] bool dispatchEdgeGesture(const QString &action, const EdgeGestureHandlers &handlers);

// Listens for the compositor's EdgeGestureTriggered(edge, action) signal
// (org.qindaqt.Compositor1, ADR-0205) and dispatches it.
class EdgeGestureSubscriber final : public QObject {
    Q_OBJECT

public:
    explicit EdgeGestureSubscriber(EdgeGestureHandlers handlers,
                                   QDBusConnection connection = QDBusConnection::sessionBus(),
                                   QObject *parent = nullptr);
    ~EdgeGestureSubscriber() override;

    [[nodiscard]] bool subscribed() const noexcept { return m_subscribed; }

Q_SIGNALS:
    void gestureReceived(const QString &edge, const QString &action, bool handled);

private Q_SLOTS:
    void handleGesture(const QString &edge, const QString &action);

private:
    EdgeGestureHandlers m_handlers;
    QDBusConnection m_connection;
    bool m_subscribed = false;
};

// Asks KWin's task switcher to open through KGlobalAccel, the only public
// door to it; harmless where no global shortcut daemon runs.
void invokeWalkThroughWindows(const QDBusConnection &connection);

} // namespace QindaQt::Shell
