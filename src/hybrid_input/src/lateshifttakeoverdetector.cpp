// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/lateshifttakeoverdetector.h"

namespace QindaQt::HybridInput {

LateShiftTakeoverDetector::LateShiftTakeoverDetector(Qt::KeyboardModifiers requiredModifiers)
    : m_requiredModifiers(requiredModifiers)
{
}

bool LateShiftTakeoverDetector::observe(const void *activeDrag, Qt::KeyboardModifiers modifiers)
{
    if (activeDrag != m_trackedDrag) {
        m_trackedDrag = activeDrag;
        m_lastModifiers = Qt::KeyboardModifiers();
    }
    if (!activeDrag) {
        return false;
    }

    const bool wasSatisfied = (m_lastModifiers & m_requiredModifiers) == m_requiredModifiers;
    const bool nowSatisfied = (modifiers & m_requiredModifiers) == m_requiredModifiers;
    m_lastModifiers = modifiers;
    return !wasSatisfied && nowSatisfied;
}

} // namespace QindaQt::HybridInput
