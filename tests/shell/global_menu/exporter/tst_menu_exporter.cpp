// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>
#include <qindaqt/shell/global_menu/exporter/menu_source.h>

#include <QtCore/QHash>

#include <QtTest>

#include <optional>

using namespace QindaQt::Shell::GlobalMenu::Exporter;
using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

class FakeMenuSource final : public MenuSource {
public:
    MenuTree next;
    bool complete = true;
    QString defectCode;

    [[nodiscard]] MenuSnapshot snapshot() const override
    {
        return MenuSnapshot{.tree = next, .complete = complete, .defectCode = defectCode};
    }
};

// AGENT-NOTE: in production, shell composition backs this seam with the
// ownership selector; here a keyed fake keeps the exporter contract isolated.
class FakeLineageSource final : public ExportLineageSource {
public:
    QHash<QUuid, ExportLineage> lineages;

    [[nodiscard]] std::optional<ExportLineage> lineageFor(const QUuid &ownerWindowId) const override
    {
        const auto it = lineages.constFind(ownerWindowId);
        if (it == lineages.constEnd()) {
            return std::nullopt;
        }
        return *it;
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

} // namespace

class MenuExporterTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void firstValidPullIsPublishedUnderAuthoritativeLineage();
    void identicalContentIsUnchangedAndRestamped();
    void changedContentIsPublishedAsChanged();
    void invalidPullIsRejectedAndKeepsLastAccepted();
    void incompleteSnapshotIsRejectedWholeWithoutValidation();
    void ownerWithoutAuthorityIsRejectedAndKeepsLastAccepted();
    void exportedTreeCarriesTheSelectorLineageVerbatim();
};

void MenuExporterTests::firstValidPullIsPublishedUnderAuthoritativeLineage()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    const QUuid epoch = QUuid::createUuid();
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 7});

    MenuExporter exporter(source, lineages);
    const ExportResult result = exporter.refresh();
    QCOMPARE(result.outcome, ExportOutcome::Published);
    QVERIFY(result.validation.accepted);
    QVERIFY(result.changed);
    QVERIFY(exporter.lastAccepted().has_value());
    QCOMPARE(exporter.lastAccepted()->epoch, epoch);
    QCOMPARE(exporter.lastAccepted()->revision, quint64(7));
}

void MenuExporterTests::identicalContentIsUnchangedAndRestamped()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    const QUuid epoch = QUuid::createUuid();
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 1});

    MenuExporter exporter(source, lineages);
    exporter.refresh();

    // A re-adoption advances the revision without changing content: the
    // outcome is Unchanged, but the stored tree must carry the new revision
    // so it stays invocable against the advanced selector.
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 2});
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Unchanged);
    QVERIFY(!second.changed);
    QVERIFY(exporter.lastAccepted().has_value());
    QCOMPARE(exporter.lastAccepted()->revision, quint64(2));
    QCOMPARE(exporter.lastAccepted()->epoch, epoch);
}

void MenuExporterTests::changedContentIsPublishedAsChanged()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    const QUuid epoch = QUuid::createUuid();
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 1});

    MenuExporter exporter(source, lineages);
    exporter.refresh();

    source.next.items.append(action(QStringLiteral("b"), QStringLiteral("Save")));
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 2});
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Published);
    QVERIFY(second.changed);
    QCOMPARE(exporter.lastAccepted()->revision, quint64(2));
}

void MenuExporterTests::invalidPullIsRejectedAndKeepsLastAccepted()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    lineages.lineages.insert(windowId, ExportLineage{.epoch = QUuid::createUuid(), .revision = 1});

    MenuExporter exporter(source, lineages);
    exporter.refresh();
    const MenuTree goodTree = exporter.lastAccepted().value();

    source.next.items.append(action(QStringLiteral("a"), QStringLiteral("Duplicate id")));
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::RejectedInvalid);
    QVERIFY(!second.validation.accepted);
    QCOMPARE(second.validation.reasonCode, QStringLiteral("duplicate-id"));
    QCOMPARE(exporter.lastAccepted().value(), goodTree);
}

void MenuExporterTests::incompleteSnapshotIsRejectedWholeWithoutValidation()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    lineages.lineages.insert(windowId, ExportLineage{.epoch = QUuid::createUuid(), .revision = 1});

    MenuExporter exporter(source, lineages);
    exporter.refresh();
    const MenuTree goodTree = exporter.lastAccepted().value();

    // The "prefix" content would validate on its own; only the completeness
    // verdict may reject it, and the exporter must do exactly that.
    source.next.items.append(action(QStringLiteral("b"), QStringLiteral("Truncated prefix")));
    source.complete = false;
    source.defectCode = QStringLiteral("too-many-items");
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::RejectedIncomplete);
    QCOMPARE(second.defectCode, QStringLiteral("too-many-items"));
    QCOMPARE(exporter.lastAccepted().value(), goodTree);
}

void MenuExporterTests::ownerWithoutAuthorityIsRejectedAndKeepsLastAccepted()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    lineages.lineages.insert(windowId, ExportLineage{.epoch = QUuid::createUuid(), .revision = 1});

    MenuExporter exporter(source, lineages);
    exporter.refresh();
    const MenuTree goodTree = exporter.lastAccepted().value();

    // Focus moved: no lineage exists for the newly focused window, so the
    // pull fails closed instead of inventing lineage for it.
    source.next.ownerWindowId = QUuid::createUuid();
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::RejectedNoAuthority);
    QCOMPARE(exporter.lastAccepted().value(), goodTree);
}

void MenuExporterTests::exportedTreeCarriesTheSelectorLineageVerbatim()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    FakeLineageSource lineages;
    const QUuid epoch = QUuid::createUuid();
    lineages.lineages.insert(windowId, ExportLineage{.epoch = epoch, .revision = 42});

    MenuExporter exporter(source, lineages);
    exporter.refresh();

    QCOMPARE(exporter.lastAccepted()->ownerWindowId, windowId);
    QCOMPARE(exporter.lastAccepted()->epoch, epoch);
    QCOMPARE(exporter.lastAccepted()->revision, quint64(42));
}

QTEST_APPLESS_MAIN(MenuExporterTests)
#include "tst_menu_exporter.moc"
