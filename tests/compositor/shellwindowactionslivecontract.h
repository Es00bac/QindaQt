// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Compositor::TestSupport {

// Compares the running private-KWin object with the immutable source
// descriptor. This is intentionally dynamic evidence, unlike the descriptor's
// standalone syntax test.
[[nodiscard]] bool liveShellInterfaceMatchesDescriptor(QString *failure);

} // namespace QindaQt::Compositor::TestSupport
