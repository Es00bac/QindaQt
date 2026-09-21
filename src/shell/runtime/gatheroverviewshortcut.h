// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QObject>

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

// The global shortcut that raises and dismisses the gather overview.
//
// AGENT-CONTRACT (ADR-0232): the overview also replaces KWin's upper-left
// screen corner.
// SessionDefaults seeds an empty Effect-overview BorderActivate so brushing
// that corner no longer raises KWin's own window grid; this action is the
// keyboard half of the same gesture, and the panel applet is the third. All
// three reach one controller, so the three can never disagree about whether
// the overview is up.
//
// Meta+Shift+G, for gather. NOT Meta+G: KWin's own Grid View effect holds
// that, and ADR-0232's position is that the effect keeps its keyboard
// shortcuts - only the screen corner is un-reserved. KGlobalAccel does not
// refuse a conflicting default, it registers the action with an EMPTY active
// binding, so the first attempt shipped a key that silently did nothing and
// looked registered (`qindaqt_toggle_gather_overview=,Meta+G,...` in
// kglobalshortcutsrc: no active key, only a default).
//
// Alt+F1 (launcher), Meta+N (notifications), Meta+Space (command palette),
// Meta+Shift+E (live customization) and Meta+Shift+F1 (panel visibility) are
// the shell's other claims. Before adding a seventh, grep the host's
// ~/.config/kglobalshortcutsrc as well as this list - most of the Meta+letter
// space belongs to KWin and Plasma components, not to us.
class GatherOverviewShortcutProducer final : public QObject {
    Q_OBJECT

public:
    GatherOverviewShortcutProducer(GlobalShortcutRegistrar &registrar,
                                   QObject *parent = nullptr);
    ~GatherOverviewShortcutProducer() override;

    GatherOverviewShortcutProducer(const GatherOverviewShortcutProducer &) = delete;
    GatherOverviewShortcutProducer &
    operator=(const GatherOverviewShortcutProducer &) = delete;

    [[nodiscard]] static QString stableActionId();
    [[nodiscard]] static QKeySequence defaultShortcut();

    [[nodiscard]] bool registrationRequestAccepted() const noexcept;
    [[nodiscard]] bool activeBindingPresent() const noexcept;

Q_SIGNALS:
    void toggleRequested();
    void activeBindingPresentChanged();

private:
    class Private;
    Private *m_private;
};

} // namespace QindaQt::Shell
