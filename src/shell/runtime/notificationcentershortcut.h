// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QObject>

#include <functional>

class QAction;

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

class NotificationCenterShortcut final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool registrationRequestAccepted READ registrationRequestAccepted
                   CONSTANT)
    Q_PROPERTY(bool activeBindingPresent READ activeBindingPresent
                   NOTIFY activeBindingPresentChanged)

public:
    NotificationCenterShortcut(GlobalShortcutRegistrar &registrar,
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
