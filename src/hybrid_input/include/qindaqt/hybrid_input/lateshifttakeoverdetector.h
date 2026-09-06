// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>

namespace QindaQt::HybridInput {

// Toolkit-neutral detection of "the exact takeover chord became newly
// satisfied while some external drag was already in progress" - the one
// event a caller should react to by pre-empting that external drag. The
// caller supplies an opaque identity for whichever external drag is active
// right now (or nullptr when none is) plus the modifiers observed on this
// event; this class only ever compares identity/equality and modifier
// bitmasks, so it carries no window-toolkit dependency and is fully
// unit-testable without a live window of any kind.
class LateShiftTakeoverDetector final
{
public:
    explicit LateShiftTakeoverDetector(Qt::KeyboardModifiers requiredModifiers);

    // Call once per raw input event. `activeDrag` is nullptr when no
    // external drag is in progress right now (including the instant one
    // just ended); any other value identifies which drag is active. A
    // changed value between calls (including from/to nullptr) is treated as
    // a brand-new drag whose observed modifiers start unsatisfied, matching
    // a native move's own modifiers resetting to empty at its start.
    //
    // Returns true on exactly the one call where `requiredModifiers` is
    // newly fully satisfied for the *same* still-active drag that did not
    // satisfy it a moment ago.
    [[nodiscard]] bool observe(const void *activeDrag, Qt::KeyboardModifiers modifiers);

private:
    Qt::KeyboardModifiers m_requiredModifiers;
    const void *m_trackedDrag = nullptr;
    Qt::KeyboardModifiers m_lastModifiers = Qt::KeyboardModifiers();
};

} // namespace QindaQt::HybridInput
