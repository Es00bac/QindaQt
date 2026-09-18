// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QObject>

#include <functional>

class QAction;

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

// Global edit-mode toggle for live customization (Meta+Shift+E). Same
// contract as ShortcutNoteShortcut: the registrar is borrowed for the
// constructor only and registration completes synchronously.
class LiveCustomizationShortcut final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool registrationRequestAccepted READ registrationRequestAccepted CONSTANT)
    Q_PROPERTY(bool activeBindingPresent READ activeBindingPresent
                   NOTIFY activeBindingPresentChanged)

public:
    explicit LiveCustomizationShortcut(GlobalShortcutRegistrar &registrar,
                                       std::function<void()> toggle,
                                       QObject *parent = nullptr);

    [[nodiscard]] static QString stableActionId();
    [[nodiscard]] static QKeySequence defaultShortcut();
    [[nodiscard]] QAction *action() const noexcept { return m_action; }
    [[nodiscard]] bool registrationRequestAccepted() const noexcept
    {
        return m_registrationRequestAccepted;
    }
    [[nodiscard]] bool activeBindingPresent() const noexcept { return m_activeBindingPresent; }

Q_SIGNALS:
    void activeBindingPresentChanged();

private:
    QAction *m_action = nullptr;
    bool m_registrationRequestAccepted = false;
    bool m_activeBindingPresent = false;
};

} // namespace QindaQt::Shell
