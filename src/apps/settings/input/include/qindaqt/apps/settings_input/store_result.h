// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Apps::SettingsInput {

// Outcome of a configuration write against one input authority file.
//
// AGENT-CONTRACT: `Stored` means the configuration file was synced; the
// desktop reload was also requested successfully. `StoredButReloadFailed`
// means the file is durable but the running desktop could not be told to
// apply it (the route must say so instead of claiming success). `Failed`
// means nothing was persisted. Callers present all three differently and
// must never collapse the first two.
enum class StoreResult {
    Stored,
    StoredButReloadFailed,
    Failed,
};

} // namespace QindaQt::Apps::SettingsInput
