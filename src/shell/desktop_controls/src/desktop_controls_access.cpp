// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"

namespace QindaQt::Shell::DesktopControls {

DesktopControlsAccess::DesktopControlsAccess(Facades facades, QObject *parent)
    : QObject(parent), m_facades(facades)
{
}

} // namespace QindaQt::Shell::DesktopControls
