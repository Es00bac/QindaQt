// SPDX-License-Identifier: GPL-3.0-or-later
#include "paneleditkeyboard.h"

#include "livecustomizationcontroller.h"

#include <QEvent>
#include <QKeyEvent>
#include <QWindow>

#include <utility>

namespace QindaQt::Shell {

PanelEditKeyboard::PanelEditKeyboard(LiveCustomizationController &controller,
                                     KeyboardPolicy setKeyboard, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_setKeyboard(std::move(setKeyboard))
{
    connect(&m_controller, &LiveCustomizationController::editModeChanged, this,
            &PanelEditKeyboard::applyAll);
}

void PanelEditKeyboard::attach(QWindow *window)
{
    if (window == nullptr) {
        return;
    }
    m_windows.removeIf([](const QPointer<QWindow> &entry) { return entry.isNull(); });
    for (const QPointer<QWindow> &entry : std::as_const(m_windows)) {
        if (entry == window) {
            return;
        }
    }
    m_windows.append(QPointer<QWindow>(window));
    window->installEventFilter(this);
    if (m_setKeyboard && m_controller.editMode()) {
        m_setKeyboard(window, true);
    }
}

void PanelEditKeyboard::applyAll()
{
    if (!m_setKeyboard) {
        return;
    }
    const bool wanted = m_controller.editMode();
    for (const QPointer<QWindow> &window : std::as_const(m_windows)) {
        if (window) {
            m_setKeyboard(window, wanted);
        }
    }
}

bool PanelEditKeyboard::eventFilter(QObject *watched, QEvent *event)
{
    auto *window = qobject_cast<QWindow *>(watched);
    if (window == nullptr || !m_controller.editMode()) {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Show && m_setKeyboard) {
        m_setKeyboard(window, true);
    } else if (event->type() == QEvent::KeyPress
               && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        // Escape backs out one level: an open drag first, then edit mode.
        if (m_controller.dragActive()) {
            const bool cancelled = m_controller.cancelDrag();
            Q_UNUSED(cancelled);
        } else {
            m_controller.exitEditMode();
        }
        return true;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace QindaQt::Shell
