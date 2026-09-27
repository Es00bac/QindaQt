// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_window_preview.h"
#include "qindaqt/decoration_painter/decoration_shadow.h"
#include <QImage>
#include <QPainter>
#include <QtTest>
using namespace QindaQt::Apps::SettingsAppearance;
using namespace QindaQt::Decoration;
namespace {
DecorationChrome chromeFor(const QString &buttons, qreal radius, qreal opacity = 0.4)
{
    DecorationChrome chrome;
    chrome.buttonStyle = buttons;
    chrome.appIcon = false;
    chrome.titleWorn = false;
    chrome.titleBar = QColor("#e6b440");
    chrome.surface = QColor("#dddddd");
    chrome.surfaceRaised = QColor("#dddddd");
    chrome.border = QColor("#333333");
    chrome.text = QColor("#111111");
    chrome.textMuted = QColor("#333333");
    chrome.cornerRadius = radius;
    chrome.shadowExtent = 12;
    chrome.shadowOpacity = opacity;
    return chrome;
}
QImage render(const DecorationChrome &chrome, qreal scale = 1.0,
              const QString &caption = QStringLiteral("Short"))
{
    AppearanceWindowPreview item;
    item.setWidth(640);
    item.setHeight(400);
    item.setShowInactiveWindow(false);
    item.setCaption(caption);
    item.setChrome(chrome.toVariantMap());
    item.setCanvas(QColor("#ee88ee"));
    item.setToolkitPalette({{QStringLiteral("window"), QColor("#dddddd")}});
    QImage pixels(QSize(qRound(640 * scale), qRound(400 * scale)), QImage::Format_ARGB32_Premultiplied);
    pixels.setDevicePixelRatio(scale);
    pixels.fill(Qt::transparent);
    QPainter painter(&pixels);
    item.paint(&painter);
    return pixels;
}
QColor sample(const QImage &image, int x, int y) {
    return image.pixelColor(qRound(x * image.devicePixelRatio()), qRound(y * image.devicePixelRatio()));
}
}
class AppearanceWindowSilhouetteTest final : public QObject {
    Q_OBJECT
private slots:
    void cornerCutoutAndShadow_data() {
        QTest::addColumn<qreal>("radius");
        QTest::addColumn<qreal>("scale");
        QTest::newRow("square") << qreal(0) << qreal(1);
        QTest::newRow("rounded") << qreal(10) << qreal(1);
        QTest::newRow("rounded-hidpi") << qreal(10) << qreal(2);
    }
    void cornerCutoutAndShadow() {
        QFETCH(qreal, radius);
        QFETCH(qreal, scale);
        const auto chrome = chromeFor("tab", radius);
        const auto pixels = render(chrome, scale);
        const QColor canvas("#ee88ee");
        // Front frame: x12,y96,width486.4. Well beyond the short caption:
        // no opaque client backing, no phantom shadow above the empty bar.
        QCOMPARE(sample(pixels, 430, 99), canvas);
        QCOMPARE(sample(pixels, 430, 93), canvas);
        // Real body shoulder and the tab's top each cast a shadow.
        const int title = qRound(decorationTitleHeight(chrome));
        QVERIFY(sample(pixels, 430, 96 + title - 3) != canvas);
        QVERIFY(sample(pixels, 65, 93) != canvas);
        // Rounded title paint must stop at the actual client seam.
        QCOMPARE(sample(pixels, 65, 96 + title + 3), QColor("#dddddd"));
        // Turning shadow off removes shoulder shadow, not the actual title.
        auto flat = chrome;
        flat.shadowOpacity = 0;
        const auto unshadowed = render(flat, scale);
        QCOMPARE(sample(unshadowed, 430, 96 + title - 3), canvas);
        QVERIFY(sample(unshadowed, 65, 99) != canvas);
    }
    void ordinaryTitleAndAuthoredOpacity() {
        auto chrome = chromeFor("symbols", 10);
        const auto pixels = render(chrome);
        QVERIFY(sample(pixels, 430, 99) != QColor("#ee88ee"));
        QCOMPARE(sample(pixels, 350, 96 + qRound(decorationTitleHeight(chrome)) + 3),
                 QColor("#dddddd"));
        chrome.titleOpacity = 0;
        chrome.titleHighlight = false;
        chrome.titleTint = QColor();
        // An invisible material must reveal the canvas, not the client fill.
        QCOMPARE(sample(render(chrome, 1.0, QString{}), 350, 100), QColor("#ee88ee"));
    }
};
QTEST_MAIN(AppearanceWindowSilhouetteTest)
#include "tst_appearance_window_silhouette.moc"
