// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/compositor_capture/raw_capture_decoder.h>
// Compatibility for existing Screenshot consumers; shared capture belongs to its public module.
namespace QindaQt::Screenshot { using namespace QindaQt::CompositorCapture; }
