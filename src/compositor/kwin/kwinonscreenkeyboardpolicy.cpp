// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinonscreenkeyboardpolicy.h"

#include <inputmethod.h>
#include <main.h>

namespace QindaQt::Compositor::KWinIntegration {

KWinOnScreenKeyboardPolicy::KWinOnScreenKeyboardPolicy(QObject *parent) : QObject(parent) {}

void KWinOnScreenKeyboardPolicy::apply(const QString &mode)
{
    m_mode = mode;
    auto *inputMethod = KWin::kwinApp()->inputMethod();
    if (inputMethod == nullptr) {
        return;
    }
    // KWin starts or stops the keyboard process with this switch and
    // remembers it in kwinrc; Settings1 stays the source of truth. Whether
    // a started keyboard shows is KWin's own rule (a finger or pen focused
    // the field): the panel gate is not reachable from a plugin, so there
    // is no "always" mode.
    const bool wanted = mode != QLatin1String("off");
    if (inputMethod->isEnabled() != wanted) {
        inputMethod->setEnabled(wanted);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
