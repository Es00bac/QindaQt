// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/icon_theme_limits.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QDir>
#include <QtTest>

#include "shell_icons_test_fixtures.h"

using namespace QindaQt::Shell::Icons;

// Hostile and spec-rule coverage for IconThemeLocator over generated fixture
// roots. Every negative control here fails on a tree without the rule it
// proves (escaping symlinks would resolve, hostile names would pass through,
// oversized indexes would parse).
class ShellIconsLocatorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactFixedSizeMatch();
    void closestThresholdMatch();
    void scalableRangeMatch();
    void scaleAwareDirectory();
    void inheritsParentFallback();
    void inheritanceCycleGuarded();
    void inheritanceDepthBounded();
    void hicolorIsLastResort();
    void hicolorOnlyFallback();
    void hicolorStaysLastWhenChainCapFull();
    void rootOrderIsDeterministic();
    void symbolicVariantPreferred();
    void symbolicFallsBackToPlainName();
    void unthemedRootFallback();
    void hostileNamesRefused();
    void hostileNamesRefused_data();
    void escapingDirectoryEntriesRefused();
    void symlinkEscapeRefused();
    void oversizedIndexIgnored();
    void lookupsAreDeterministic();
    void cacheBoundKeepsLookupCorrect();

private:
    QString m_icons1;
    QString m_icons2;
    std::unique_ptr<IconThemeLocator> m_locator;
};

void ShellIconsLocatorTest::initTestCase()
{
    const QString base = ShellIconsTest::fixtureRoot() + QStringLiteral("/locator");
    QVERIFY2(ShellIconsTest::buildLocatorFixtures(base), "locator fixture tree");
    m_icons1 = base + QStringLiteral("/icons1");
    m_icons2 = base + QStringLiteral("/icons2");
    m_locator = std::make_unique<IconThemeLocator>(QStringList { m_icons1, m_icons2 },
                                                   QStringList { QStringLiteral("fixturetheme") });
}

void ShellIconsLocatorTest::exactFixedSizeMatch()
{
    QCOMPARE(m_locator->locate(QStringLiteral("exact"), 16),
             m_icons1 + QStringLiteral("/fixturetheme/16/exact.png"));
    // A 16px request for an icon that only exists at 32px resolves to the
    // 32px file through the closest-size rule, not to a fabricated path.
    QCOMPARE(m_locator->locate(QStringLiteral("closest"), 16),
             m_icons1 + QStringLiteral("/fixturetheme/32/closest.png"));
}

void ShellIconsLocatorTest::closestThresholdMatch()
{
    // Size 63 is inside the 64 ± 4 threshold window: exact threshold match.
    QCOMPARE(m_locator->locate(QStringLiteral("near"), 63),
             m_icons1 + QStringLiteral("/fixturetheme/thresh/near.png"));
    // Size 50 is outside the threshold window but still closest to 64.
    QCOMPARE(m_locator->locate(QStringLiteral("near"), 50),
             m_icons1 + QStringLiteral("/fixturetheme/thresh/near.png"));
    // The scalable directory (24..256) is a closer match for size 30 than
    // the threshold directory at 64, and carries no "near" file, so a name
    // present only in thresh still resolves from thresh.
    QCOMPARE(m_locator->locate(QStringLiteral("near"), 30),
             m_icons1 + QStringLiteral("/fixturetheme/thresh/near.png"));
}

void ShellIconsLocatorTest::scalableRangeMatch()
{
    const QString vector = m_locator->locate(QStringLiteral("vector"), 100);
    QCOMPARE(vector, m_icons1 + QStringLiteral("/fixturetheme/scalable/vector.svg"));
    // Scalable exists only as SVG: the extension order is exercised by the
    // PNG-first fixtures elsewhere.
    QCOMPARE(m_locator->locate(QStringLiteral("vector"), 1000),
             m_icons1 + QStringLiteral("/fixturetheme/scalable/vector.svg"));
}

void ShellIconsLocatorTest::scaleAwareDirectory()
{
    QCOMPARE(m_locator->locate(QStringLiteral("scaled"), 48, 2.0),
             m_icons1 + QStringLiteral("/fixturetheme/48@2/scaled.png"));
    // At scale 1 the 48@2 directory does not match; closest-size still finds
    // it when nothing else carries the name, at the correct device distance.
    QCOMPARE(m_locator->locate(QStringLiteral("scaled"), 48, 1.0),
             m_icons1 + QStringLiteral("/fixturetheme/48@2/scaled.png"));
}

void ShellIconsLocatorTest::inheritsParentFallback()
{
    QCOMPARE(m_locator->locate(QStringLiteral("parent-only"), 32),
             m_icons1 + QStringLiteral("/fixtureparent/32/parent-only.png"));
}

void ShellIconsLocatorTest::inheritanceCycleGuarded()
{
    IconThemeLocator cyclic(QStringList { m_icons1 },
                            QStringList { QStringLiteral("fixturecycle-a") });
    // A <-> B cycle: the icon in A resolves and the lookup terminates.
    QCOMPARE(cyclic.locate(QStringLiteral("cyclic"), 32),
             m_icons1 + QStringLiteral("/fixturecycle-a/32/cyclic.png"));
}

void ShellIconsLocatorTest::inheritanceDepthBounded()
{
    IconThemeLocator chained(QStringList { m_icons1 },
                             QStringList { QStringLiteral("chain00") });
    QCOMPARE(chained.locate(QStringLiteral("near-chain"), 32),
             m_icons1 + QStringLiteral("/chain03/32/near-chain.png"));
    // chain19 lies beyond the depth cap: fail closed, no resolution.
    QVERIFY(chained.locate(QStringLiteral("deep-chain"), 32).isEmpty());
}

void ShellIconsLocatorTest::hicolorIsLastResort()
{
    // Present in both fixturetheme and hicolor at the same size: the themed
    // file wins even though hicolor is always in the chain.
    QCOMPARE(m_locator->locate(QStringLiteral("both"), 32),
             m_icons1 + QStringLiteral("/fixturetheme/32/both.png"));
}

void ShellIconsLocatorTest::hicolorOnlyFallback()
{
    QCOMPARE(m_locator->locate(QStringLiteral("fallback"), 32),
             m_icons1 + QStringLiteral("/hicolor/32/fallback.png"));
    // hicolor works even when it is the only theme named by the caller.
    IconThemeLocator hicolorOnly(QStringList { m_icons1 }, QStringList {});
    QCOMPARE(hicolorOnly.locate(QStringLiteral("fallback"), 32),
             m_icons1 + QStringLiteral("/hicolor/32/fallback.png"));
}

void ShellIconsLocatorTest::hicolorStaysLastWhenChainCapFull()
{
    // Injecting more distinct theme names than the chain cap must not push
    // the specification-mandated hicolor fallback out of the chain: the cap
    // holds and hicolor stays last. Fails on a tree that drops hicolor when
    // the cap is reached.
    QStringList themes;
    for (int i = 0; i < kMaxThemeChainLength + 4; ++i) {
        themes.append(QStringLiteral("flooded-%1").arg(i));
    }
    IconThemeLocator flooded(QStringList { m_icons1 }, themes);
    QCOMPARE(flooded.locate(QStringLiteral("fallback"), 32),
             m_icons1 + QStringLiteral("/hicolor/32/fallback.png"));
}

void ShellIconsLocatorTest::rootOrderIsDeterministic()
{
    QCOMPARE(m_locator->locate(QStringLiteral("dup"), 32),
             m_icons1 + QStringLiteral("/fixturetheme/32/dup.png"));
    QCOMPARE(m_locator->locate(QStringLiteral("second-root"), 32),
             m_icons2 + QStringLiteral("/fixturetheme/32/second-root.png"));
}

void ShellIconsLocatorTest::symbolicVariantPreferred()
{
    QCOMPARE(m_locator->locate(QStringLiteral("sym"), 32, 1.0, true),
             m_icons1 + QStringLiteral("/fixturetheme/32/sym-symbolic.svg"));
}

void ShellIconsLocatorTest::symbolicFallsBackToPlainName()
{
    QCOMPARE(m_locator->locate(QStringLiteral("sym"), 32, 1.0, false),
             m_icons1 + QStringLiteral("/fixturetheme/32/sym.svg"));
    // Requesting symbolic for a name with no symbolic variant still resolves.
    QCOMPARE(m_locator->locate(QStringLiteral("exact"), 16, 1.0, true),
             m_icons1 + QStringLiteral("/fixturetheme/16/exact.png"));
}

void ShellIconsLocatorTest::unthemedRootFallback()
{
    QCOMPARE(m_locator->locate(QStringLiteral("standalone"), 32),
             m_icons1 + QStringLiteral("/standalone.png"));
}

void ShellIconsLocatorTest::hostileNamesRefused()
{
    QFETCH(QString, name);
    QVERIFY2(m_locator->locate(name, 32).isEmpty(), qPrintable(name));
    QVERIFY2(!IconThemeLocator::isAcceptableIconName(name), qPrintable(name));
}

void ShellIconsLocatorTest::hostileNamesRefused_data()
{
    QTest::addColumn<QString>("name");
    QTest::newRow("parent-traversal") << QStringLiteral("../outside/leak/escape");
    QTest::newRow("embedded-parent") << QStringLiteral("foo/../bar");
    QTest::newRow("absolute-path") << QStringLiteral("/etc/passwd");
    QTest::newRow("forward-slash") << QStringLiteral("some/name");
    QTest::newRow("backslash") << QStringLiteral("some\\name");
    QTest::newRow("nul-byte") << QString::fromUtf8("evil\0name", 9);
    QTest::newRow("space") << QStringLiteral("two words");
    QTest::newRow("empty") << QString();
    QTest::newRow("huge") << QString(200, QLatin1Char('a'));
}

void ShellIconsLocatorTest::escapingDirectoryEntriesRefused()
{
    IconThemeLocator escape(QStringList { m_icons1 },
                            QStringList { QStringLiteral("escapetheme") });
    // The index declares ../outside; the file exists there but must never
    // resolve through the confined root.
    QVERIFY(escape.locate(QStringLiteral("escape"), 32).isEmpty());
}

void ShellIconsLocatorTest::symlinkEscapeRefused()
{
    IconThemeLocator escape(QStringList { m_icons1 },
                            QStringList { QStringLiteral("escapetheme") });
    // linkdir is a symlink to outside/leak, which holds escape.png.
    QVERIFY(escape.locate(QStringLiteral("escape"), 32).isEmpty());
    // The unthemed fallback must not rescue the name through the link either.
    QVERIFY(m_locator->locate(QStringLiteral("escape"), 32).isEmpty());
}

void ShellIconsLocatorTest::oversizedIndexIgnored()
{
    IconThemeLocator big(QStringList { m_icons1 },
                         QStringList { QStringLiteral("bigtheme") });
    // The index exceeds the byte ceiling, so bigtheme contributes no
    // directories and the icon fails closed to unresolved.
    QVERIFY(big.locate(QStringLiteral("big"), 32).isEmpty());
}

void ShellIconsLocatorTest::lookupsAreDeterministic()
{
    const QString first = m_locator->locate(QStringLiteral("closest"), 20);
    const QString second = m_locator->locate(QStringLiteral("closest"), 20);
    QVERIFY(!first.isEmpty());
    QCOMPARE(first, second);
}

void ShellIconsLocatorTest::cacheBoundKeepsLookupCorrect()
{
    // One locator whose chain (16 themes, capped) times ten roots exceeds the
    // parsed-index cache bound: eviction must never change lookup results.
    QStringList roots { m_icons1, m_icons2 };
    for (int i = 0; i < 8; ++i) {
        roots.append(m_icons1 + QStringLiteral("/absent-root-%1").arg(i));
    }
    IconThemeLocator wide(roots,
                          {QStringLiteral("fixturetheme"), QStringLiteral("chain00"),
                           QStringLiteral("escapetheme"), QStringLiteral("fixturecycle-a")});
    const QString before = wide.locate(QStringLiteral("exact"), 16);
    QVERIFY(!before.isEmpty());
    Q_UNUSED(wide.locate(QStringLiteral("cyclic"), 32));
    Q_UNUSED(wide.locate(QStringLiteral("fallback"), 32));
    QCOMPARE(wide.locate(QStringLiteral("exact"), 16), before);
    QCOMPARE(m_locator->locate(QStringLiteral("exact"), 16),
             m_icons1 + QStringLiteral("/fixturetheme/16/exact.png"));
}

QTEST_GUILESS_MAIN(ShellIconsLocatorTest)
#include "tst_shell_icons_locator.moc"
