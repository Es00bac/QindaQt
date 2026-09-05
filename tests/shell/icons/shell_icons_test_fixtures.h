// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixture builders for the shell-icons tests. Every tree is generated
// at test time beneath QINDAQT_SHELL_ICONS_FIXTURE_ROOT (the test binary's
// build directory), never under /tmp and never in the source tree.

#include <QDir>
#include <QFile>
#include <QImage>
#include <QString>

namespace ShellIconsTest
{

inline QString fixtureRoot()
{
    return QStringLiteral(QINDAQT_SHELL_ICONS_FIXTURE_ROOT);
}

inline bool recreateDir(const QString &path)
{
    QDir dir(path);
    if (dir.exists() && !dir.removeRecursively()) {
        return false;
    }
    return QDir().mkpath(path);
}

inline bool writeTextFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(content.toUtf8()) == qint64(content.toUtf8().size());
}

inline bool writePng(const QString &path, int side, const QColor &color)
{
    QImage image(side, side, QImage::Format_ARGB32);
    image.fill(color);
    return image.save(path, "png");
}

inline const char *kGreenCircleSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"><circle cx="12" cy="12" r="10" fill="#00ff00"/></svg>)svg";
inline const char *kBlackSquareSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"><rect width="24" height="24" fill="#000000"/></svg>)svg";
inline const char *kBlueSquareSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"><rect width="24" height="24" fill="#0000ff"/></svg>)svg";

// Writes the theme indexes (fixturetheme, fixtureparent, cycles, hicolor,
// and the hostile escape index) beneath the two icon roots.
inline bool writeLocatorIndexes(const QString &base)
{
    const QString icons1 = base + QStringLiteral("/icons1");
    const QString icons2 = base + QStringLiteral("/icons2");
    if (!writeTextFile(icons1 + QStringLiteral("/fixturetheme/index.theme"),
                       QStringLiteral(
                           "[Icon Theme]\n"
                           "Name=Fixture\n"
                           "Directories=16,32,48@2,scalable,thresh,evil\n"
                           "Inherits=fixtureparent\n"
                           "\n"
                           "[16]\nSize=16\nType=Fixed\n\n"
                           "[32]\nSize=32\nType=Fixed\n\n"
                           "[48@2]\nSize=48\nScale=2\nType=Fixed\n\n"
                           "[scalable]\nSize=48\nType=Scalable\nMinSize=24\nMaxSize=256\n\n"
                           "[thresh]\nSize=64\nType=Threshold\nThreshold=4\n\n"
                           "[evil]\nSize=32\nType=Fixed\n"))) {
        return false;
    }
    if (!writeTextFile(icons1 + QStringLiteral("/fixtureparent/index.theme"),
                       QStringLiteral("[Icon Theme]\nName=Parent\nDirectories=32\n\n"
                                      "[32]\nSize=32\nType=Fixed\n"))
        // Every root carries its own copy of the theme index per the spec.
        || !writeTextFile(icons2 + QStringLiteral("/fixturetheme/index.theme"),
                          QStringLiteral("[Icon Theme]\nName=Fixture2\nDirectories=32\n\n"
                                         "[32]\nSize=32\nType=Fixed\n"))) {
        return false;
    }
    if (!writeTextFile(icons1 + QStringLiteral("/fixturecycle-a/index.theme"),
                       QStringLiteral("[Icon Theme]\nName=CycleA\nDirectories=32\n"
                                      "Inherits=fixturecycle-b\n\n[32]\nSize=32\nType=Fixed\n"))
        || !writeTextFile(icons1 + QStringLiteral("/fixturecycle-b/index.theme"),
                          QStringLiteral("[Icon Theme]\nName=CycleB\nDirectories=32\n"
                                         "Inherits=fixturecycle-a\n\n[32]\nSize=32\nType=Fixed\n"))) {
        return false;
    }
    return writeTextFile(icons1 + QStringLiteral("/hicolor/index.theme"),
                         QStringLiteral("[Icon Theme]\nName=Fallback\nDirectories=32\n\n"
                                        "[32]\nSize=32\nType=Fixed\n"));
}

// Oversized and deep-chain hostile themes beneath the first root.
inline bool writeHostileThemes(const QString &base)
{
    const QString icons1 = base + QStringLiteral("/icons1");
    // Oversized index: valid-looking header followed by padding past the
    // 256 KiB ceiling; the theme must contribute no directories.
    QString oversized = QStringLiteral("[Icon Theme]\nName=Big\nDirectories=32\n\n"
                                       "[32]\nSize=32\nType=Fixed\n#");
    oversized += QString(300 * 1024, QLatin1Char('x'));
    if (!QDir().mkpath(icons1 + QStringLiteral("/bigtheme/32"))
        || !writeTextFile(icons1 + QStringLiteral("/bigtheme/index.theme"), oversized)
        || !writePng(icons1 + QStringLiteral("/bigtheme/32/big.png"), 32,
                     QColor(200, 0, 200))) {
        return false;
    }
    // Deep inheritance chain: chain0 -> chain1 -> ... -> chain19, with the
    // icon living in chain19 (beyond the depth cap) and chain03 (within it).
    for (int level = 0; level < 20; ++level) {
        const QString name = QStringLiteral("chain%1").arg(level, 2, 10, QLatin1Char('0'));
        const QString dir = icons1 + QLatin1Char('/') + name;
        if (!QDir().mkpath(dir + QStringLiteral("/32"))) {
            return false;
        }
        const QString parent = level < 19
            ? QStringLiteral("Inherits=chain%1\n").arg(level + 1, 2, 10, QLatin1Char('0'))
            : QString();
        if (!writeTextFile(dir + QStringLiteral("/index.theme"),
                           QStringLiteral("[Icon Theme]\nName=%1\nDirectories=32\n%2\n"
                                          "[32]\nSize=32\nType=Fixed\n")
                               .arg(name, parent))) {
            return false;
        }
    }
    return writePng(icons1 + QStringLiteral("/chain03/32/near-chain.png"), 32,
                    QColor(10, 200, 10))
        && writePng(icons1 + QStringLiteral("/chain19/32/deep-chain.png"), 32,
                    QColor(10, 100, 10));
}

// Icon payloads beneath both roots plus the outside escape target.
inline bool writeLocatorIcons(const QString &base)
{
    const QString icons1 = base + QStringLiteral("/icons1");
    const QString icons2 = base + QStringLiteral("/icons2");
    const QString outside = base + QStringLiteral("/outside");
    return writePng(icons1 + QStringLiteral("/fixturetheme/16/exact.png"), 16,
                    QColor(255, 0, 0))
        && writePng(icons1 + QStringLiteral("/fixturetheme/32/closest.png"), 32,
                    QColor(0, 255, 0))
        && writePng(icons1 + QStringLiteral("/fixturetheme/48@2/scaled.png"), 96,
                    QColor(0, 0, 255))
        && writeTextFile(icons1 + QStringLiteral("/fixturetheme/scalable/vector.svg"),
                         QString::fromUtf8(kGreenCircleSvg))
        && writePng(icons1 + QStringLiteral("/fixturetheme/thresh/near.png"), 64,
                    QColor(255, 255, 0))
        && writeTextFile(icons1 + QStringLiteral("/fixturetheme/32/sym.svg"),
                         QString::fromUtf8(kBlueSquareSvg))
        && writeTextFile(icons1 + QStringLiteral("/fixturetheme/32/sym-symbolic.svg"),
                         QString::fromUtf8(kBlackSquareSvg))
        && writePng(icons1 + QStringLiteral("/fixturetheme/32/dup.png"), 32,
                    QColor(1, 2, 3))
        && writePng(icons1 + QStringLiteral("/fixturetheme/32/both.png"), 32,
                    QColor(9, 9, 9))
        && writePng(icons1 + QStringLiteral("/fixtureparent/32/parent-only.png"), 32,
                    QColor(50, 50, 50))
        && writePng(icons1 + QStringLiteral("/fixturecycle-a/32/cyclic.png"), 32,
                    QColor(60, 60, 60))
        && writePng(icons1 + QStringLiteral("/hicolor/32/fallback.png"), 32,
                    QColor(70, 70, 70))
        && writePng(icons1 + QStringLiteral("/hicolor/32/both.png"), 32,
                    QColor(80, 80, 80))
        && writePng(icons1 + QStringLiteral("/standalone.png"), 32, QColor(90, 90, 90))
        && writePng(icons2 + QStringLiteral("/fixturetheme/32/dup.png"), 32,
                    QColor(4, 5, 6))
        && writePng(icons2 + QStringLiteral("/fixturetheme/32/second-root.png"), 32,
                    QColor(7, 8, 9))
        && writePng(outside + QStringLiteral("/leak/escape.png"), 32,
                    QColor(255, 0, 255));
}

// Builds the locator fixture set beneath `base`:
//   icons1/   first injected root (wins ties)
//   icons2/   second injected root
//   outside/  escape target that must never be read through the roots
inline bool buildLocatorFixtures(const QString &base)
{
    if (!recreateDir(base)) {
        return false;
    }
    const QString icons1 = base + QStringLiteral("/icons1");
    const QString icons2 = base + QStringLiteral("/icons2");
    const QString outside = base + QStringLiteral("/outside");
    if (!QDir().mkpath(icons1 + QStringLiteral("/fixturetheme/16"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturetheme/32"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturetheme/48@2"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturetheme/scalable"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturetheme/thresh"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixtureparent/32"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturecycle-a/32"))
        || !QDir().mkpath(icons1 + QStringLiteral("/fixturecycle-b/32"))
        || !QDir().mkpath(icons1 + QStringLiteral("/hicolor/32"))
        || !QDir().mkpath(icons1 + QStringLiteral("/escapetheme"))
        || !QDir().mkpath(icons2 + QStringLiteral("/fixturetheme/32"))
        || !QDir().mkpath(outside + QStringLiteral("/leak"))
        || !QDir().mkpath(icons1 + QStringLiteral("/escapetheme/32"))) {
        return false;
    }
    if (!writeLocatorIndexes(base) || !writeHostileThemes(base)
        || !writeLocatorIcons(base)) {
        return false;
    }

    // Symlink escape: inside the root, pointing at the outside tree. The
    // escape index is written after the link exists so declaration order is
    // fixed before any lookup.
    const QString linkPath = icons1 + QStringLiteral("/escapetheme/linkdir");
    if (!QFile::link(base + QStringLiteral("/outside/leak"), linkPath)) {
        return false;
    }
    return writeTextFile(icons1 + QStringLiteral("/escapetheme/index.theme"),
                         QStringLiteral(
                             "[Icon Theme]\nName=Escape\nDirectories=../outside/leak,linkdir,32\n\n"
                             "[../outside/leak]\nSize=32\nType=Fixed\n\n"
                             "[linkdir]\nSize=32\nType=Fixed\n\n"
                             "[32]\nSize=32\nType=Fixed\n"));
}

// Builds the resolver fixture set beneath `base`:
//   apps1/   first injected applications root (wins ties)
//   apps2/   second injected applications root
//   outside/ escape target
inline bool buildResolverFixtures(const QString &base)
{
    if (!recreateDir(base)) {
        return false;
    }
    const QString apps1 = base + QStringLiteral("/apps1");
    const QString apps2 = base + QStringLiteral("/apps2");
    const QString outside = base + QStringLiteral("/outside");
    if (!QDir().mkpath(apps1 + QStringLiteral("/nested/deep")) || !QDir().mkpath(apps2)
        || !QDir().mkpath(outside)) {
        return false;
    }

    const auto entry = [](const QString &name, const QString &icon) {
        return QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nIcon=%2\n")
            .arg(name, icon);
    };

    if (!writeTextFile(apps1 + QStringLiteral("/org.example.Foo.desktop"),
                       entry(QStringLiteral("Foo"), QStringLiteral("foo-icon")))
        || !writeTextFile(apps1 + QStringLiteral("/org.kde.Dolphin.desktop"),
                          entry(QStringLiteral("Dolphin"), QStringLiteral("dolphin-icon")))
        || !writeTextFile(apps1 + QStringLiteral("/nested/deep/org.example.Nested.desktop"),
                          entry(QStringLiteral("Nested"), QStringLiteral("nested-icon")))
        || !writeTextFile(apps1 + QStringLiteral("/hidden.desktop"),
                          entry(QStringLiteral("Hidden"), QStringLiteral("hidden-icon"))
                              + QStringLiteral("Hidden=true\n"))
        || !writeTextFile(apps1 + QStringLiteral("/nodisplay.desktop"),
                          entry(QStringLiteral("NoDisplay"), QStringLiteral("nodisplay-icon"))
                              + QStringLiteral("NoDisplay=true\n"))
        || !writeTextFile(apps1 + QStringLiteral("/broken.desktop"),
                          QStringLiteral("[Desktop Entry]\nType=Application\nIcon=broken-icon\n"))
        || !writeTextFile(apps1 + QStringLiteral("/notanapp.desktop"),
                          entry(QStringLiteral("Link"), QStringLiteral("link-icon"))
                              .replace(QStringLiteral("Type=Application"),
                                       QStringLiteral("Type=Link")))
        || !writeTextFile(apps1 + QStringLiteral("/absolute.desktop"),
                          entry(QStringLiteral("Absolute"), QStringLiteral("/etc/passwd")))
        || !writeTextFile(apps1 + QStringLiteral("/traversal.desktop"),
                          entry(QStringLiteral("Traversal"), QStringLiteral("../escape")))
        || !writeTextFile(apps2 + QStringLiteral("/org.example.Foo.desktop"),
                          entry(QStringLiteral("FooShadow"), QStringLiteral("shadow-icon")))
        || !writeTextFile(outside + QStringLiteral("/org.example.Outside.desktop"),
                          entry(QStringLiteral("Outside"), QStringLiteral("outside-icon")))) {
        return false;
    }

    // Oversized document: refused before parse.
    QString huge = entry(QStringLiteral("Huge"), QStringLiteral("huge-icon"));
    huge += QString(80 * 1024, QLatin1Char('#'));
    if (!writeTextFile(apps1 + QStringLiteral("/huge.desktop"), huge)) {
        return false;
    }
    // Symlink escaping the injected root must never be read.
    return QFile::link(outside + QStringLiteral("/org.example.Outside.desktop"),
                       apps1 + QStringLiteral("/escape.desktop"));
}

} // namespace ShellIconsTest
