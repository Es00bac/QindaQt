// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <memory>

namespace QindaQt::Bluetooth
{

// Creates the B0 platform adapter: a deterministic in-memory backend that
// reports no adapters and no devices until a future BluezQt backend replaces
// it in the runtime lane (ADR-0026). It never touches BlueZ, rfkill, or host
// Bluetooth state. Test and qualification code populates it through the
// private deterministic_adapter_backend.h interface.
[[nodiscard]] std::unique_ptr<AdapterBackend> makeDeterministicAdapterBackend();

} // namespace QindaQt::Bluetooth
