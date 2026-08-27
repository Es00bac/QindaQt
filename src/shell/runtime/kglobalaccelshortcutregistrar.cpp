// SPDX-License-Identifier: GPL-3.0-or-later
#include "kglobalaccelshortcutregistrar.h"

#include <KGlobalAccel>

#include <QAction>

#include <utility>

namespace QindaQt::Shell {

GlobalShortcutRegistration KGlobalAccelShortcutRegistrar::registerShortcut(
    QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
    std::function<void(bool)> activeBindingChanged)
{
    auto *const globalAccel = KGlobalAccel::self();
    QObject::connect(
        globalAccel, &KGlobalAccel::globalShortcutChanged, &lifetime,
        [&action, callback = std::move(activeBindingChanged)](
            QAction *changedAction, const QKeySequence &sequence) {
            if (changedAction == &action && callback) {
                callback(!sequence.isEmpty());
            }
        });

    const QList<QKeySequence> shortcuts{defaultShortcut};
    const bool defaultAccepted = globalAccel->setDefaultShortcut(
        &action, shortcuts, KGlobalAccel::Autoloading);
    // AGENT-CONTRACT: Autoloading preserves user remapping and intentional
    // disablement. Never switch this startup path to NoAutoloading or steal a
    // conflicting binding.
    const bool activeAccepted = globalAccel->setShortcut(
        &action, shortcuts, KGlobalAccel::Autoloading);
    return {.requestAccepted = defaultAccepted && activeAccepted,
            .activeBindingPresent =
                !globalAccel->shortcut(&action).isEmpty()};
}

} // namespace QindaQt::Shell
