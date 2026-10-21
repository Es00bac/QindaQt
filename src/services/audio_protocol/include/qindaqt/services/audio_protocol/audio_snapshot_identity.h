// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

namespace QindaQt::Audio
{

// AGENT-CONTRACT: equal epoch and revision identify one snapshot publication,
// except for meter readings. The service folds every Levels batch into its
// retained snapshot (Strip::level, Bus::level) without advancing the revision
// (ADR-0174), and AudioClient applies and clears the same fields from the
// Levels stream itself. Two same-revision snapshots are therefore the same
// publication when everything except those readings is equal; any other
// difference is a lineage contradiction and fails closed (Audio1 reference,
// "Lineage and completion").
[[nodiscard]] bool sameRevisionContent(const Snapshot &left, const Snapshot &right);

} // namespace QindaQt::Audio
