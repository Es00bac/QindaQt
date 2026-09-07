// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/session/desktop_controls/shortcut_registration.h"

namespace QindaQt::Session::DesktopControls {

// Production registrar over KGlobalAccel, whose D-Bus service is provided by
// the session compositor. Autoloading preserves user remapping and
// intentional disablement.
class KGlobalAccelRegistrar final : public ShortcutRegistrar {
public:
    KGlobalAccelRegistrar() = default;
    ~KGlobalAccelRegistrar() override = default;

    KGlobalAccelRegistrar(const KGlobalAccelRegistrar &) = delete;
    KGlobalAccelRegistrar &operator=(const KGlobalAccelRegistrar &) = delete;

    [[nodiscard]] ShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) override;
};

} // namespace QindaQt::Session::DesktopControls
