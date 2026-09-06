// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QObject>

#include <functional>

class QAction;

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

// Global toggle for the desktop shortcut note. The registrar is borrowed for
// the constructor only; registration completes synchronously and no callback
// retains it, so the registrar object may be destroyed after construction.
class ShortcutNoteShortcut final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool registrationRequestAccepted READ registrationRequestAccepted
                   CONSTANT)
    Q_PROPERTY(bool activeBindingPresent READ activeBindingPresent
                   NOTIFY activeBindingPresentChanged)

public:
    explicit ShortcutNoteShortcut(GlobalShortcutRegistrar &registrar,
                                  std::function<void()> toggle,
                                  QObject *parent = nullptr);

    [[nodiscard]] static QString stableActionId();
    [[nodiscard]] static QKeySequence defaultShortcut();
    [[nodiscard]] QAction *action() const noexcept;
    [[nodiscard]] bool registrationRequestAccepted() const noexcept;
    [[nodiscard]] bool activeBindingPresent() const noexcept;

Q_SIGNALS:
    void activeBindingPresentChanged();

private:
    void setActiveBindingPresent(bool present);

    QAction *m_action = nullptr;
    bool m_registrationRequestAccepted = false;
    bool m_activeBindingPresent = false;
};

} // namespace QindaQt::Shell
