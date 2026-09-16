// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

namespace QindaQt::Audio::Admission
{

// Lookups the admission switch shares with the coordinator's own code.
[[nodiscard]] const Device *findDevice(const Snapshot &snapshot, const Handle &handle);
[[nodiscard]] const Stream *findStream(const Snapshot &snapshot, const Handle &handle);
[[nodiscard]] bool hasCapability(Capabilities capabilities, Capability capability);
[[nodiscard]] bool hasConsoleCapability(Capabilities capabilities);

} // namespace QindaQt::Audio::Admission
