// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>

#include <functional>

class QAction;
class QObject;

namespace QindaQt::Shell {

struct GlobalShortcutRegistration final {
    // KGlobalAccel may accept local registration while its D-Bus service is
    // absent. Keep this distinct from the observable active binding.
    bool requestAccepted = false;
    bool activeBindingPresent = false;
};

class GlobalShortcutRegistrar {
public:
    virtual ~GlobalShortcutRegistrar() = default;

    // `action` and `lifetime` are borrowed and outlive callbacks installed by
    // this call. The callback reports user remapping, disablement, or recovery.
    [[nodiscard]] virtual GlobalShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) = 0;
};

} // namespace QindaQt::Shell
