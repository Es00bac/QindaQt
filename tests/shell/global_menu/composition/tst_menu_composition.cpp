// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>
#include <qindaqt/shell/global_menu/exporter/menu_source.h>
#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/active_window_source.h>
#include <qindaqt/shell/global_menu/ownership/credential_source.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>

#include <QtCore/QHash>
#include <QtTest>

// AGENT-CONTRACT: this suite is the ordinary-public-API composition proof the
// review requires — authenticate, adopt the returned proof, export through a
// selector-backed lineage seam, and invoke — with no shell wiring in between.

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Exporter;
using namespace QindaQt::Shell::GlobalMenu::Ownership;
using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

class FakeActiveWindowSource final : public ActiveWindowSource {
public:
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

class FakeMenuSource final : public MenuSource {
public:
    MenuTree next;

    [[nodiscard]] MenuSnapshot snapshot() const override
    {
        return MenuSnapshot{.tree = next, .complete = true, .defectCode = {}};
    }
};

// The composition-side adapter the shell will own: it backs the exporter's
// lineage seam with the selector, so owner/epoch/revision share one source
// of truth across export and invocation.
class SelectorLineageSource final : public ExportLineageSource {
public:
    const ActiveProviderSelector *selector = nullptr;

    [[nodiscard]] std::optional<ExportLineage> lineageFor(const QUuid &ownerWindowId) const override
    {
        if (selector == nullptr) {
            return std::nullopt;
        }
        const std::optional<SelectedProvider> current = selector->current();
        if (!current || current->window.windowId != ownerWindowId) {
            return std::nullopt;
        }
        return ExportLineage{.epoch = current->epoch, .revision = current->revision};
    }
};

MenuItem action(const QString &id, const QString &text)
{
    MenuItem item;
    item.id = id;
    item.kind = MenuItemKind::Action;
    item.text = text;
    return item;
}

struct CompositionFixture {
    QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource windowSource;
    FakeCredentialSource credentials;
    FakeMenuSource menuSource;
    ActiveProviderSelector selector;
    SelectorLineageSource lineageSource;

    CompositionFixture()
    {
        windowSource.value = ActiveWindowObservation{
            .window = WindowIdentity{.windowId = windowId, .processId = 4242},
            .focusGeneration = 11};
        credentials.knownPids.insert(QStringLiteral(":1.42"), 4242);
        menuSource.next.ownerWindowId = windowId;
        menuSource.next.items = {action(QStringLiteral("fileNewAction"), QStringLiteral("New"))};
        lineageSource.selector = &selector;
    }

    MenuProviderRegistration registration() const
    {
        return MenuProviderRegistration{.windowId = windowId,
                                         .providerUniqueName = QStringLiteral(":1.42"),
                                         .claimedProcessId = 4242};
    }
};

} // namespace

class MenuCompositionTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void ordinaryCompositionProducesAnInvocableExport();
    void sameEpochOlderRevisionFailsAfterReadoption();
    void changedContentWithoutReadoptionFailsClosed();
    void focusGenerationChangeInvalidatesTheWholeComposition();
    void providerForUnfocusedWindowCannotPublishOrInvoke();
};

void MenuCompositionTests::ordinaryCompositionProducesAnInvocableExport()
{
    CompositionFixture fx;
    ProviderAuthenticator authenticator(fx.windowSource, fx.credentials);
    MenuExporter exporter(fx.menuSource, fx.lineageSource);

    const AuthenticationResult auth = authenticator.authenticate(fx.registration());
    QVERIFY(auth.accepted);
    fx.selector.adopt(auth.proof.value());

    const ExportResult exportResult = exporter.refresh();
    QCOMPARE(exportResult.outcome, ExportOutcome::Published);
    QVERIFY(exporter.lastAccepted().has_value());

    // The request is built from values any consumer can read off the public
    // exported tree — no private lineage knowledge required.
    const MenuTree published = exporter.lastAccepted().value();
    const InvocationRequest request{.windowId = published.ownerWindowId,
                                     .epoch = published.epoch,
                                     .revision = published.revision,
                                     .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult invocation = InvocationGuard::evaluate(fx.selector, published, request);
    QVERIFY(invocation.accepted);
}

void MenuCompositionTests::sameEpochOlderRevisionFailsAfterReadoption()
{
    CompositionFixture fx;
    ProviderAuthenticator authenticator(fx.windowSource, fx.credentials);
    MenuExporter exporter(fx.menuSource, fx.lineageSource);

    const AuthenticationResult initialAuth = authenticator.authenticate(fx.registration());
    QVERIFY(initialAuth.accepted);
    fx.selector.adopt(initialAuth.proof.value());
    const ExportResult initialExport = exporter.refresh();
    QCOMPARE(initialExport.outcome, ExportOutcome::Published);
    const MenuTree firstPublication = exporter.lastAccepted().value();

    // Content changes; composition re-authenticates and re-adopts, keeping
    // the same epoch but advancing the revision.
    fx.menuSource.next.items.append(action(QStringLiteral("fileSaveAction"), QStringLiteral("Save")));
    const AuthenticationResult reauth = authenticator.authenticate(fx.registration());
    QVERIFY(reauth.accepted);
    fx.selector.adopt(reauth.proof.value());
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Published);
    QVERIFY(second.changed);
    QCOMPARE(firstPublication.epoch, exporter.lastAccepted()->epoch);
    QVERIFY(firstPublication.revision < exporter.lastAccepted()->revision);

    // A UI still looking at the older same-epoch publication is stale, for
    // both its cached tree and its request.
    const InvocationRequest staleRequest{.windowId = firstPublication.ownerWindowId,
                                          .epoch = firstPublication.epoch,
                                          .revision = firstPublication.revision,
                                          .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult stale = InvocationGuard::evaluate(fx.selector, firstPublication, staleRequest);
    QVERIFY(!stale.accepted);
    QCOMPARE(stale.reasonCode, QStringLiteral("stale-owner"));

    // A fresh request against the current publication is accepted.
    const MenuTree current = exporter.lastAccepted().value();
    const InvocationRequest freshRequest{.windowId = current.ownerWindowId,
                                          .epoch = current.epoch,
                                          .revision = current.revision,
                                          .actionId = QStringLiteral("fileNewAction")};
    QVERIFY(InvocationGuard::evaluate(fx.selector, current, freshRequest).accepted);
}

void MenuCompositionTests::changedContentWithoutReadoptionFailsClosed()
{
    CompositionFixture fx;
    ProviderAuthenticator authenticator(fx.windowSource, fx.credentials);
    MenuExporter exporter(fx.menuSource, fx.lineageSource);

    const AuthenticationResult auth = authenticator.authenticate(fx.registration());
    QVERIFY(auth.accepted);
    fx.selector.adopt(auth.proof.value());
    const ExportResult initialExport = exporter.refresh();
    QCOMPARE(initialExport.outcome, ExportOutcome::Published);
    const MenuTree goodTree = exporter.lastAccepted().value();

    // The replay adversary at composition level: the provider pushes changed
    // content without re-authenticating, so the selector — and therefore the
    // lineage seam — still reports the revision the consumer already saw.
    // The exporter must refuse the changed pull and retain the last good
    // tree instead of silently re-authorizing stale semantics.
    fx.menuSource.next.items.append(action(QStringLiteral("fileSaveAction"), QStringLiteral("Save")));
    const ExportResult stale = exporter.refresh();
    QCOMPARE(stale.outcome, ExportOutcome::RejectedStaleLineage);
    QCOMPARE(stale.defectCode, QStringLiteral("unchanged-revision"));
    QCOMPARE(exporter.lastAccepted().value(), goodTree);

    // The retained publication remains invocable; the replayed content never
    // became truth.
    const InvocationRequest request{.windowId = goodTree.ownerWindowId,
                                     .epoch = goodTree.epoch,
                                     .revision = goodTree.revision,
                                     .actionId = QStringLiteral("fileNewAction")};
    QVERIFY(InvocationGuard::evaluate(fx.selector, goodTree, request).accepted);
}

void MenuCompositionTests::focusGenerationChangeInvalidatesTheWholeComposition()
{
    CompositionFixture fx;
    ProviderAuthenticator authenticator(fx.windowSource, fx.credentials);
    MenuExporter exporter(fx.menuSource, fx.lineageSource);

    const AuthenticationResult auth = authenticator.authenticate(fx.registration());
    QVERIFY(auth.accepted);
    fx.selector.adopt(auth.proof.value());
    const ExportResult initialExport = exporter.refresh();
    QCOMPARE(initialExport.outcome, ExportOutcome::Published);
    const MenuTree published = exporter.lastAccepted().value();

    // Focus moves on: the composition's invalidation seam drops the adoption
    // before any export or invocation can run against it.
    fx.selector.applyFocusGeneration(auth.proof->focusGeneration() + 1);
    QVERIFY(!fx.selector.current().has_value());

    const InvocationRequest request{.windowId = published.ownerWindowId,
                                     .epoch = published.epoch,
                                     .revision = published.revision,
                                     .actionId = QStringLiteral("fileNewAction")};
    const InvocationResult invocation = InvocationGuard::evaluate(fx.selector, published, request);
    QVERIFY(!invocation.accepted);
    QCOMPARE(invocation.reasonCode, QStringLiteral("no-active-provider"));
}

void MenuCompositionTests::providerForUnfocusedWindowCannotPublishOrInvoke()
{
    CompositionFixture fx;
    ProviderAuthenticator authenticator(fx.windowSource, fx.credentials);
    MenuExporter exporter(fx.menuSource, fx.lineageSource);

    // The registering peer names a window that is not the authenticated
    // active window; nothing is adopted and the export has no authority.
    const QUuid unfocused = QUuid::createUuid();
    MenuProviderRegistration forged{.windowId = unfocused,
                                     .providerUniqueName = QStringLiteral(":1.42"),
                                     .claimedProcessId = 4242};
    const AuthenticationResult auth = authenticator.authenticate(forged);
    QVERIFY(!auth.accepted);
    QCOMPARE(auth.reasonCode, QStringLiteral("not-active-window"));

    fx.menuSource.next.ownerWindowId = unfocused;
    const ExportResult exportResult = exporter.refresh();
    QCOMPARE(exportResult.outcome, ExportOutcome::RejectedNoAuthority);
    QVERIFY(!exporter.lastAccepted().has_value());
}

QTEST_APPLESS_MAIN(MenuCompositionTests)
#include "tst_menu_composition.moc"
