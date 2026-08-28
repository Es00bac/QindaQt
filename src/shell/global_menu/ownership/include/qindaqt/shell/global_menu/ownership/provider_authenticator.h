// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/menu_provider_registration.h>

#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

struct AuthenticationResult final {
    bool accepted = false;
    // One of: "invalid-registration", "no-active-window",
    // "not-active-window", "credential-unavailable", "pid-mismatch". Empty
    // when accepted.
    QString reasonCode;
};

// Authenticates a provider's claim against two independently sourced facts:
// which window is genuinely active right now, and which OS process really
// owns the registering bus name. A registration is accepted only when both
// agree with the claim and with each other. This is deliberately narrower
// than `com.canonical.AppMenu.Registrar`'s `RegisterWindow`, which trusts the
// caller's claimed window id outright; G0 authenticates only the currently
// active window; a per-window registration cache is a later milestone.
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
