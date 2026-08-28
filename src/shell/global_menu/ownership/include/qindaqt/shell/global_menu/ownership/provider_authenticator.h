// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/menu_provider_registration.h>
#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

// The verified facts an accepted authentication established, issued as an
// opaque capability. AGENT-CONTRACT: construction is reserved to
// ProviderAuthenticator — the type is deliberately not an aggregate and has
// no public or default constructor, so no caller can mint "accepted
// ownership" without the authenticator actually checking it. Defaulted
// copy/move let honest holders pass the issued proof to the selector; they
// cannot create a new one.
class AuthenticatedProvider final
{
public:
    AuthenticatedProvider(const AuthenticatedProvider &) = default;
    AuthenticatedProvider &operator=(const AuthenticatedProvider &) = default;
    AuthenticatedProvider(AuthenticatedProvider &&) noexcept = default;
    AuthenticatedProvider &operator=(AuthenticatedProvider &&) noexcept = default;
    ~AuthenticatedProvider() = default;

    [[nodiscard]] const WindowIdentity &window() const noexcept { return m_window; }
    [[nodiscard]] const QString &providerUniqueName() const noexcept { return m_providerUniqueName; }
    // The focus generation both successful observations agreed on. Adoption
    // and later invalidation are keyed on this value.
    [[nodiscard]] quint64 focusGeneration() const noexcept { return m_focusGeneration; }

    [[nodiscard]] bool operator==(const AuthenticatedProvider &) const = default;

private:
    friend class ProviderAuthenticator;

    AuthenticatedProvider(WindowIdentity window, QString providerUniqueName,
                          quint64 focusGeneration);

    WindowIdentity m_window;
    QString m_providerUniqueName;
    quint64 m_focusGeneration = 0;
};

struct AuthenticationResult final {
    bool accepted = false;
    // One of: "invalid-registration", "no-active-window", "not-active-window",
    // "credential-unavailable", "pid-mismatch", "focus-changed". Empty when
    // accepted.
    QString reasonCode{};
    // AGENT-CONTRACT: carries an authenticator-issued proof exactly when
    // accepted is true; always empty otherwise. There is no way to obtain or
    // construct a proof outside an accepted authenticate() call.
    std::optional<AuthenticatedProvider> proof{};
};

// Authenticates a provider's claim against two independently sourced facts:
// which window is genuinely active right now, and which OS process really
// owns the registering bus name. Focus is sampled before and after the
// credential lookup; both samples must agree on window and focus generation
// or the authentication fails, closing the sample-lookup race where focus
// moves mid-check. A registration is accepted only when everything agrees
// and the peer's unique name is a syntactically valid D-Bus unique name.
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
