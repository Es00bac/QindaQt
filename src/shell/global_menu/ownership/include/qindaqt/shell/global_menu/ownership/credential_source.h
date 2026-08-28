// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// Seam for the bus daemon's own peer-credential lookup (the D-Bus
// equivalent of `GetConnectionUnixProcessID`). A later transport milestone
// supplies the real QDBusConnectionInterface-backed implementation; G0 tests
// exercise only fakes.
class CredentialSource
{
public:
    virtual ~CredentialSource() = default;

    // AGENT-CONTRACT: implementations must query the bus daemon itself, never
    // trust a caller-supplied process id. Returns std::nullopt when the name
    // has no current owner or the daemon cannot be asked right now; a
    // provider authenticator must treat that as rejection, not as "trust the
    // claim."
    [[nodiscard]] virtual std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
