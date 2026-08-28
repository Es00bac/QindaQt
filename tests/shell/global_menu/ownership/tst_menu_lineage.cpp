// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtTest>

// Lineage-side ownership tests: adoption of authenticator-issued proofs by
// the single lineage authority and revision/lineage-checked invocation.
// Authentication-rejection cases live in tst_menu_ownership.cpp.

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Ownership;

namespace {

ActiveWindowObservation observation(const QUuid &windowId, qint64 pid, quint64 focusGeneration)
{
    return ActiveWindowObservation{.window = WindowIdentity{.windowId = windowId, .processId = pid},
                                    .focusGeneration = focusGeneration};
}

// Issues a real, authenticator-minted proof through minimal inline fakes.
// There is no other way to obtain an AuthenticatedProvider — that is the
// forgeability property under test.
AuthenticatedProvider issueProof(const QUuid &windowId, quint64 focusGeneration)
{
    class LocalWindowSource final : public ActiveWindowSource {
    public:
        std::optional<ActiveWindowObservation> value;

        [[nodiscard]] std::optional<ActiveWindowObservation> activeWindow() const override
        {
            return value;
        }
    };

    class LocalCredentialSource final : public CredentialSource {
    public:
        [[nodiscard]] std::optional<qint64> processIdForUniqueName(const QString &) const override
        {
            return 4242;
        }
    };

    LocalWindowSource windowSource;
    windowSource.value = observation(windowId, 4242, focusGeneration);
    LocalCredentialSource credentials;
    ProviderAuthenticator authenticator(windowSource, credentials);
    const AuthenticationResult result = authenticator.authenticate(
        MenuProviderRegistration{.windowId = windowId,
                                  .providerUniqueName = QStringLiteral(":1.42"),
                                  .claimedProcessId = 4242});
    Q_ASSERT(result.accepted);
    return result.proof.value();
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

class MenuLineageTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
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

void MenuLineageTests::selectorStartsEmpty()
{
    ActiveProviderSelector selector;
    QVERIFY(!selector.current().has_value());
}

void MenuLineageTests::selectorAdoptsOnlyProofs()
{
    // AGENT-NOTE: the API has no adopt(registration, window) overload and
    // AuthenticatedProvider cannot be constructed outside the authenticator;
    // the type system forces the verified proof to be the adopted value.
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const SelectedProvider selection = selector.current().value();
    QCOMPARE(selection.window.windowId, windowId);
    QCOMPARE(selection.providerUniqueName, QStringLiteral(":1.42"));
    QCOMPARE(selection.focusGeneration, quint64(11));
}

void MenuLineageTests::selectorAssignsFreshEpochForNewWindow()
{
    ActiveProviderSelector selector;
    const QUuid firstWindow = QUuid::createUuid();
    selector.adopt(issueProof(firstWindow, 11));
    const SelectedProvider firstSelection = selector.current().value();
    QCOMPARE(firstSelection.revision, quint64(1));

    const QUuid secondWindow = QUuid::createUuid();
    selector.adopt(issueProof(secondWindow, 12));
    const SelectedProvider secondSelection = selector.current().value();
    QVERIFY(secondSelection.epoch != firstSelection.epoch);
    QCOMPARE(secondSelection.revision, quint64(1));
}

void MenuLineageTests::selectorKeepsEpochAndBumpsRevisionForSameWindow()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const QUuid epoch = selector.current()->epoch;

    selector.adopt(issueProof(windowId, 11));
    const SelectedProvider selection = selector.current().value();
    QCOMPARE(selection.epoch, epoch);
    QCOMPARE(selection.revision, quint64(2));
}

void MenuLineageTests::selectorClearRemovesCurrent()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    QVERIFY(selector.current().has_value());
    selector.clear();
    QVERIFY(!selector.current().has_value());
}

void MenuLineageTests::selectorInvalidatesOnFocusGenerationChange()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    QVERIFY(selector.current().has_value());

    // Same generation: no-op.
    selector.applyFocusGeneration(11);
    QVERIFY(selector.current().has_value());

    // Focus moved since authentication: the adoption is dropped.
    selector.applyFocusGeneration(12);
    QVERIFY(!selector.current().has_value());
}

void MenuLineageTests::invocationRejectsWithNoActiveProvider()
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

void MenuLineageTests::invocationRejectsStaleWindow()
{
    ActiveProviderSelector selector;
    const QUuid ownedWindow = QUuid::createUuid();
    selector.adopt(issueProof(ownedWindow, 11));

    const QUuid otherWindow = QUuid::createUuid();
    const Protocol::MenuTree tree = treeWithOneAction(otherWindow, QUuid::createUuid(), 1, true);
    const InvocationRequest request{
        .windowId = otherWindow, .epoch = tree.epoch, .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuLineageTests::invocationRejectsStaleEpoch()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const Protocol::MenuTree tree = treeWithOneAction(windowId, QUuid::createUuid(), 1, true);
    // The request carries an epoch from before the current adoption.
    const InvocationRequest request{
        .windowId = windowId, .epoch = QUuid::createUuid(), .revision = 1, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("stale-owner"));
}

void MenuLineageTests::invocationRejectsSameEpochStaleRevision()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    selector.adopt(issueProof(windowId, 11)); // same epoch, revision 2 now
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

void MenuLineageTests::invocationRejectsStaleTree()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
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

void MenuLineageTests::invocationRejectsUnknownAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("does-not-exist")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("unknown-action"));
}

void MenuLineageTests::invocationRejectsSubmenu()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
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

void MenuLineageTests::invocationRejectsDisabledAction()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, /*enabled=*/false);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("disabled"));
}

void MenuLineageTests::invocationAcceptsCurrentLineage()
{
    ActiveProviderSelector selector;
    const QUuid windowId = QUuid::createUuid();
    selector.adopt(issueProof(windowId, 11));
    const QUuid epoch = selector.current()->epoch;
    const quint64 revision = selector.current()->revision;
    const Protocol::MenuTree tree = treeWithOneAction(windowId, epoch, revision, /*enabled=*/true);
    const InvocationRequest request{
        .windowId = windowId, .epoch = epoch, .revision = revision, .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult result = InvocationGuard::evaluate(selector, tree, request);
    QVERIFY(result.accepted);
}

QTEST_APPLESS_MAIN(MenuLineageTests)
#include "tst_menu_lineage.moc"
