// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

namespace
{

bool isValidRegistration(const MenuProviderRegistration &registration)
{
    if (registration.windowId.isNull()) {
        return false;
    }
    if (registration.claimedProcessId <= 0) {
        return false;
    }
    const QString &name = registration.providerUniqueName;
    return !name.isEmpty() && name.toUtf8().size() <= Protocol::kMaxProviderUniqueNameUtf8Bytes;
}

} // namespace

ProviderAuthenticator::ProviderAuthenticator(const ActiveWindowSource &activeWindowSource,
                                              const CredentialSource &credentialSource)
    : m_activeWindowSource(activeWindowSource)
    , m_credentialSource(credentialSource)
{
}

AuthenticationResult ProviderAuthenticator::authenticate(
    const MenuProviderRegistration &registration) const
{
    if (!isValidRegistration(registration)) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("invalid-registration")};
    }

    const std::optional<WindowIdentity> active = m_activeWindowSource.activeWindow();
    if (!active || !active->isValid()) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("no-active-window")};
    }
    if (active->windowId != registration.windowId) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("not-active-window")};
    }

    const std::optional<qint64> authenticatedPid =
        m_credentialSource.processIdForUniqueName(registration.providerUniqueName);
    if (!authenticatedPid) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("credential-unavailable")};
    }
    if (*authenticatedPid != registration.claimedProcessId || *authenticatedPid != active->processId) {
        return AuthenticationResult{.accepted = false, .reasonCode = QStringLiteral("pid-mismatch")};
    }

    return AuthenticationResult{.accepted = true, .reasonCode = QString()};
}

} // namespace QindaQt::Shell::GlobalMenu::Ownership
