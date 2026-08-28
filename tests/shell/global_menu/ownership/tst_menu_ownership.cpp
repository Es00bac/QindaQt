// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QHash>
#include <QtTest>

#include <type_traits>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Ownership;

namespace {

class FakeActiveWindowSource final : public ActiveWindowSource {
public:
    // The observation every activeWindow() call returns until advanced.
    std::optional<ActiveWindowObservation> value;

    [[nodiscard]] std::optional<ActiveWindowObservation> activeWindow() const override
    {
        return value;
    }
};

class FakeCredentialSource final : public CredentialSource {
public:
    QHash<QString, qint64> knownPids;

    [[nodiscard]] std::optional<qint64> processIdForUniqueName(const QString &uniqueName) const override
    {
        const auto it = knownPids.constFind(uniqueName);
        if (it == knownPids.constEnd()) {
            return std::nullopt;
        }
        return *it;
    }
};

MenuProviderRegistration validRegistration(const QUuid &windowId)
{
    return MenuProviderRegistration{
        .windowId = windowId, .providerUniqueName = QStringLiteral(":1.42"), .claimedProcessId = 4242};
}

ActiveWindowObservation observation(const QUuid &windowId, qint64 pid, quint64 focusGeneration)
{
    return ActiveWindowObservation{.window = WindowIdentity{.windowId = windowId, .processId = pid},
                                    .focusGeneration = focusGeneration};
}


} // namespace

class MenuOwnershipTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void proofTypeIsNotForgeable();
    void authenticatorAcceptsMatchingWindowAndPid();
    void authenticatorRejectsWhenNoActiveWindow();
    void authenticatorRejectsWhenNotActiveWindow();
    void authenticatorRejectsWhenCredentialUnavailable();
    void authenticatorRejectsPidMismatchWithClaim();
    void authenticatorRejectsPidMismatchWithActiveWindow();
    void authenticatorRejectsInvalidRegistration();
    void authenticatorRejectsWellKnownProviderName();
    void authenticatorRejectsMalformedUniqueName();
    void authenticatorAcceptsHyphenatedUniqueName();
    void authenticatorEnforcesUniqueNameByteBoundary();
    void authenticatorRejectsFocusChangeDuringLookup();
    void authenticatorProofCarriesVerifiedFacts();


};

void MenuOwnershipTests::proofTypeIsNotForgeable()
{
    // Structural proof of the issuance restriction: the type cannot be
    // default-constructed or aggregate-initialized, so a caller cannot mint
    // an "accepted" value — only ProviderAuthenticator::authenticate() can.
    static_assert(!std::is_default_constructible_v<AuthenticatedProvider>);
    static_assert(!std::is_aggregate_v<AuthenticatedProvider>);
    // Honest holders may pass an issued proof along by value or reference.
    static_assert(std::is_copy_constructible_v<AuthenticatedProvider>);
    static_assert(std::is_move_constructible_v<AuthenticatedProvider>);
    QVERIFY(true);
}

void MenuOwnershipTests::authenticatorAcceptsMatchingWindowAndPid()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(result.accepted);
    QCOMPARE(result.proof->window().windowId, windowId);
    QCOMPARE(result.proof->focusGeneration(), quint64(11));
}

void MenuOwnershipTests::authenticatorRejectsWhenNoActiveWindow()
{
    FakeActiveWindowSource windowSource;
    FakeCredentialSource credentialSource;
    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(QUuid::createUuid()));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("no-active-window"));
    QVERIFY(!result.proof.has_value());
}

void MenuOwnershipTests::authenticatorRejectsWhenNotActiveWindow()
{
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(QUuid::createUuid(), 4242, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    // Registration names a different window than the authenticated active one.
    const AuthenticationResult result = authenticator.authenticate(validRegistration(QUuid::createUuid()));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("not-active-window"));
    QVERIFY(!result.proof.has_value());
}

void MenuOwnershipTests::authenticatorRejectsWhenCredentialUnavailable()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource; // no known pids

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("credential-unavailable"));
}

void MenuOwnershipTests::authenticatorRejectsPidMismatchWithClaim()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    // The daemon says a different process actually owns this bus name than
    // what the registration claims: a spoofing attempt.
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 9999);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("pid-mismatch"));
}

void MenuOwnershipTests::authenticatorRejectsPidMismatchWithActiveWindow()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    // The active window is genuinely owned by a different process than the
    // registering peer, even though the peer's own claimed pid is honest.
    windowSource.value = observation(windowId, 5555, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("pid-mismatch"));
}

void MenuOwnershipTests::authenticatorRejectsInvalidRegistration()
{
    FakeActiveWindowSource windowSource;
    FakeCredentialSource credentialSource;
    ProviderAuthenticator authenticator(windowSource, credentialSource);

    MenuProviderRegistration registration = validRegistration(QUuid::createUuid());
    registration.providerUniqueName.clear();
    const AuthenticationResult result = authenticator.authenticate(registration);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-registration"));
}

void MenuOwnershipTests::authenticatorRejectsWellKnownProviderName()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    // The credential seam would even resolve a well-known name, but the
    // ownership boundary must refuse it: a well-known name can be re-owned
    // later, which would silently change who the proof names.
    credentialSource.knownPids.insert(QStringLiteral("com.example.Menu"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = QStringLiteral("com.example.Menu"),
                                  .claimedProcessId = 4242});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-registration"));
    QVERIFY(!result.proof.has_value());
}

void MenuOwnershipTests::authenticatorRejectsMalformedUniqueName()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1..42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult emptyElement = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = QStringLiteral(":1..42"),
                                  .claimedProcessId = 4242});
    QVERIFY(!emptyElement.accepted);
    QCOMPARE(emptyElement.reasonCode, QStringLiteral("invalid-registration"));

    const AuthenticationResult singleElement = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = QStringLiteral(":onlyelement"),
                                  .claimedProcessId = 4242});
    QVERIFY(!singleElement.accepted);
    QCOMPARE(singleElement.reasonCode, QStringLiteral("invalid-registration"));

    const AuthenticationResult illegalCharacter = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = QStringLiteral(":1.4 2"),
                                  .claimedProcessId = 4242});
    QVERIFY(!illegalCharacter.accepted);
    QCOMPARE(illegalCharacter.reasonCode, QStringLiteral("invalid-registration"));
}

void MenuOwnershipTests::authenticatorAcceptsHyphenatedUniqueName()
{
    // Hyphen is a legal D-Bus bus-name element character; a daemon-issued
    // name like ":1.worker-2" must authenticate, not be refused.
    const QUuid windowId = QUuid::createUuid();
    const QString name = QStringLiteral(":1.worker-2");
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(name, 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = name,
                                  .claimedProcessId = 4242});
    QVERIFY(result.accepted);
    QCOMPARE(result.proof->providerUniqueName(), name);
}

void MenuOwnershipTests::authenticatorEnforcesUniqueNameByteBoundary()
{
    // D-Bus caps bus names at exactly 255 bytes: 255 is valid, 256 is not.
    const QUuid windowId = QUuid::createUuid();
    const QString name255 = QStringLiteral(":1.")
        + QString(static_cast<int>(Protocol::kMaxProviderUniqueNameUtf8Bytes) - 3, QLatin1Char('a'));
    QCOMPARE(name255.toUtf8().size(), qsizetype(255));
    const QString name256 = name255 + QLatin1Char('a');
    QCOMPARE(name256.toUtf8().size(), qsizetype(256));

    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 11);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(name255, 4242);
    credentialSource.knownPids.insert(name256, 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult atBound = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = name255,
                                  .claimedProcessId = 4242});
    QVERIFY(atBound.accepted);

    const AuthenticationResult overBound = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = name256,
                                  .claimedProcessId = 4242});
    QVERIFY(!overBound.accepted);
    QCOMPARE(overBound.reasonCode, QStringLiteral("invalid-registration"));
    QVERIFY(!overBound.proof.has_value());
}

void MenuOwnershipTests::authenticatorRejectsFocusChangeDuringLookup()
{
    const QUuid windowId = QUuid::createUuid();
    // The source reports a different generation on the post-lookup re-read:
    // focus moved while the credential lookup was in flight.
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    class MutatingSource final : public ActiveWindowSource {
    public:
        // AGENT-NOTE: activeWindow() is a const seam; a fake that must count
        // observations marks the counter mutable rather than weakening the
        // production contract to non-const.
        mutable int calls = 0;
        QUuid windowId;

        [[nodiscard]] std::optional<ActiveWindowObservation> activeWindow() const override
        {
            ++calls;
            // Second read happens after the credential lookup: focus moved on.
            return observation(windowId, 4242, calls == 1 ? 11 : 12);
        }
    };

    MutatingSource source;
    source.windowId = windowId;
    ProviderAuthenticator authenticator(source, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("focus-changed"));
    QCOMPARE(source.calls, 2);
}

void MenuOwnershipTests::authenticatorProofCarriesVerifiedFacts()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, 33);
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(result.accepted);
    // The proof is the only adoption currency, so its fields must be exactly
    // the verified observations, never the raw claim's process id.
    QCOMPARE(result.proof->window().windowId, windowId);
    QCOMPARE(result.proof->window().processId, qint64(4242));
    QCOMPARE(result.proof->providerUniqueName(), QStringLiteral(":1.42"));
    QCOMPARE(result.proof->focusGeneration(), quint64(33));
}

QTEST_APPLESS_MAIN(MenuOwnershipTests)
#include "tst_menu_ownership.moc"
