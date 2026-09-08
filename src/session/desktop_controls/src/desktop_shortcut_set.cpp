// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/desktop_shortcut_set.h"

#include <QtGlobal>

#include <utility>

namespace QindaQt::Session::DesktopControls {
namespace {

struct ShortcutSpec final {
    const char *actionId;
    const char *text;
    Qt::Key key;
};

constexpr std::array<ShortcutSpec,
                     static_cast<std::size_t>(DesktopShortcutAction::Count)>
    kShortcutSpecs{{
        {"qindaqt_volume_up", "Raise volume", Qt::Key_VolumeUp},
        {"qindaqt_volume_down", "Lower volume", Qt::Key_VolumeDown},
        {"qindaqt_volume_mute", "Toggle mute", Qt::Key_VolumeMute},
        {"qindaqt_brightness_up", "Raise brightness", Qt::Key_MonBrightnessUp},
        {"qindaqt_brightness_down", "Lower brightness", Qt::Key_MonBrightnessDown},
        {"qindaqt_take_screenshot", "Take screenshot", Qt::Key_Print},
    }};

std::function<void()> triggerFor(const DesktopShortcutTriggers &triggers,
                                 DesktopShortcutAction action)
{
    switch (action) {
    case DesktopShortcutAction::VolumeUp:
        return triggers.volumeUp;
    case DesktopShortcutAction::VolumeDown:
        return triggers.volumeDown;
    case DesktopShortcutAction::ToggleMute:
        return triggers.toggleMute;
    case DesktopShortcutAction::BrightnessUp:
        return triggers.brightnessUp;
    case DesktopShortcutAction::BrightnessDown:
        return triggers.brightnessDown;
    case DesktopShortcutAction::TakeScreenshot:
        return triggers.takeScreenshot;
    case DesktopShortcutAction::Count:
        break;
    }
    return nullptr;
}

} // namespace

DesktopShortcutSet::DesktopShortcutSet(ShortcutRegistrar &registrar,
                                       DesktopShortcutTriggers triggers,
                                       QObject *parent,
                                       const DesktopShortcutRegistrationOptions options)
    : QObject(parent)
{
    const auto count = static_cast<std::size_t>(DesktopShortcutAction::Count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto action = static_cast<DesktopShortcutAction>(index);
        const auto &spec = kShortcutSpecs[index];
        auto *const actionObject = new QAction(this);
        actionObject->setObjectName(QString::fromLatin1(spec.actionId));
        actionObject->setText(QString::fromLatin1(spec.text));
        m_actions[index] = actionObject;
        connect(actionObject, &QAction::triggered, this,
                [callback = triggerFor(triggers, action)] {
                    if (callback) {
                        callback();
                    }
                });
        if ((!options.registerBrightness
             && (action == DesktopShortcutAction::BrightnessUp
                 || action == DesktopShortcutAction::BrightnessDown))
            || (!options.registerScreenshot
                && action == DesktopShortcutAction::TakeScreenshot)) {
            // AGENT-GUARD: PowerDevil owns brightness and Spectacle owns Print
            // in the production session. A second KGlobalAccel action silently
            // loses a user or provider binding and makes ownership ambiguous.
            continue;
        }
        const auto registration = registrar.registerShortcut(
            *actionObject, QKeySequence(spec.key), *this,
            [this, action](bool present) {
                setActiveBindingPresent(action, present);
            });
        m_requestAccepted[index] = registration.requestAccepted;
        m_activeBindingPresent[index] = registration.activeBindingPresent;
    }
}

DesktopShortcutSet::~DesktopShortcutSet() = default;

QKeySequence DesktopShortcutSet::defaultShortcut(DesktopShortcutAction action)
{
    const auto index = static_cast<std::size_t>(action);
    Q_ASSERT(index < static_cast<std::size_t>(DesktopShortcutAction::Count));
    return QKeySequence(kShortcutSpecs[index].key);
}

QString DesktopShortcutSet::stableActionId(DesktopShortcutAction action)
{
    const auto index = static_cast<std::size_t>(action);
    Q_ASSERT(index < static_cast<std::size_t>(DesktopShortcutAction::Count));
    return QString::fromLatin1(kShortcutSpecs[index].actionId);
}

QAction *DesktopShortcutSet::action(DesktopShortcutAction action) const
{
    return m_actions.at(static_cast<std::size_t>(action));
}

bool DesktopShortcutSet::registrationRequestAccepted(
    DesktopShortcutAction action) const
{
    return m_requestAccepted.at(static_cast<std::size_t>(action));
}

bool DesktopShortcutSet::activeBindingPresent(DesktopShortcutAction action) const
{
    return m_activeBindingPresent.at(static_cast<std::size_t>(action));
}

void DesktopShortcutSet::setActiveBindingPresent(DesktopShortcutAction action,
                                                 bool present)
{
    const auto index = static_cast<std::size_t>(action);
    if (m_activeBindingPresent.at(index) == present) {
        return;
    }
    m_activeBindingPresent[index] = present;
    Q_EMIT activeBindingPresentChanged(action, present);
}

} // namespace QindaQt::Session::DesktopControls
