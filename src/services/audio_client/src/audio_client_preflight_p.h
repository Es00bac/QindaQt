// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QString>

namespace QindaQt::Audio
{

// Local admission check for a public operation against the published
// snapshot. Returns an empty string when the request may be dispatched, or a
// stable rejection reason code otherwise. Shared only inside the client
// implementation; the service performs its own independent admission.
[[nodiscard]] QString preflightOperation(const Snapshot &snapshot,
                                         const OperationRequest &request);

} // namespace QindaQt::Audio
