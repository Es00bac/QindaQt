// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/android_manager/setup_port.h"
#include <memory>
namespace QindaQt::AndroidManager {
// Own the result before the controller, so it outlives the borrowed GUI port.
// Fixed installed QindaPortage path only; no environment or user path override.
[[nodiscard]] std::unique_ptr<SetupPort> createNativeSetupPort();
}
