// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
namespace QindaQt::Services::AndroidRuntime {
// Caller closes/transfers both CLOEXEC/nonblocking endpoints. SO_PASSCRED is
// enabled before either is handed to a child; no filesystem/public socket name.
[[nodiscard]] std::array<int,2> createPrivateProducerDescriptors();
// Caller already observed this PID from its supervised launch, never a message.
// Returns an owned CLOEXEC pidfd or -1. Keep it from birth through admission.
[[nodiscard]] int holdProducerLifetime(int observedPid);
} // namespace QindaQt::Services::AndroidRuntime
