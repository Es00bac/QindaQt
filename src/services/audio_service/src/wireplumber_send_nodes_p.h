// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "console_send_admission_p.h"
#include <wp/wp.h>

namespace QindaQt::Audio
{
// Uses actual own-client proxy ID and daemon GLOBAL serial, never application
// PID or mutable node-provided object.serial/client.id fallbacks.
[[nodiscard]] SendProducerIdentity sendProducerIdentity(
    WpCore *core, WpObjectManager *manager, quint64 generation);
// Bounded complete marked-node observation; overflow returns empty, never a
// prefix that can miss a duplicate. Existing pins then become invalid and must
// be retired before ordinary projection or any further writes.
[[nodiscard]] QList<ObservedSendNode> observedSendNodes(WpObjectManager *manager);
} // namespace QindaQt::Audio
