// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// One authenticated active-window observation. `focusGeneration` is a
// monotonic counter that changes every time compositor focus moves; two
// observations with the same generation describe the same unbroken focus
// interval. AGENT-CONTRACT: implementations must derive both fields from an
// authenticated compositor-owned inventory (a later milestone supplies the
// resident adapter, analogous to SessionLockState's owner/PID-authenticated
// observation); client-supplied metadata is never acceptable. Generation
// values are only comparable within one process and one source instance.
struct ActiveWindowObservation final {
    WindowIdentity window;
    quint64 focusGeneration = 0;

    bool operator==(const ActiveWindowObservation &) const = default;
};

// Seam for an authenticated compositor-owned active-window observer. G0
// tests exercise only fakes; a later milestone supplies the real adapter.
class ActiveWindowSource
{
public:
    virtual ~ActiveWindowSource() = default;

    // AGENT-CONTRACT: implementations must return std::nullopt when no window
    // is active or the source is not yet authenticated; callers must treat
    // that as "no provider can be authenticated right now." Two calls that
    // straddle a focus change must return different generations so a
    // ProviderAuthenticator can detect the race instead of trusting the
    // first sample.
    [[nodiscard]] virtual std::optional<ActiveWindowObservation> activeWindow() const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
