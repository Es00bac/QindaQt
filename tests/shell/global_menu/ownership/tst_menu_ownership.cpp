// SPDX-License-Identifier: GPL-3.0-or-later

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
    std::optional<WindowIdentity> value;

    [[nodiscard]] std::optional<WindowIdentity> activeWindow() const override { return value; }
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

    void selectorStartsEmpty();
    void selectorAssignsFreshEpochForNewWindow();
    void selectorKeepsEpochAndBumpsRevisionForSameWindow();
    void selectorClearRemovesCurrent();

    void invocationRejectsWithNoActiveProvider();
    void invocationRejectsStaleWindow();
    void invocationRejectsStaleEpoch();
    void invocationRejectsStaleTree();
    void invocationRejectsUnknownAction();
    void invocationRejectsSubmenu();
    void invocationRejectsDisabledAction();
    void invocationAcceptsEnabledAction();
};

void MenuOwnershipTests::authenticatorAcceptsMatchingWindowAndPid()
{
    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    windowSource.value = WindowIdentity{.windowId = windowId, .processId = 4242};
    FakeCredentialSource credentialSource;
    credentialSource.knownPids.insert(QStringLiteral(":1.42"), 4242);

    ProviderAuthenticator authenticator(windowSource, credentialSource);
    const AuthenticationResult result = authenticator.authenticate(validRegistration(windowId));
    QVERIFY(result.accepted);
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
    windowSource.value = WindowIdentity{.windowId = QUuid::createUuid(), .processId = 4242};
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
    windowSource.value = WindowIdentity{.windowId = windowId, .processId = 4242};
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
    windowSource.value = WindowIdentity{.windowId = windowId, .processId = 4242};
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
    windowSource.value = WindowIdentity{.windowId = windowId, .processId = 5555};
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

void MenuOwnershipTests::selectorStartsEmpty()
{
    ActiveProviderSelector selector;
    QVERIFY(!selector.current().has_value());
}

void MenuOwnershipTests::selectorAssignsFreshEpochForNewWindow()
{
    ActiveProviderSelector selector;
    const QUuid firstWindow = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(firstWindow),
                                 WindowIdentity{.windowId = firstWindow, .processId = 111});
    const SelectedProvider firstSelection = selector.current().value();
    QCOMPARE(firstSelection.revision, quint64(1));

    const QUuid secondWindow = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(secondWindow),
                                 WindowIdentity{.windowId = secondWindow, .processId = 222});
    const SelectedProvider secondSelection = selector.current().value();
    QVERIFY(secondSelection.epoch != firstSelection.epoch);
    QCOMPARE(secondSelection.revision, quint64(1));
}

void MenuOwnershipTests::selectorKeepsEpochAndBumpsRevisionForSameWindow()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;

    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const SelectedProvider selection = selector.current().value();
    QCOMPARE(selection.epoch, epoch);
    QCOMPARE(selection.revision, quint64(2));
}

void MenuOwnershipTests::selectorClearRemovesCurrent()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    QVERIFY(selector.current().has_value());
    selector.clear();
    QVERIFY(!selector.current().has_value());
}

namespace {

Protocol::MenuTree treeWithOneAction(const QUuid &windowId, const QUuid &epoch, bool enabled)
{
    Protocol::MenuItem item;
    item.id = QStringLiteral("fileNewAction");
    item.kind = Protocol::MenuItemKind::Action;
    item.text = QStringLiteral("New");
    item.enabled = enabled;

    Protocol::MenuTree tree;
    tree.ownerWindowId = windowId;
    tree.epoch = epoch;
    tree.revision = 1;
    tree.items = {item};
    return tree;
}

} // namespace

void MenuOwnershipTests::invocationRejectsWithNoActiveProvider()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    const Protocol::MenuTree tree = treeWithOneAction(windowId, QUuid::createUuid(), true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = tree.epoch, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("no-active-provider"));
}

void MenuOwnershipTests::invocationRejectsStaleWindow()
{
    ActiveProviderSelector selector;
    const QUuid ownedWindow = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(ownedWindow),
                                 WindowIdentity{.windowId = ownedWindow, .processId = 111});

    const QUuid otherWindow = QUuid::createUuid();
    const Protocol::MenuTree tree = treeWithOneAction(otherWindow, QUuid::createUuid(), true);
    const InvocationRequest request{
        .windowId = otherWindow, .epoch = tree.epoch, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsStaleEpoch()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const Protocol::MenuTree tree = treeWithOneAction(windowId, QUuid::createUuid(), true);
    // The request carries an epoch from before the current adoption.
    const InvocationRequest request{
        .windowId = windowId, .epoch = QUuid::createUuid(), .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsStaleTree()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;
    // The request matches the current lineage, but the tree it is paired with
    // was published under an older epoch; the action must not resolve.
    const Protocol::MenuTree staleTree =
        treeWithOneAction(windowId, QUuid::createUuid(), true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, staleTree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuOwnershipTests::invocationRejectsUnknownAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .actionId = QStringLiteral("does-not-exist")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("unknown-action"));
}

void MenuOwnershipTests::invocationRejectsSubmenu()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;

    Protocol::MenuItem submenu;
    submenu.id = QStringLiteral("fileMenu");
    submenu.kind = Protocol::MenuItemKind::Submenu;
    submenu.text = QStringLiteral("File");
    Protocol::MenuTree tree;
    tree.ownerWindowId = windowId;
    tree.epoch = epoch;
    tree.items = {submenu};

    const InvocationRequest request{.windowId = windowId, .epoch = epoch, .actionId = QStringLiteral("fileMenu")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("not-invocable"));
}

void MenuOwnershipTests::invocationRejectsDisabledAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, /*enabled=*/false);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("disabled"));
}

void MenuOwnershipTests::invocationAcceptsEnabledAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adoptAuthenticated(validRegistration(windowId),
                                 WindowIdentity{.windowId = windowId, .processId = 111});
    const QUuid epoch = selector.current()->epoch;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, /*enabled=*/true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(result.accepted);
}

QTEST_APPLESS_MAIN(MenuOwnershipTests)
#include "tst_menu_ownership.moc"
