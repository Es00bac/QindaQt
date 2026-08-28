// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QHash>
#include <QtTest>

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

AuthenticatedProvider authenticated(const QUuid &windowId, quint64 focusGeneration)
{
    return AuthenticatedProvider{.window =
                                     WindowIdentity{.windowId = windowId, .processId = 4242},
                                  .providerUniqueName = QStringLiteral(":1.42"),
                                  .focusGeneration = focusGeneration};
}

Protocol::MenuTree treeWithOneAction(const QUuid &windowId, const QUuid &epoch, quint64 revision,
                                     bool enabled)
{
    Protocol::MenuItem item;
    item.id = QStringLiteral("fileNewAction");
    item.kind = Protocol::MenuItemKind::Action;
    item.text = QStringLiteral("New");
    item.enabled = enabled;

    Protocol::MenuTree tree;
    tree.ownerWindowId = windowId;
    tree.epoch = epoch;
    tree.revision = revision;
    tree.items = {item};
    return tree;
}

} // namespace

class MenuOwnershipTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void authenticatorAcceptsMatchingWindowAndPid();
    void authenticatorRejectsWhenNoActiveWindow();
    void authenticatorRejectsWhenNotActiveWindow();
    void authenticatorRejectsWhenCredentialUnavailable();
    void authenticatorRejectsPidMismatchWithClaim();
    void authenticatorRejectsPidMismatchWithActiveWindow();
    void authenticatorRejectsInvalidRegistration();
    void authenticatorRejectsFocusChangeDuringLookup();
    void authenticatorProofCarriesVerifiedFacts();

    void selectorStartsEmpty();
    void selectorAdoptsOnlyProofs();
    void selectorAssignsFreshEpochForNewWindow();
    void selectorKeepsEpochAndBumpsRevisionForSameWindow();
    void selectorClearRemovesCurrent();
    void selectorInvalidatesOnFocusGenerationChange();

    void invocationRejectsWithNoActiveProvider();
    void invocationRejectsStaleWindow();
    void invocationRejectsStaleEpoch();
    void invocationRejectsSameEpochStaleRevision();
    void invocationRejectsStaleTree();
    void invocationRejectsUnknownAction();
    void invocationRejectsSubmenu();
    void invocationRejectsDisabledAction();
    void invocationAcceptsCurrentLineage();
};

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
    QCOMPARE(result.proof.window.windowId, windowId);
    QCOMPARE(result.proof.focusGeneration, quint64(11));
}

void MenuOwnershipTests::authenticatorRejectsWhenNoActiveWindow()
{
    FakeActiveWindowSource windowSource;
    FakeCredentialSource credentialSource;
    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(QUuid::createUuid()));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("no-active-window"));
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

void MenuOwnershipTests::authenticatorRejectsFocusChangeDuringLookup()
{
    const QUuid windowId = QUuid::createUuid();
    // The source reports a different generation on the post-lookup re-read:
    // focus moved while the credential lookup was in flight.
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    class MutatingSource final : public ActiveWindowSource {
    public:
        int calls = 0;
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
    QCOMPARE(result.proof.window.windowId, windowId);
    QCOMPARE(result.proof.window.processId, qint64(4242));
    QCOMPARE(result.proof.providerUniqueName, QStringLiteral(":1.42"));
    QCOMPARE(result.proof.focusGeneration, quint64(33));
}

void MenuOwnershipTests::selectorStartsEmpty()
{
    ActiveProviderSelector selector;
    QVERIFY(!selector.current().has_value());
}

void MenuOwnershipTests::selectorAdoptsOnlyProofs()
{
    // AGENT-NOTE: the API has no adopt(registration, window) overload; the
    // type system forces the verified proof to be the adopted value.
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const SelectedProvider selection = selector.current().value();
    QCOMPARE(selection.window.windowId, windowId);
    QCOMPARE(selection.providerUniqueName, QStringLiteral(":1.42"));
    QCOMPARE(selection.focusGeneration, quint64(11));
}

void MenuOwnershipTests::selectorAssignsFreshEpochForNewWindow()
{
    ActiveProviderSelector selector;
    const QUuid firstWindow = QUuid::createUuid();
    selector.adopt(authenticated(firstWindow, 11));
    const SelectedProvider firstSelection = selector.current().value();
    QCOMPARE(firstSelection.revision, quint64(1));

    const QUuid secondWindow = QUuid::createUuid();
    selector.adopt(authenticated(secondWindow, 12));
    const SelectedProvider secondSelection = selector.current().value();
    QVERIFY(secondSelection.epoch != firstSelection.epoch);
    QCOMPARE(secondSelection.revision, quint64(1));
}

void MenuOwnershipTests::selectorKeepsEpochAndBumpsRevisionForSameWindow()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const QUuid epoch = selector.current()->epoch;

    selector.adopt(authenticated(windowId, 11));
    const SelectedProvider selection = selector.current().value();
    QCOMPARE(selection.epoch, epoch);
    QCOMPARE(selection.revision, quint64(2));
}

void MenuOwnershipTests::selectorClearRemovesCurrent()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    QVERIFY(selector.current().has_value());
    selector.clear();
    QVERIFY(!selector.current().has_value());
}

void MenuOwnershipTests::selectorInvalidatesOnFocusGenerationChange()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    QVERIFY(selector.current().has_value());

    // Same generation: no-op.
    selector.applyFocusGeneration(11);
    QVERIFY(selector.current().has_value());

    // Focus moved since authentication: the adoption is dropped.
    selector.applyFocusGeneration(12);
    QVERIFY(!selector.current().has_value());
}

void MenuOwnershipTests::invocationRejectsWithNoActiveProvider()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    const Protocol::MenuTree tree = treeWithOneAction(windowId, QUuid::createUuid(), 1, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = tree.epoch, .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("no-active-provider"));
}

void MenuOwnershipTests::invocationRejectsStaleWindow()
{
    ActiveProviderSelector selector;
    const QUuid ownedWindow = QUuid::createUuid();
    selector.adopt(authenticated(ownedWindow, 11));

    const QUuid otherWindow = QUuid::createUuid();
    const Protocol::MenuTree tree = treeWithOneAction(otherWindow, QUuid::createUuid(), 1, true);
    const InvocationRequest request{
        .windowId = otherWindow, .epoch = tree.epoch, .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsStaleEpoch()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const Protocol::MenuTree tree = treeWithOneAction(windowId, QUuid::createUuid(), 1, true);
    // The request carries an epoch from before the current adoption.
    const InvocationRequest request{
        .windowId = windowId, .epoch = QUuid::createUuid(), .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsSameEpochStaleRevision()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    selector.adopt(authenticated(windowId, 11)); // same epoch, revision 2 now
    const QUuid epoch = selector.current()->epoch;

    // The tree and request both describe the still-earlier revision 1
    // publication: same window, same epoch, but no longer authoritative.
    const Protocol::MenuTree staleTree = treeWithOneAction(windowId, epoch, 1, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, staleTree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsStaleTree()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const quint64 revision = selector.current()->revision;
    // The request matches the current lineage, but the tree it is paired with
    // was published under an older epoch; the action must not resolve.
    const Protocol::MenuTree staleTree =
        treeWithOneAction(windowId, QUuid::createUuid(), revision, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = selector.current()->epoch, .revision = revision, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, staleTree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsUnknownAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("does-not-exist")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("unknown-action"));
}

void MenuOwnershipTests::invocationRejectsSubmenu()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;

    Protocol::MenuItem submenu;
    submenu.id = QStringLiteral("fileMenu");
    submenu.kind = Protocol::MenuItemKind::Submenu;
    submenu.text = QStringLiteral("File");
    Protocol::MenuTree tree;
    tree.ownerWindowId = windowId;
    tree.epoch = epoch;
    tree.revision = revision;
    tree.items = {submenu};

    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("fileMenu")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("not-invocable"));
}

void MenuOwnershipTests::invocationRejectsDisabledAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, /*enabled=*/false);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("disabled"));
}

void MenuOwnershipTests::invocationAcceptsCurrentLineage()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(authenticated(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, /*enabled=*/true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(result.accepted);
}

QTEST_APPLESS_MAIN(MenuOwnershipTests)
#include "tst_menu_ownership.moc"
