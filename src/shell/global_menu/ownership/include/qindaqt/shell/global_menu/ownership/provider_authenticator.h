// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/menu_provider_registration.h>
#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// The verified facts an accepted authentication established. AGENT-CONTRACT:
// this proof — never the raw registration or any separately supplied window —
// is the only value ActiveProviderSelector accepts for adoption, so the facts
// that were checked are provably the facts that become authoritative.
struct AuthenticatedProvider final {
    WindowIdentity window;
    QString providerUniqueName;
    // The focus generation both successful observations agreed on. Adopting
    // and later invalidation are keyed on this value.
    quint64 focusGeneration = 0;

    bool operator==(const AuthenticatedProvider &) const = default;
};

struct AuthenticationResult final {
    bool accepted = false;
    // One of: "invalid-registration", "no-active-window", "not-active-window",
    // "credential-unavailable", "pid-mismatch", "focus-changed". Empty when
    // accepted.
    QString reasonCode;
    // Valid only when accepted.
    AuthenticatedProvider proof;
};

// Authenticates a provider's claim against two independently sourced facts:
// which window is genuinely active right now, and which OS process really
// owns the registering bus name. Focus is sampled before and after the
// credential lookup; both samples must agree on window and focus generation
// or the authentication fails, closing the sample-lookup race where focus
// moves mid-check. A registration is accepted only when everything agrees.
// This is deliberately narrower than `com.canonical.AppMenu.Registrar`'s
// `RegisterWindow`, which trusts the caller's claimed window id outright; G0
// authenticates only the currently active window; a per-window registration
// cache is a later milestone.
//
// Lifetime/thread contract: the two seam references must outlive this
// object, and authenticate() must be called on the same thread that owns
// both seam implementations; they are invoked synchronously and in order.
class ProviderAuthenticator final
{
public:
    ProviderAuthenticator(const ActiveWindowSource &activeWindowSource,
                          const CredentialSource &credentialSource);

    [[nodiscard]] AuthenticationResult authenticate(
        const MenuProviderRegistration &registration) const;

private:
    const ActiveWindowSource &m_activeWindowSource;
    const CredentialSource &m_credentialSource;
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
