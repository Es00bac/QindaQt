// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

class QQuickWindow;

namespace QindaQt::Shell {

// A panel surface that can be lent keyboard focus.
//
// AGENT-CONTRACT: panels are published with layer-shell keyboard
// interactivity None (ADR-0217, layer_shell_surface_backend.cpp) so typing
// never lands in the panel instead of the focused application. A popup that
// takes typed input is the one exception, and it is a loan: the grant lasts
// exactly as long as that popup is open.
class PanelFocusTarget : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~PanelFocusTarget() override = default;

    virtual void grantKeyboardFocus() = 0;
    virtual void revokeKeyboardFocus() = 0;
    [[nodiscard]] virtual bool hasKeyboardFocus() const = 0;

Q_SIGNALS:
    void keyboardFocusChanged();
};

// Layer-shell implementation over whichever panel window currently hosts the
// launcher. The window is resolved on every call rather than held: panels are
// republished whenever outputs or the layout change, and a stale pointer would
// either crash or lend focus to a window that is no longer on screen.
class LayerShellPanelFocusTarget final : public PanelFocusTarget {
    Q_OBJECT

public:
    using WindowResolver = std::function<QQuickWindow *()>;

    explicit LayerShellPanelFocusTarget(WindowResolver resolver,
                                        QObject *parent = nullptr);

    void grantKeyboardFocus() override;
    void revokeKeyboardFocus() override;
    [[nodiscard]] bool hasKeyboardFocus() const override;

private:
    [[nodiscard]] QQuickWindow *resolve();

    WindowResolver m_resolver;
    QPointer<QQuickWindow> m_observed;
};

// Opens the launcher on behalf of a global shortcut.
//
// AGENT-NOTE: a Wayland popup that grabs input needs a serial from a real
// input event. A pointer click on the panel supplies one; a global shortcut
// does not, and Qt then refuses the popup with "Failed to create grabbing
// popup". Lending the panel keyboard focus first gives the popup a keyboard
// enter to grab with. The wait is bounded: if focus never arrives the open is
// attempted anyway, because a launcher that does not appear is worse than one
// that appears without a grab.
class LauncherKeyboardFocusRelay final : public QObject {
    Q_OBJECT

public:
    explicit LauncherKeyboardFocusRelay(PanelFocusTarget &target,
                                        QObject *parent = nullptr);

    [[nodiscard]] static int focusWaitMilliseconds() noexcept { return 400; }

public Q_SLOTS:
    void requestOpen();
    void browserClosed();

Q_SIGNALS:
    // The panel is ready to host a grabbing popup; open the browser now.
    void openNow();

private:
    void openAndStopWaiting();

    PanelFocusTarget &m_target;
    QTimer m_focusWait;
    bool m_waiting = false;
    bool m_holdingGrant = false;
};

} // namespace QindaQt::Shell
