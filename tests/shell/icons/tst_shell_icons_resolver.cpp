// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/desktop_entry_icon_resolver.h>

#include <QtTest>

#include "shell_icons_test_fixtures.h"

using namespace QindaQt::Shell::Icons;

// DesktopEntryIconResolver over generated application roots: id mapping,
// precedence, hostile documents, and confinement. Negative controls fail on
// a tree without the rule (escaping symlinks would parse, hostile Icon=
// values would leak through).
class ShellIconsResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactDesktopId();
    void nestedEntryIdMapping();
    void firstRootWinsIdentity();
    void appIdNormalizations();
    void unknownIdsFailClosed();
    void hostileEntriesSkipped();
    void emptyAndMissingRoots();
    void repeatedScansAreDeterministic();
    void startupWmClassMatchesBeforeTail();
    void displayNamesFollowTheSameEntryMatching();
    void applicationDisplayNameFallsBackToPrettifiedIds();

private:
    QString m_apps1;
    QString m_apps2;
    std::unique_ptr<DesktopEntryIconResolver> m_resolver;
};

void ShellIconsResolverTest::initTestCase()
{
    const QString base = ShellIconsTest::fixtureRoot() + QStringLiteral("/resolver");
    QVERIFY2(ShellIconsTest::buildResolverFixtures(base), "resolver fixture tree");
    m_apps1 = base + QStringLiteral("/apps1");
    m_apps2 = base + QStringLiteral("/apps2");
    m_resolver =
        std::make_unique<DesktopEntryIconResolver>(QStringList { m_apps1, m_apps2 });
}

void ShellIconsResolverTest::exactDesktopId()
{
    QCOMPARE(m_resolver->iconNameForDesktopId(QStringLiteral("org.example.Foo")),
             QStringLiteral("foo-icon"));
    QCOMPARE(m_resolver->iconNameForDesktopId(QStringLiteral("org.kde.Dolphin")),
             QStringLiteral("dolphin-icon"));
}

void ShellIconsResolverTest::nestedEntryIdMapping()
{
    // nested/deep/org.example.Nested.desktop maps to the slash-to-dash id.
    QCOMPARE(m_resolver->iconNameForDesktopId(
                 QStringLiteral("nested-deep-org.example.Nested")),
             QStringLiteral("nested-icon"));
}

void ShellIconsResolverTest::firstRootWinsIdentity()
{
    // apps2 carries a shadowing org.example.Foo with a different icon.
    QCOMPARE(m_resolver->iconNameForDesktopId(QStringLiteral("org.example.Foo")),
             QStringLiteral("foo-icon"));
}

void ShellIconsResolverTest::appIdNormalizations()
{
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("org.example.Foo")),
             QStringLiteral("foo-icon"));
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("org.example.Foo.desktop")),
             QStringLiteral("foo-icon"));
    // Case-insensitive: compositor app ids are frequently lowercase.
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("org.kde.dolphin")),
             QStringLiteral("dolphin-icon"));
    // Reverse-DNS tail: bare final component.
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("dolphin")),
             QStringLiteral("dolphin-icon"));
}

void ShellIconsResolverTest::unknownIdsFailClosed()
{
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("org.example.Missing")).isEmpty());
    QVERIFY(m_resolver->iconNameForAppId(QString()).isEmpty());
    QVERIFY(m_resolver->iconNameForAppId(QStringLiteral("missing")).isEmpty());
    QVERIFY(m_resolver->iconNameForAppId(QString(300, QLatin1Char('x'))).isEmpty());
}

void ShellIconsResolverTest::hostileEntriesSkipped()
{
    // Hidden and NoDisplay entries contribute no icon.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("hidden")).isEmpty());
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("nodisplay")).isEmpty());
    // Missing Name fails the parser; non-Application Type is rejected.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("broken")).isEmpty());
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("notanapp")).isEmpty());
    // Icon= values outside the icon-name grammar are refused.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("absolute")).isEmpty());
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("traversal")).isEmpty());
    // Oversized documents are never parsed.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("huge")).isEmpty());
    // The symlink escaping the injected root is never opened.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("escape")).isEmpty());
    // The outside target itself stays invisible.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("org.example.Outside")).isEmpty());
}

void ShellIconsResolverTest::emptyAndMissingRoots()
{
    const DesktopEntryIconResolver empty(QStringList {});
    QCOMPARE(empty.entryCount(), 0);
    QVERIFY(empty.iconNameForAppId(QStringLiteral("org.example.Foo")).isEmpty());
    const DesktopEntryIconResolver missing(
        QStringList { ShellIconsTest::fixtureRoot() + QStringLiteral("/resolver/absent") });
    QCOMPARE(missing.entryCount(), 0);
}

void ShellIconsResolverTest::repeatedScansAreDeterministic()
{
    const DesktopEntryIconResolver again(QStringList { m_apps1, m_apps2 });
    QCOMPARE(again.entryCount(), m_resolver->entryCount());
    QCOMPARE(again.iconNameForAppId(QStringLiteral("dolphin")),
             m_resolver->iconNameForAppId(QStringLiteral("dolphin")));
    QCOMPARE(m_resolver->entryCount(), 4);
}

void ShellIconsResolverTest::startupWmClassMatchesBeforeTail()
{
    // X11 windows report WM_CLASS; the entry declares it as StartupWMClass.
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("LegacyClass")),
             QStringLiteral("legacy-icon"));
    QCOMPARE(m_resolver->iconNameForAppId(QStringLiteral("legacyclass")),
             QStringLiteral("legacy-icon"));
    QVERIFY(m_resolver->iconNameForAppId(QStringLiteral("OtherClass")).isEmpty());
}

void ShellIconsResolverTest::displayNamesFollowTheSameEntryMatching()
{
    QCOMPARE(m_resolver->displayNameForAppId(QStringLiteral("org.example.Foo")),
             QStringLiteral("Foo"));
    QCOMPARE(m_resolver->displayNameForAppId(QStringLiteral("org.kde.dolphin")),
             QStringLiteral("Dolphin"));
    QCOMPARE(m_resolver->displayNameForAppId(QStringLiteral("dolphin")),
             QStringLiteral("Dolphin"));
    QCOMPARE(m_resolver->displayNameForAppId(QStringLiteral("legacyclass")),
             QStringLiteral("Legacy Client"));
    // A refused Icon= value does not hide the entry's name.
    QVERIFY(m_resolver->iconNameForDesktopId(QStringLiteral("absolute")).isEmpty());
    QCOMPARE(m_resolver->displayNameForAppId(QStringLiteral("absolute")),
             QStringLiteral("Absolute"));
    // Hidden, NoDisplay, malformed, oversized, and escaping entries name nothing.
    for (const QString &id : {QStringLiteral("hidden"), QStringLiteral("nodisplay"),
                              QStringLiteral("broken"), QStringLiteral("huge"),
                              QStringLiteral("escape"), QStringLiteral("org.example.Outside")}) {
        QVERIFY2(m_resolver->displayNameForAppId(id).isEmpty(), qPrintable(id));
    }
}

void ShellIconsResolverTest::applicationDisplayNameFallsBackToPrettifiedIds()
{
    // Wayland reports the app id as the resource class too.
    QCOMPARE(m_resolver->applicationDisplayName(QStringLiteral("org.kde.dolphin"),
                                                QStringLiteral("org.kde.dolphin")),
             QStringLiteral("Dolphin"));
    // X11: the id misses, the reported WM_CLASS names the entry.
    QCOMPARE(m_resolver->applicationDisplayName(QStringLiteral("legacy-client-bin"),
                                                QStringLiteral("LegacyClass")),
             QStringLiteral("Legacy Client"));
    // No entry: the last reverse-DNS segment of the raw id.
    QCOMPARE(m_resolver->applicationDisplayName(QStringLiteral("org.qindaqt.Terminal"),
                                                QStringLiteral("org.qindaqt.Terminal")),
             QStringLiteral("Terminal"));
    QCOMPARE(m_resolver->applicationDisplayName(QStringLiteral("org.qindaqt.Terminal"),
                                                QString()),
             QStringLiteral("Terminal"));
    // A reported class that is not a reverse-DNS id is kept whole.
    QCOMPARE(m_resolver->applicationDisplayName(QStringLiteral("gimp-2.10"),
                                                QStringLiteral("Gimp-2.10")),
             QStringLiteral("Gimp-2.10"));
    QVERIFY(m_resolver->applicationDisplayName(QString(), QString()).isEmpty());

    QCOMPARE(DesktopEntryIconResolver::prettifiedApplicationId(QStringLiteral("firefox")),
             QStringLiteral("Firefox"));
    QCOMPARE(DesktopEntryIconResolver::prettifiedApplicationId(
                 QStringLiteral("org.example.Foo.desktop")),
             QStringLiteral("Foo"));
    QCOMPARE(DesktopEntryIconResolver::prettifiedApplicationId(QStringLiteral("App org.x.Y")),
             QStringLiteral("App org.x.Y"));
}

QTEST_GUILESS_MAIN(ShellIconsResolverTest)
#include "tst_shell_icons_resolver.moc"
