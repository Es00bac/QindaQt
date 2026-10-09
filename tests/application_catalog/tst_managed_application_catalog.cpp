// SPDX-License-Identifier: GPL-3.0-or-later
#include "managed_catalog_fixtures.h"

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_presentation.h"
#include "qindaqt/shell_launcher/launcher_search_ranker.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace ManagedCatalogFixtures;

class ManagedApplicationCatalogTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void bothOriginsShareOrdinaryCatalogBehavior();
    void identitySurvivesRecipeAndPresentationUpdates();
    void copiedDesktopKeysCannotMintManagedBinding();
    void everyNativeClaimMasksManagedIdentity();
    void malformedBindingRejectsWholeSnapshot();
    void duplicateIdentityRejectsWholeSnapshot();
    void malformedDisplayRejectsWholeSnapshot();
    void utf8ByteBoundsAreEnforced();
    void overLimitSnapshotsAreAtomic();
    void incompleteNativeInspectionWithholdsManagedRows();
    void combinedEntryBoundWithholdsManagedRows();
    void nativeOnlyBehaviorRemainsUnchanged();
};

void ManagedApplicationCatalogTest::bothOriginsShareOrdinaryCatalogBehavior()
{
    const auto android = registration();
    auto windows = registration(ApplicationOrigin::Windows);
    windows.name = QStringLiteral("Fixture Notepad");
    const auto catalog = ApplicationCatalog::build(
        {{QStringLiteral("native.fixture"), desktop()}}, {android, windows});
    QVERIFY(catalog.diagnostics().isEmpty());
    QCOMPARE(catalog.entries().size(), 3);
    QCOMPARE(catalog.entries().first().name, android.name);
    const auto androidId = managedApplicationEntryId(android.binding);
    QCOMPARE(androidId, QStringLiteral("qindaqt.managed.android.12345678-1234-4234-8234-123456789abc"));
    const auto intent = catalog.makeLaunchIntent(androidId);
    QVERIFY(intent.ok());
    QCOMPARE(intent.intent->managedBinding, std::optional(android.binding));
    QCOMPARE(catalog.makeLaunchIntent(androidId, QStringLiteral("arbitrary")).error,
             LaunchIntentError::UnknownAction);
    PinnedApplications pinned;
    QCOMPARE(pinned.pin(androidId), PinError::None);
    RecentApplications recent;
    QCOMPARE(recent.record(androidId), RecentError::None);
    const auto presentation = LauncherPresentationModel::build(catalog, pinned, recent);
    QCOMPARE(presentation.status, LauncherStatus::Ready);
    QVERIFY(presentation.itemAt(0));
    QCOMPARE(presentation.itemAt(0)->entryId, androidId);
    QCOMPARE(presentation.itemAt(0)->origin, ApplicationOrigin::Android);
    QVERIFY(presentation.itemAt(0)->accessibleDescription.contains(QStringLiteral("Android application")));
    const auto ranked = LauncherSearchRanker::search(catalog.entries(), QStringLiteral("arithmetic"));
    QVERIFY(ranked.ok());
    QCOMPARE(ranked.results.size(), 2);
    const auto windowsItem = LauncherPresentationModel::itemFor(
        *catalog.entry(managedApplicationEntryId(windows.binding)), false);
    QCOMPARE(windowsItem.origin, ApplicationOrigin::Windows);
    QVERIFY(windowsItem.accessibleDescription.contains(QStringLiteral("Windows application")));
}

void ManagedApplicationCatalogTest::identitySurvivesRecipeAndPresentationUpdates()
{
    auto value = registration();
    const auto id = managedApplicationEntryId(value.binding);
    const auto oldIntent = ApplicationCatalog::build({}, {value}).makeLaunchIntent(id);
    value.name = QStringLiteral("Renamed fixture");
    value.binding.registrationRevision = 2;
    value.binding.recipeDigest = QString(64, QLatin1Char('b'));
    QCOMPARE(managedApplicationEntryId(value.binding), id);
    const auto updated = ApplicationCatalog::build({}, {value});
    const auto intent = updated.makeLaunchIntent(id);
    QVERIFY(intent.ok());
    QCOMPARE(intent.intent->managedBinding->registrationRevision, quint64(2));
    QVERIFY(oldIntent.intent->managedBinding != intent.intent->managedBinding);
    QCOMPARE(intent.intent->displayName, value.name);
    QVERIFY(!ApplicationCatalog::build({}, QVector<ManagedApplicationRegistration>{})
                 .makeLaunchIntent(id).ok());
}

void ManagedApplicationCatalogTest::copiedDesktopKeysCannotMintManagedBinding()
{
    const auto value = registration();
    const auto id = managedApplicationEntryId(value.binding);
    const auto catalog = ApplicationCatalog::build({{id, desktop(
        QStringLiteral("X-QindaQt-Origin=Android\nX-QindaQt-RegistrationRevision=1\n"
                       "StartupWMClass=com.android.calculator2\n"))}});
    QVERIFY(catalog.entry(id));
    QVERIFY(!catalog.entry(id)->managedBinding);
    QVERIFY(!catalog.makeLaunchIntent(id).intent->managedBinding);
    QCOMPARE(LauncherPresentationModel::itemFor(*catalog.entry(id), false).origin,
             ApplicationOrigin::Native);
}

void ManagedApplicationCatalogTest::everyNativeClaimMasksManagedIdentity()
{
    const auto value = registration();
    const auto id = managedApplicationEntryId(value.binding);
    for (const QString &body : {desktop(), desktop(QStringLiteral("Hidden=true\n")),
                                desktop(QStringLiteral("NoDisplay=true\n")),
                                QStringLiteral("not a desktop entry")}) {
        const auto catalog = ApplicationCatalog::build({{id, body}}, {value});
        if (const auto entry = catalog.entry(id))
            QVERIFY(!entry->managedBinding);
        for (const auto &entry : catalog.entries())
            QVERIFY(!entry.managedBinding);
    }
    const auto handlers = ApplicationCatalog::build(
        {{id, desktop(QStringLiteral("NoDisplay=true\n"))}}, {value},
        ApplicationVisibility::IncludeNoDisplay);
    QVERIFY(handlers.entry(id));
    QVERIFY(!handlers.entry(id)->managedBinding);
}

void ManagedApplicationCatalogTest::malformedBindingRejectsWholeSnapshot()
{
    const auto good = registration();
    QVector<ManagedApplicationBinding> bad;
    auto binding = good.binding;
    binding.origin = ApplicationOrigin::Native; bad.append(binding);
    binding = good.binding; binding.origin = static_cast<ApplicationOrigin>(99); bad.append(binding);
    binding = good.binding; binding.registrationRevision = 0; bad.append(binding);
    binding = good.binding; binding.registrationId = binding.registrationId.toUpper(); bad.append(binding);
    binding = good.binding; binding.registrationId = QStringLiteral("00000000-0000-0000-0000-000000000000"); bad.append(binding);
    binding = good.binding; binding.registrationId = QStringLiteral("bad"); bad.append(binding);
    binding = good.binding; binding.recipeDigest = QString(64, QLatin1Char('z')); bad.append(binding);
    binding = good.binding; binding.recipeDigest = QString(64, QLatin1Char('A')); bad.append(binding);
    for (const auto &invalid : bad) {
        auto value = registration(ApplicationOrigin::Windows);
        value.binding = invalid;
        QVERIFY(!isValidManagedApplicationBinding(invalid));
        QVERIFY(managedApplicationEntryId(invalid).isEmpty());
        const auto catalog = ApplicationCatalog::build(
            {{QStringLiteral("native.fixture"), desktop()}}, {good, value});
        QCOMPARE(catalog.entries().size(), 1);
        QCOMPARE(catalog.diagnostics().last().kind, DiagnosticKind::InvalidManagedSnapshot);
    }
}

void ManagedApplicationCatalogTest::duplicateIdentityRejectsWholeSnapshot()
{
    const auto value = registration();
    const auto catalog = ApplicationCatalog::build({}, {value, value});
    QVERIFY(catalog.entries().isEmpty());
    QCOMPARE(catalog.diagnostics().last().kind, DiagnosticKind::InvalidManagedSnapshot);
}

void ManagedApplicationCatalogTest::malformedDisplayRejectsWholeSnapshot()
{
    auto value = registration();
    value.name = QStringLiteral("bad\nlabel");
    QVERIFY(!validateManagedApplications({value}));
    value = registration(); value.name = QString(1, QChar(0xd800));
    QVERIFY(!validateManagedApplications({value}));
    value = registration(); value.keywords.append(value.keywords.first());
    QVERIFY(!validateManagedApplications({value}));
    value = registration(); value.categories = {QStringLiteral("../../arbitrary")};
    QVERIFY(!validateManagedApplications({value}));
    value = registration(); value.name = QStringLiteral("   ");
    QVERIFY(!validateManagedApplications({value}));
}

void ManagedApplicationCatalogTest::utf8ByteBoundsAreEnforced()
{
    auto value = registration();
    value.name = QString(129, QChar(0x00e9));
    QVERIFY(value.name.size() < Bounds::maxNameLength);
    QVERIFY(!validateManagedApplications({value}));
    value.name = QString(128, QChar(0x00e9));
    QVERIFY(validateManagedApplications({value}));
}

void ManagedApplicationCatalogTest::overLimitSnapshotsAreAtomic()
{
    const auto value = registration();
    QVector<ManagedApplicationRegistration> registrations(Bounds::maxVisibleEntries + 1, value);
    const auto catalog = ApplicationCatalog::build({}, registrations);
    QVERIFY(catalog.entries().isEmpty());
    QCOMPARE(catalog.diagnostics().last().kind, DiagnosticKind::InvalidManagedSnapshot);
}

void ManagedApplicationCatalogTest::incompleteNativeInspectionWithholdsManagedRows()
{
    QVector<SourceDocument> documents(Bounds::maxSourceDocuments + 1,
                                    {QStringLiteral("native.hidden"), desktop(QStringLiteral("Hidden=true\n"))});
    const auto catalog = ApplicationCatalog::build(documents, {registration()});
    QVERIFY(catalog.entries().isEmpty());
    QVERIFY(catalog.diagnosticsTruncated());
}

void ManagedApplicationCatalogTest::combinedEntryBoundWithholdsManagedRows()
{
    QVector<SourceDocument> documents;
    for (int i = 0; i < Bounds::maxVisibleEntries; ++i)
        documents.append({QStringLiteral("native.%1").arg(i), desktop()});
    const auto catalog = ApplicationCatalog::build(documents, {registration()});
    QCOMPARE(catalog.entries().size(), Bounds::maxVisibleEntries);
    QCOMPARE(catalog.diagnostics().last().kind, DiagnosticKind::InvalidManagedSnapshot);
    for (const auto &entry : catalog.entries())
        QVERIFY(!entry.managedBinding);
}

void ManagedApplicationCatalogTest::nativeOnlyBehaviorRemainsUnchanged()
{
    const QVector<SourceDocument> documents{{QStringLiteral("native.fixture"), desktop()}};
    const auto before = ApplicationCatalog::build(documents);
    const auto after = ApplicationCatalog::build(documents, QVector<ManagedApplicationRegistration>{});
    QCOMPARE(before.entries(), after.entries());
    QCOMPARE(before.diagnostics(), after.diagnostics());
}

QTEST_GUILESS_MAIN(ManagedApplicationCatalogTest)
#include "tst_managed_application_catalog.moc"
