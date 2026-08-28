// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// A provider's unauthenticated claim: "I export the menu for this window, my
// bus unique name is this, and my OS process id is that." Nothing here is
// trusted until ProviderAuthenticator cross-checks it against an
// independently authenticated source.
struct MenuProviderRegistration final {
    QUuid windowId;
    QString providerUniqueName;
    qint64 claimedProcessId = -1;

    bool operator==(const MenuProviderRegistration &) const = default;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
