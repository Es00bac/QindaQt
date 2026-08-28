// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <optional>

namespace QindaQt::ShellCustomizationEditor {

// Pure keyboard-equivalent navigation over the outline. Pointer drags and
// keyboard moves feed the same translator and the same gesture state machine
// (architecture D7), so these helpers only resolve the NEXT drop target for a
// step. nullopt means "the step stays where it is" (edge of the panel, zone,
// or list); callers must not synthesize a target from it.

// Step the anchor one slot forward/backward inside the target panel's flat
// applet list. The zone is preserved.
[[nodiscard]] std::optional<DropTarget> nextSlotInPanel(const Profiles::LayoutProfile &profile,
                                                        const DropTarget &current);
[[nodiscard]] std::optional<DropTarget> previousSlotInPanel(const Profiles::LayoutProfile &profile,
                                                            const DropTarget &current);

// Step through start → center → end within the same panel, keeping the anchor.
[[nodiscard]] std::optional<DropTarget> nextZoneInPanel(const Profiles::LayoutProfile &profile,
                                                        const DropTarget &current);
[[nodiscard]] std::optional<DropTarget> previousZoneInPanel(const Profiles::LayoutProfile &profile,
                                                            const DropTarget &current);

// Step to the next/previous panel in profile order; the target appends at the
// panel end. Identical outputs for horizontal and vertical panels keep the
// keyboard model independent of panel orientation.
[[nodiscard]] std::optional<DropTarget> nextPanelTarget(const Profiles::LayoutProfile &profile,
                                                        const DropTarget &current);
[[nodiscard]] std::optional<DropTarget> previousPanelTarget(const Profiles::LayoutProfile &profile,
                                                            const DropTarget &current);

// Rotate a panel's edge through Top → Right → Bottom → Left (stepDelta +1/-1)
// keeping output, alignment, and restack position. Wrap-around is intentional:
// the four edges form a cycle.
[[nodiscard]] MovePanelIntent steppedPanelEdge(const Profiles::LayoutProfile &profile,
                                               const QString &panelId,
                                               int stepDelta);

} // namespace QindaQt::ShellCustomizationEditor
