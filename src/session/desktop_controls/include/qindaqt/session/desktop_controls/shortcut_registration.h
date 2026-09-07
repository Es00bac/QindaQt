// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QKeySequence>

#include <functional>

class QAction;
class QObject;

namespace QindaQt::Session::DesktopControls {

struct ShortcutRegistration final {
    // The registrar may accept local registration while its D-Bus service is
    // absent. Keep this distinct from the observable active binding.
    bool requestAccepted = false;
    bool activeBindingPresent = false;
};

// Seam over the compositor-provided global shortcut authority. Production uses
// KGlobalAccel Autoloading so a user's remapping or intentional disablement
// survives restart; tests inject a fake.
class ShortcutRegistrar {
public:
    virtual ~ShortcutRegistrar() = default;

    // `action` and `lifetime` are borrowed and outlive callbacks installed by
    // this call. The callback reports user remapping, disablement, or recovery.
    [[nodiscard]] virtual ShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) = 0;
};

} // namespace QindaQt::Session::DesktopControls
