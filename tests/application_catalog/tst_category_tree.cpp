// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/application_catalog/category_tree.h"

#include <QtTest>

using namespace QindaQt::ApplicationCatalog;
using QindaQt::ShellLauncher::ApplicationEntry;

namespace {

ApplicationEntry entry(const QString &id, const QString &name,
                       const QStringList &categories)
{
    ApplicationEntry value;
    value.id = id;
    value.name = name;
    value.categories = categories;
    return value;
}

const CategoryNode *child(const CategoryNode &node, const QString &id)
{
    const auto match = std::find_if(node.children.cbegin(), node.children.cend(),
                                    [&id](const CategoryNode &candidate) {
                                        return candidate.id == id;
                                    });
    return match == node.children.cend() ? nullptr : &*match;
}

} // namespace

class ApplicationCatalogCategoryTreeTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void groupsEntriesIntoTheFixedLauncherGroups();
    void nestsRegisteredAdditionalCategoriesAsChildFolders();
    void prunesEmptyGroupsAndPlacesUnclassifiedEntriesInOther();
};

void ApplicationCatalogCategoryTreeTests::groupsEntriesIntoTheFixedLauncherGroups()
{
    const QVector<ApplicationEntry> applications{
        entry("org.qindaqt.editor", "Editor", {QStringLiteral("Development")}),
        entry("org.qindaqt.music", "Music", {QStringLiteral("AudioVideo"),
                                             QStringLiteral("Audio")}),
    };
    const auto tree = buildCategoryTree(applications);
    const auto *development = child(tree, QStringLiteral("Development"));
    const auto *audioVideo = child(tree, QStringLiteral("AudioVideo"));
    QVERIFY(development && audioVideo);
    QCOMPARE(development->entries.size(), 1);
    QCOMPARE(development->entries.constFirst().id, QStringLiteral("org.qindaqt.editor"));
    // The Audio additional category is not registered, so the entry sits
    // directly in its primary group.
    QCOMPARE(audioVideo->entries.size(), 1);
    QVERIFY(audioVideo->children.isEmpty());
}

void ApplicationCatalogCategoryTreeTests::nestsRegisteredAdditionalCategoriesAsChildFolders()
{
    const QVector<ApplicationEntry> applications{
        entry("some.game", "Some Game",
              {QStringLiteral("Game"), QStringLiteral("StrategyGame")}),
        entry("plain.game", "Plain Game", {QStringLiteral("Game")}),
    };
    const auto tree = buildCategoryTree(applications);
    const auto *games = child(tree, QStringLiteral("Games"));
    QVERIFY(games);
    QCOMPARE(games->entries.size(), 1);
    QCOMPARE(games->entries.constFirst().id, QStringLiteral("plain.game"));
    QCOMPARE(games->children.size(), 1);
    QCOMPARE(games->children.constFirst().id, QStringLiteral("StrategyGame"));
    QCOMPARE(games->children.constFirst().label, QStringLiteral("Strategy Game"));
    QCOMPARE(games->children.constFirst().entries.size(), 1);
    QCOMPARE(games->children.constFirst().entries.constFirst().id,
             QStringLiteral("some.game"));
}

void ApplicationCatalogCategoryTreeTests::prunesEmptyGroupsAndPlacesUnclassifiedEntriesInOther()
{
    const QVector<ApplicationEntry> applications{
        entry("mystery.tool", "Mystery Tool", {}),
        entry("kitchen.sink", "Kitchen Sink",
              {QStringLiteral("TotallyCustom")}),
    };
    const auto tree = buildCategoryTree(applications);
    // Only groups with content are emitted; both entries land in Other
    // because unrecognized or missing categories map there.
    QCOMPARE(tree.children.size(), 1);
    const auto &other = tree.children.constFirst();
    QCOMPARE(other.id, QStringLiteral("Other"));
    QCOMPARE(other.entries.size(), 2);
    // Input order is preserved inside the folder; the caller passes scan
    // output, which is already in catalog display order.
    QCOMPARE(other.entries.constFirst().id, QStringLiteral("mystery.tool"));
    QVERIFY(other.children.isEmpty());
}

QTEST_GUILESS_MAIN(ApplicationCatalogCategoryTreeTests)
#include "tst_category_tree.moc"
