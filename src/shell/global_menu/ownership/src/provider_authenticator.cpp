// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

#include <utility>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

namespace
{

// AGENT-CONTRACT: the registration's provider identity must be a D-Bus
// *unique* name (":1.42" shape) — never a well-known name. Unique names are
// assigned by the bus daemon for the lifetime of one connection, so the
// credential lookup below proves the registering process owns the identity;
// a well-known name can be re-owned later, which would silently change who
// the issued proof names. Grammar and the 255-byte maximum follow the D-Bus
// specification's bus-name rules: elements of [A-Za-z0-9_-] separated by
// periods.
bool isValidUniqueName(const QString &name)
{
    if (name.isEmpty() || !name.startsWith(u':')) {
        return false;
    }
    if (name.toUtf8().size() > Protocol::kMaxProviderUniqueNameUtf8Bytes) {
        return false;
    }
    const QString body = name.sliced(1);
    int elements = 0;
    qsizetype elementStart = 0;
    for (qsizetype i = 0; i <= body.size(); ++i) {
        if (i == body.size() || body.at(i) == u'.') {
            const qsizetype elementLength = i - elementStart;
            if (elementLength == 0) {
                // Empty element: ":1..42", ":.42", or a trailing dot.
                return false;
            }
            ++elements;
            elementStart = i + 1;
            continue;
        }
        const QChar c = body.at(i);
        const bool allowed = (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z')
            || (c >= u'A' && c <= u'Z') || c == u'_' || c == u'-';
        if (!allowed) {
            return false;
        }
    }
    // D-Bus unique names carry at least two elements (":1.42"), which also
    // excludes a bare ":" prefix with no identity at all.
    return elements >= 2;
}

bool isValidRegistration(const MenuProviderRegistration &registration)
{
    if (registration.windowId.isNull()) {
        return false;
    }
    if (registration.claimedProcessId <= 0) {
        return false;
    }
    return isValidUniqueName(registration.providerUniqueName);
}

} // namespace

AuthenticatedProvider::AuthenticatedProvider(WindowIdentity window, QString providerUniqueName,
                                             quint64 focusGeneration)
    : m_window(window)
    , m_providerUniqueName(std::move(providerUniqueName))
    , m_focusGeneration(focusGeneration)
{
}

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

    const std::optional<ActiveWindowObservation> before = m_activeWindowSource.activeWindow();
    if (!before || !before->window.isValid()) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("no-active-window")};
    }
    if (before->window.windowId != registration.windowId) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("not-active-window")};
    }

    const std::optional<qint64> authenticatedPid =
        m_credentialSource.processIdForUniqueName(registration.providerUniqueName);
    if (!authenticatedPid) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("credential-unavailable")};
    }
    if (*authenticatedPid != registration.claimedProcessId
        || *authenticatedPid != before->window.processId) {
        return AuthenticationResult{.accepted = false, .reasonCode = QStringLiteral("pid-mismatch")};
    }

    // AGENT-GUARD: focus must be re-read after the (potentially slow)
    // credential lookup and agree exactly with the first observation. Skipping
    // this recheck lets a registration that was valid only before a focus
    // change return accepted — the focus TOCTOU this seam exists to close.
    const std::optional<ActiveWindowObservation> after = m_activeWindowSource.activeWindow();
    if (!after || after->window != before->window
        || after->focusGeneration != before->focusGeneration) {
        return AuthenticationResult{.accepted = false,
                                     .reasonCode = QStringLiteral("focus-changed")};
    }

    return AuthenticationResult{.accepted = true,
                                 .reasonCode = QString(),
                                 .proof = AuthenticatedProvider{
                                     before->window, registration.providerUniqueName,
                                     before->focusGeneration}};
}

} // namespace QindaQt::Shell::GlobalMenu::Ownership
