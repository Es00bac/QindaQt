// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>

namespace QindaQt::DisplayRuntime
{

struct StateRootInputs {
    QString explicitPath;
    QString systemdStateDirectory;
    QString xdgStateHome;
    QString home;
};

enum class StateRootError {
    None,
    Missing,
    AmbiguousSystemdDirectory,
    InvalidPath,
};

struct StateRootSelection {
    QString path;
    StateRootError error = StateRootError::Missing;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept
    {
        return error == StateRootError::None;
    }
};

// Pure process-boundary selection. It performs no I/O and never creates a
// directory. D5 remains the authority for existence, ownership, permissions,
// symlink rejection, and canonical journal truth.
[[nodiscard]] StateRootSelection selectStateRoot(const StateRootInputs &inputs);

// Resolve the exact directory systemd provisioned before handing it to the
// journal boundary. StateDirectory may legitimately retain a compatibility
// symlink across upgrades; only this systemd-selected source is resolved.
// Explicit and XDG-selected roots retain the journal store's no-symlink rule.
[[nodiscard]] StateRootSelection
resolveProvisionedStateRoot(const StateRootSelection &selection);

// Production activation without systemd has no StateDirectory provisioning.
// Prepare the selected implicit per-user directory when absent, or resolve an
// existing compatibility link, before D5 performs its ownership and mode
// checks. Explicit --state-root callers retain the strict no-create contract.
[[nodiscard]] StateRootSelection
prepareImplicitStateRoot(const StateRootSelection &selection);

} // namespace QindaQt::DisplayRuntime
