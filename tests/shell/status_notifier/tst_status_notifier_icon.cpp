// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QtTest>

using namespace QindaQt::StatusNotifier;

namespace
{

// Fixture roots live beside the test executable in the build tree; the icon
// module itself never writes anywhere, and tests only write inside the build
// directory, never /tmp.
QString fixtureRoot(const QString &name)
{
    const QString path =
        QDir::currentPath() + QStringLiteral("/icon-fixtures-") + name;
    QDir().mkpath(path);
    return path;
}

bool writeImage(const QString &directory, const QString &name, const QSize &size,
                const QColor &color, QString *error)
{
    if (!QDir().mkpath(directory)) {
        *error = QStringLiteral("cannot create %1").arg(directory);
        return false;
    }
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    if (!image.save(directory + QLatin1Char('/') + name + QStringLiteral(".png"))) {
        *error = QStringLiteral("cannot save %1/%2.png").arg(directory, name);
        return false;
    }
    return true;
}

bool writeFile(const QString &path, const QByteArray &content, QString *error)
{
    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        *error = QStringLiteral("cannot create %1").arg(directory);
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("cannot open %1").arg(path);
        return false;
    }
    if (file.write(content) != qsizetype(content.size())) {
        *error = QStringLiteral("short write to %1").arg(path);
        return false;
    }
    return true;
}

QByteArray makeArgb(qsizetype width, qsizetype height, char fill)
{
    return QByteArray(width * height * 4, fill);
}

bool materializeTheme(const QString &root, QString *error)
{
    return writeFile(root + QStringLiteral("/index.theme"),
                     "[Icon Theme]\nName=QindaQtTest\nDirectories=32x32/apps,48x48/apps\n\n"
                     "[32x32/apps]\nSize=32\n\n[48x48/apps]\nSize=48\n",
                     error)
        && writeImage(root + QStringLiteral("/32x32/apps"), QStringLiteral("test-icon"),
                      QSize(32, 32), QColor(255, 0, 0), error)
        && writeImage(root + QStringLiteral("/48x48/apps"), QStringLiteral("test-icon"),
                      QSize(48, 48), QColor(0, 255, 0), error);
}

} // namespace

class StatusNotifierIconTests final : public QObject
{
    Q_OBJECT

private slots:
    void decodesArgb32Pixmaps();
    void decodeRejectsByteAndDimensionPoison();
    void renderPrefersNearestWirePixmap();
    void renderResolvesThemedIconsByNearestSize();
    void renderFallsBackToHicolor();
    void renderUsesDirectRootHitAsLastResort();
    void renderFallbackIsDeterministicAndRefusesTraversal();
    void renderIgnoresUndecodableThemeFiles();
    void locatorIgnoresOversizedIndexFiles();
};

void StatusNotifierIconTests::decodesArgb32Pixmaps()
{
    // Opaque gray: bytes are B,G,R,A so this is qRgba(0x7F, 0x7F, 0x7F, 0xFF).
    Pixmap pixmap;
    pixmap.width = 2;
    pixmap.height = 2;
    const QByteArray pixel = QByteArray(3, char(0x7F)) + char(0xFF);
    pixmap.argb = pixel.repeated(4);

    const QImage decoded = StatusNotifierIconRenderer::decodePixmap(pixmap);
    QVERIFY(!decoded.isNull());
    QCOMPARE(decoded.size(), QSize(2, 2));
    QCOMPARE(decoded.format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(decoded.pixelColor(0, 0), QColor(127, 127, 127, 255));

    // The decoded image owns its bytes: mutating the source must not alias.
    QImage copy = decoded;
    pixmap.argb[0] = 0;
    QCOMPARE(copy.pixelColor(0, 0), QColor(127, 127, 127, 255));
}

void StatusNotifierIconTests::decodeRejectsByteAndDimensionPoison()
{
    Pixmap wrongBytes;
    wrongBytes.width = 2;
    wrongBytes.height = 2;
    wrongBytes.argb = makeArgb(2, 1, char(0x7F));
    QVERIFY(StatusNotifierIconRenderer::decodePixmap(wrongBytes).isNull());

    Pixmap zero;
    zero.width = 0;
    zero.height = 4;
    zero.argb.clear();
    QVERIFY(StatusNotifierIconRenderer::decodePixmap(zero).isNull());

    // One dimension beyond the shared 512 bound decodes to null even with
    // exact byte accounting.
    Pixmap oversized;
    oversized.width = kMaxIconPixmapDimension + 1;
    oversized.height = 1;
    oversized.argb = makeArgb(oversized.width, 1, char(0x7F));
    QVERIFY(StatusNotifierIconRenderer::decodePixmap(oversized).isNull());

    // A boundary-valid 512x512 pixmap must decode.
    Pixmap boundary;
    boundary.width = kMaxIconPixmapDimension;
    boundary.height = kMaxIconPixmapDimension;
    boundary.argb = makeArgb(boundary.width, boundary.height, char(0x11));
    QCOMPARE(StatusNotifierIconRenderer::decodePixmap(boundary).size(),
             QSize(int(kMaxIconPixmapDimension), int(kMaxIconPixmapDimension)));
}

void StatusNotifierIconTests::renderPrefersNearestWirePixmap()
{
    IconPayload icon;
    icon.iconName = QStringLiteral("whatever-missing");
    Pixmap far;
    far.width = 64;
    far.height = 64;
    far.argb = makeArgb(64, 64, char(0x22));
    Pixmap near_;
    near_.width = 8;
    near_.height = 8;
    near_.argb = makeArgb(8, 8, char(0x22));
    icon.pixmaps = {far, near_};

    // Empty roots: even the theme miss must end at the deterministic
    // fallback, never null.
    StatusNotifierIconRenderer renderer({});
    const QImage rendered = renderer.render(icon, 16);
    QVERIFY(!rendered.isNull());
    QCOMPARE(rendered.size(), QSize(8, 8));
}

void StatusNotifierIconTests::renderResolvesThemedIconsByNearestSize()
{
    QString error;
    const QString root = fixtureRoot(QStringLiteral("theme"));
    QVERIFY2(materializeTheme(root, &error), qPrintable(error));

    StatusNotifierIconRenderer renderer({root});
    IconPayload icon;
    icon.iconName = QStringLiteral("test-icon");

    QCOMPARE(renderer.render(icon, 44).size(), QSize(48, 48));
    QCOMPARE(renderer.render(icon, 36).size(), QSize(32, 32));
    // Pixmap presence overrides theme resolution.
    Pixmap wire;
    wire.width = 22;
    wire.height = 22;
    wire.argb = makeArgb(22, 22, char(0x33));
    icon.pixmaps = {wire};
    QCOMPARE(renderer.render(icon, 40).size(), QSize(22, 22));
}

void StatusNotifierIconTests::renderFallsBackToHicolor()
{
    QString error;
    const QString root = fixtureRoot(QStringLiteral("hicolor"));
    QVERIFY2(writeFile(root + QStringLiteral("/hicolor/index.theme"),
                       "[Icon Theme]\nName=Hicolor\nDirectories=16x16/apps\n\n"
                       "[16x16/apps]\nSize=16\n",
                       &error)
                 && writeImage(root + QStringLiteral("/hicolor/16x16/apps"),
                               QStringLiteral("hicolor-only"), QSize(16, 16),
                               QColor(0, 0, 255), &error),
             qPrintable(error));

    StatusNotifierIconRenderer renderer({root});
    IconPayload icon;
    icon.iconName = QStringLiteral("hicolor-only");
    QCOMPARE(renderer.render(icon, 16).size(), QSize(16, 16));
}

void StatusNotifierIconTests::renderUsesDirectRootHitAsLastResort()
{
    QString error;
    const QString root = fixtureRoot(QStringLiteral("direct"));
    QVERIFY2(writeImage(root, QStringLiteral("direct-icon"), QSize(10, 10),
                        QColor(9, 9, 9), &error),
             qPrintable(error));

    StatusNotifierIconRenderer renderer({root});
    IconPayload icon;
    icon.iconName = QStringLiteral("direct-icon");
    QCOMPARE(renderer.render(icon, 48).size(), QSize(10, 10));
}

void StatusNotifierIconTests::renderFallbackIsDeterministicAndRefusesTraversal()
{
    StatusNotifierIconRenderer renderer({});
    IconPayload missing;
    missing.iconName = QStringLiteral("no-such-icon-anywhere");

    const QImage first = renderer.render(missing, 24);
    const QImage second = renderer.render(missing, 24);
    QVERIFY(!first.isNull());
    QCOMPARE(first.size(), QSize(24, 24));
    QCOMPARE(first, second); // Deterministic fallback bytes.

    // Traversal and path-style names are refused and render the fallback.
    IconPayload traversal;
    traversal.iconName = QStringLiteral("../outside");
    QCOMPARE(renderer.render(traversal, 24), first);
    IconPayload absolute;
    absolute.iconName = QStringLiteral("/etc/passwd");
    QCOMPARE(renderer.render(absolute, 24), first);
}

void StatusNotifierIconTests::renderIgnoresUndecodableThemeFiles()
{
    QString error;
    const QString root = fixtureRoot(QStringLiteral("garbage"));
    QVERIFY2(writeFile(root + QStringLiteral("/index.theme"),
                       "[Icon Theme]\nName=Broken\nDirectories=broken/apps\n\n"
                       "[broken/apps]\nSize=32\n",
                       &error)
                 && writeFile(root + QStringLiteral("/broken/apps/garbage-icon.png"),
                              "this is not a png at all", &error),
             qPrintable(error));

    StatusNotifierIconRenderer renderer({root});
    IconPayload icon;
    icon.iconName = QStringLiteral("garbage-icon");
    const QImage rendered = renderer.render(icon, 32);
    QCOMPARE(rendered, StatusNotifierIconRenderer::fallbackIcon(32));
}

void StatusNotifierIconTests::locatorIgnoresOversizedIndexFiles()
{
    QString error;
    const QString root = fixtureRoot(QStringLiteral("fat-index"));
    // An index file beyond the shared budget contributes no directories; the
    // lookup fails closed instead of parsing a truncated theme.
    QVERIFY2(writeFile(root + QStringLiteral("/index.theme"),
                       QByteArray(kMaxIconThemeIndexBytes + 1, 'x'), &error),
             qPrintable(error));

    StatusNotifierIconRenderer renderer({root});
    IconPayload icon;
    icon.iconName = QStringLiteral("anything");
    QCOMPARE(renderer.render(icon, 32), StatusNotifierIconRenderer::fallbackIcon(32));
}

QTEST_GUILESS_MAIN(StatusNotifierIconTests)
#include "tst_status_notifier_icon.moc"
