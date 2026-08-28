// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// AGENT-CONTRACT: both fields must already be authenticated by the caller
// (an authenticated compositor-owned inventory, never client-supplied
// metadata) before this value is constructed. This type carries no proof of
// that authentication by itself; ActiveWindowSource implementations are the
// trust boundary.
struct WindowIdentity final {
    QUuid windowId;
    qint64 processId = -1;

    [[nodiscard]] bool isValid() const noexcept { return !windowId.isNull() && processId > 0; }

    bool operator==(const WindowIdentity &) const = default;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
