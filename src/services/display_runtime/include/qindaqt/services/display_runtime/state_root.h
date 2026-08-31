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

} // namespace QindaQt::DisplayRuntime
