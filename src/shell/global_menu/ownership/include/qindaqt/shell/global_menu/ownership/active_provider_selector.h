// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/menu_provider_registration.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
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
    // The focus generation at which this provider was authenticated.
    quint64 focusGeneration = 0;

    bool operator==(const SelectedProvider &) const = default;
};

// The single authoritative ownership/lineage authority of the global-menu
// module: it mints the epoch/revision lineage that the exporter stamps into
// trees (through shell composition) and the invocation guard checks against.
// `epoch` changes only when the owning window identity changes; `revision`
// advances on every adoption within one epoch. Shell composition must
// re-adopt — advancing the revision — whenever republished content must
// become invocable, so a same-epoch older tree/request is always stale.
//
// AGENT-CONTRACT: this class is deliberately unable to trust separately
// supplied facts — adoption takes only an `AuthenticatedProvider` proof, so
// the verified identity and the adopted identity cannot diverge.
//
// Lifetime/thread contract: callers on one thread; there is no internal
// locking. The instance must outlive every exported tree's lineage checks.
class ActiveProviderSelector final
{
public:
    ActiveProviderSelector() = default;

    void adopt(const AuthenticatedProvider &proof);
    void clear();

    // AGENT-CONTRACT: fail-closed focus invalidation. A generation other than
    // the adopted proof's generation means focus has moved since
    // authentication; the adoption is dropped so no stale provider can stay
    // authoritative. Same generation is a no-op. Shell composition calls
    // this on every observed focus change before any export or invocation.
    void applyFocusGeneration(quint64 generation);

    [[nodiscard]] std::optional<SelectedProvider> current() const;

private:
    std::optional<SelectedProvider> m_current;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
