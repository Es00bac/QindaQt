// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_registry.h>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>

namespace QindaQt::StatusNotifier
{

struct PresentationInput {
    // True while the StatusNotifierWatcher is reachable through the injected
    // transport. A watcher loss degrades presentation to Degraded while the
    // registered last-known-good items stay visible and actionable; the
    // accepted contract is in ADR-0032.
    bool transportLive = false;

    friend bool operator==(const PresentationInput &, const PresentationInput &) = default;
};

// AGENT-CONTRACT: Localization boundary. All human-readable strings emitted
// by the projection come from this struct; the shell presenter supplies
// locale-appropriate values, and the defaults exist only as a deterministic
// fallback for tests and early integration. The fallback accessible name is
// the item identity, which is locale-independent by definition. Empty
// `keyboardSecondaryActivate` is the truthful "pointer-only" state and must
// stay empty until a designed keyboard route exists.
struct PresentationTexts {
    QString statusPassive = QStringLiteral("passive");
    QString statusActive = QStringLiteral("active");
    QString statusNeedsAttention = QStringLiteral("needs attention");
    QString statusUnknown = QStringLiteral("unknown");
    QString keyboardActivate = QStringLiteral("Enter or Space");
    QString keyboardContextMenu = QStringLiteral("Shift+F10 or Menu key");
    QString keyboardSecondaryActivate;

    friend bool operator==(const PresentationTexts &, const PresentationTexts &) = default;
};

// AGENT-CONTRACT: Pure projection from registry state to tray presentation.
// Same registry state, input, and texts always yield an equal
// TrayPresentation with a stable item order, so QML and assistive technology
// observe no phantom reorder. No I/O, clock, or transport access happens
// here. States:
// - Loading: watcher live but the current watcher epoch's item population has
//   not been observed yet (initial start and every reconnect rebaseline).
// - Ready: watcher live, population observed, at least one item.
// - Empty: watcher live, population observed, no items.
// - Degraded: watcher unavailable (last-known-good items stay visible and
//   actionable) or the registry is degraded (diagnostic names the cause).
[[nodiscard]] TrayPresentation projectPresentation(const StatusNotifierRegistry &registry,
                                                   const PresentationInput &input,
                                                   const PresentationTexts &texts = {});

} // namespace QindaQt::StatusNotifier
