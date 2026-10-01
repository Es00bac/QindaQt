// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/compositor_capture/kwin_capture_port.h>
// Compatibility for existing Screenshot consumers; shared capture belongs to its public module.
namespace QindaQt::Screenshot { using namespace QindaQt::CompositorCapture; }
