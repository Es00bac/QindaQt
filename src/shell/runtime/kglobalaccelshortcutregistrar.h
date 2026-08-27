// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "globalshortcutregistrar.h"

namespace QindaQt::Shell {

class KGlobalAccelShortcutRegistrar final : public GlobalShortcutRegistrar {
public:
    [[nodiscard]] GlobalShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) override;
};

} // namespace QindaQt::Shell
