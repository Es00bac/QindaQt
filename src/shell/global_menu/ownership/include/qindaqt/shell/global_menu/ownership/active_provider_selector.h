// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/menu_provider_registration.h>
#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

struct SelectedProvider final {
    WindowIdentity window;
    QString providerUniqueName;
    QUuid epoch;
    quint64 revision = 0;

    bool operator==(const SelectedProvider &) const = default;
};

// Tracks which authenticated registration is currently authoritative,
// mirroring the owner/epoch/revision lineage used by Display1, Audio1, and
// NotificationPresentation1: `epoch` changes only when the owning window
// identity changes; `revision` advances on every adoption within one epoch.
// This class trusts its caller completely — only feed it registrations that
// ProviderAuthenticator has already accepted.
class ActiveProviderSelector final
{
public:
    ActiveProviderSelector() = default;

    void adoptAuthenticated(const MenuProviderRegistration &registration,
                             const WindowIdentity &window);
    void clear();

    [[nodiscard]] std::optional<SelectedProvider> current() const;

private:
    std::optional<SelectedProvider> m_current;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
