// SPDX-License-Identifier: GPL-3.0-or-later
#include "shortcutnoteshortcut.h"

#include "globalshortcutregistrar.h"

#include <QAction>

#include <utility>

namespace QindaQt::Shell {

ShortcutNoteShortcut::ShortcutNoteShortcut(
    GlobalShortcutRegistrar &registrar, std::function<void()> toggle,
    QObject *parent)
    : QObject(parent)
    , m_action(new QAction(this))
{
    m_action->setObjectName(stableActionId());
    m_action->setText(QStringLiteral("Toggle the QindaQt shortcut note"));
    connect(m_action, &QAction::triggered, this,
            [callback = std::move(toggle)] {
                if (callback) {
                    callback();
                }
            });
    const auto registration = registrar.registerShortcut(
        *m_action, defaultShortcut(), *this,
        [this](bool present) { setActiveBindingPresent(present); });
    m_registrationRequestAccepted = registration.requestAccepted;
    m_activeBindingPresent = registration.activeBindingPresent;
}

QString ShortcutNoteShortcut::stableActionId()
{
    return QStringLiteral("qindaqt_toggle_shortcut_note");
}

QKeySequence ShortcutNoteShortcut::defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::Key_F1);
}

QAction *ShortcutNoteShortcut::action() const noexcept
{
    return m_action;
}

bool ShortcutNoteShortcut::registrationRequestAccepted() const noexcept
{
    return m_registrationRequestAccepted;
}

bool ShortcutNoteShortcut::activeBindingPresent() const noexcept
{
    return m_activeBindingPresent;
}

void ShortcutNoteShortcut::setActiveBindingPresent(bool present)
{
    if (m_activeBindingPresent == present) {
        return;
    }
    m_activeBindingPresent = present;
    Q_EMIT activeBindingPresentChanged();
}

} // namespace QindaQt::Shell
