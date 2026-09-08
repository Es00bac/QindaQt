// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "shortcut_registration.h"

#include <QAction>
#include <QKeySequence>
#include <QObject>

#include <array>
#include <functional>

namespace QindaQt::Session::DesktopControls {

enum class DesktopShortcutAction {
    VolumeUp = 0,
    VolumeDown,
    ToggleMute,
    BrightnessUp,
    BrightnessDown,
    TakeScreenshot,
    Count,
};

struct DesktopShortcutTriggers final {
    std::function<void()> volumeUp;
    std::function<void()> volumeDown;
    std::function<void()> toggleMute;
    std::function<void()> brightnessUp;
    std::function<void()> brightnessDown;
    std::function<void()> takeScreenshot;
};

struct DesktopShortcutRegistrationOptions final {
    // PowerDevil owns monitor-brightness and Spectacle owns Print in production.
    // Tests and embedders can opt in when they provide those owners themselves.
    bool registerBrightness = true;
    bool registerScreenshot = true;
};

// One QAction per media key, registered through the injected registrar with
// stable action ids so user remapping persists across sessions. The set owns
// no policy: triggers are borrowed callbacks into the key controllers.
class DesktopShortcutSet final : public QObject {
    Q_OBJECT

public:
    DesktopShortcutSet(ShortcutRegistrar &registrar,
                       DesktopShortcutTriggers triggers, QObject *parent = nullptr,
                       DesktopShortcutRegistrationOptions options = {});
    ~DesktopShortcutSet() override;

    DesktopShortcutSet(const DesktopShortcutSet &) = delete;
    DesktopShortcutSet &operator=(const DesktopShortcutSet &) = delete;

    [[nodiscard]] static QKeySequence defaultShortcut(DesktopShortcutAction action);
    [[nodiscard]] static QString stableActionId(DesktopShortcutAction action);

    [[nodiscard]] QAction *action(DesktopShortcutAction action) const;
    [[nodiscard]] bool registrationRequestAccepted(DesktopShortcutAction action) const;
    [[nodiscard]] bool activeBindingPresent(DesktopShortcutAction action) const;

Q_SIGNALS:
    void activeBindingPresentChanged(DesktopShortcutAction action, bool present);

private:
    void setActiveBindingPresent(DesktopShortcutAction action, bool present);

    std::array<QAction *, static_cast<std::size_t>(DesktopShortcutAction::Count)> m_actions{};
    std::array<bool, static_cast<std::size_t>(DesktopShortcutAction::Count)>
        m_requestAccepted{};
    std::array<bool, static_cast<std::size_t>(DesktopShortcutAction::Count)>
        m_activeBindingPresent{};
};

} // namespace QindaQt::Session::DesktopControls

Q_DECLARE_METATYPE(QindaQt::Session::DesktopControls::DesktopShortcutAction)
