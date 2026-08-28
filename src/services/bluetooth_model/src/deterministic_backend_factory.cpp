// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/deterministic_backend_factory.h>

#include "deterministic_adapter_backend.h"

#include <memory>

namespace QindaQt::Bluetooth
{

std::unique_ptr<AdapterBackend> makeDeterministicAdapterBackend()
{
    return std::make_unique<DeterministicAdapterBackend>();
}

} // namespace QindaQt::Bluetooth
