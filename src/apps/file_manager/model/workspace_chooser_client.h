// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Apps::FileManager {

// One-shot client for the compositor's workspace picker route (ADR-0165).
// The picker never names its own window: the compositor resolves the choice
// against the ACTIVE window, which is the picker itself while the user is
// clicking in it.
struct ChooserReply final
{
    bool ok = false;
    QString message;

    [[nodiscard]] bool accepted() const noexcept { return ok; }
};

[[nodiscard]] ChooserReply chooseApplicationOnCompositor(
    const QString &desktopEntryId);

} // namespace QindaQt::Apps::FileManager
