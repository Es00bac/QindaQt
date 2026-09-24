// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include <functional>

class QWindow;

namespace QindaQt::Shell {

class LiveCustomizationController;

// The keyboard half of panel edit mode (ADR-0266). Panel surfaces never take
// keyboard focus, so while edit mode is on this asks every attached panel
// window for on-demand keyboard focus (a click on a panel focuses it) and
// turns Escape on a panel into "cancel the open drag, else leave edit mode".
// Leaving edit mode hands the keyboard back.
//
// AGENT-CONTRACT: GUI thread only. The controller is borrowed and must
// outlive this object (the runtime parents this to it). `setKeyboard` is the
// platform seam (LayerShellQt interactivity in the shell, a recorder in
// tests); it runs for every attached window whenever edit mode changes, and
// again when a window is shown during edit mode, because the layer-shell
// backend configures every new panel surface with no keyboard.
class PanelEditKeyboard final : public QObject {
    Q_OBJECT

public:
    using KeyboardPolicy = std::function<void(QWindow *window, bool wantsKeyboard)>;

    PanelEditKeyboard(LiveCustomizationController &controller, KeyboardPolicy setKeyboard,
                      QObject *parent = nullptr);

    // Idempotent; windows are tracked weakly and may be destroyed any time.
    void attach(QWindow *window);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyAll();

    LiveCustomizationController &m_controller;
    KeyboardPolicy m_setKeyboard;
    QList<QPointer<QWindow>> m_windows;
};

} // namespace QindaQt::Shell
