// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// AGENT-NOTE: every pure painting helper moved to the shared decoration
// painter (ADR-0127) so the Settings preview and the live decoration draw
// through one implementation. This header re-exports it and keeps the one
// KDecoration-bound helper, the nine-patch shadow.
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <memory>

namespace KDecoration3 {
class DecorationShadow;
}

namespace QindaQt::Decoration {

[[nodiscard]] std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style);

} // namespace QindaQt::Decoration
