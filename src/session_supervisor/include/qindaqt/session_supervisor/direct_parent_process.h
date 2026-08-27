// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

#include <optional>

class QString;

namespace QindaQt::SessionSupervisor {

// Arms a kernel parent-death signal, closes the setup race, and returns the
// direct parent of qindaqt-session. Production uses that witnessed value as
// the expected compositor PID instead of trusting caller-controlled state.
// Calling this function changes the caller's Linux parent-death signal.
[[nodiscard]] std::optional<qint64> establishDirectParentProcessWitness(
    QString *error = nullptr);

// PID 1 cannot be the per-user compositor parent. Keeping that rejection in a
// pure helper lets startup tests cover the fail-closed boundary without
// launching KWin.
[[nodiscard]] bool isUsableCompositorProcessId(qint64 processId) noexcept;

} // namespace QindaQt::SessionSupervisor
