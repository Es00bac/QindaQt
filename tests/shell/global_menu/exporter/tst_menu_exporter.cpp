// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>
#include <qindaqt/shell/global_menu/exporter/menu_source.h>

#include <QtTest>

using namespace QindaQt::Shell::GlobalMenu::Exporter;
using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

class FakeMenuSource final : public MenuSource {
public:
    MenuTree next;

    [[nodiscard]] MenuTree snapshot() const override { return next; }
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
    void firstValidPullIsPublishedAtRevisionOne();
    void identicalContentIsUnchangedAndKeepsRevision();
    void changedContentIsPublishedAndBumpsRevision();
    void ownerChangeStartsFreshEpochAndRevision();
    void invalidPullIsRejectedAndKeepsLastAccepted();
};

void MenuExporterTests::firstValidPullIsPublishedAtRevisionOne()
{
    FakeMenuSource source;
    source.next.ownerWindowId = QUuid::createUuid();
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    MenuExporter exporter(source);
    const ExportResult result = exporter.refresh();
    QCOMPARE(result.outcome, ExportOutcome::Published);
    QVERIFY(result.validation.accepted);
    QCOMPARE(exporter.lastAccepted()->revision, quint64(1));
    QVERIFY(!exporter.lastAccepted()->epoch.isNull());
}

void MenuExporterTests::identicalContentIsUnchangedAndKeepsRevision()
{
    FakeMenuSource source;
    source.next.ownerWindowId = QUuid::createUuid();
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    MenuExporter exporter(source);
    exporter.refresh();
    const quint64 firstRevision = exporter.lastAccepted()->revision;
    const QUuid firstEpoch = exporter.lastAccepted()->epoch;

    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Unchanged);
    QVERIFY(second.delta.identical());
    QCOMPARE(exporter.lastAccepted()->revision, firstRevision);
    QCOMPARE(exporter.lastAccepted()->epoch, firstEpoch);
}

void MenuExporterTests::changedContentIsPublishedAndBumpsRevision()
{
    FakeMenuSource source;
    const QUuid windowId = QUuid::createUuid();
    source.next.ownerWindowId = windowId;
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    MenuExporter exporter(source);
    exporter.refresh();
    const QUuid firstEpoch = exporter.lastAccepted()->epoch;

    source.next.items.append(action(QStringLiteral("b"), QStringLiteral("Save")));
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Published);
    QVERIFY(!second.delta.identical());
    QCOMPARE(exporter.lastAccepted()->revision, quint64(2));
    QCOMPARE(exporter.lastAccepted()->epoch, firstEpoch);
}

void MenuExporterTests::ownerChangeStartsFreshEpochAndRevision()
{
    FakeMenuSource source;
    source.next.ownerWindowId = QUuid::createUuid();
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    MenuExporter exporter(source);
    exporter.refresh();
    const QUuid firstEpoch = exporter.lastAccepted()->epoch;

    source.next.ownerWindowId = QUuid::createUuid();
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::Published);
    QCOMPARE(exporter.lastAccepted()->revision, quint64(1));
    QVERIFY(exporter.lastAccepted()->epoch != firstEpoch);
}

void MenuExporterTests::invalidPullIsRejectedAndKeepsLastAccepted()
{
    FakeMenuSource source;
    source.next.ownerWindowId = QUuid::createUuid();
    source.next.items = {action(QStringLiteral("a"), QStringLiteral("New"))};

    MenuExporter exporter(source);
    exporter.refresh();
    const MenuTree goodTree = exporter.lastAccepted().value();

    source.next.items.append(action(QStringLiteral("a"), QStringLiteral("Duplicate id")));
    const ExportResult second = exporter.refresh();
    QCOMPARE(second.outcome, ExportOutcome::RejectedInvalid);
    QVERIFY(!second.validation.accepted);
    QCOMPARE(second.validation.reasonCode, QStringLiteral("duplicate-id"));
    QCOMPARE(exporter.lastAccepted().value(), goodTree);
}

QTEST_APPLESS_MAIN(MenuExporterTests)
#include "tst_menu_exporter.moc"
