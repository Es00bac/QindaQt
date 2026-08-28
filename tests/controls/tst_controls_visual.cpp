// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <cmath>

using QindaQt::Controls::TestSupport::pinDeterministicFonts;
using QindaQt::Controls::TestSupport::publishTheme;
using QindaQt::Controls::TestSupport::item;
using QindaQt::Controls::TestSupport::waitForMotion;

namespace {

struct ThemeRow final {
    const char *id;
    const char *file;
};

constexpr ThemeRow kThemes[] = {
    {"qinda-light", "qinda-light.json"},
    {"qinda-dusk", "qinda-dusk.json"},
    {"qinda-dark", "qinda-dark.json"},
    {"qinda-high-contrast", "qinda-high-contrast.json"},
    {"qinda-macos", "qinda-macos.json"},
};

int channelDistance(QRgb first, QRgb second)
{
    return std::max({std::abs(qRed(first) - qRed(second)),
                     std::abs(qGreen(first) - qGreen(second)),
                     std::abs(qBlue(first) - qBlue(second)),
                     std::abs(qAlpha(first) - qAlpha(second))});
}

QString scaleDirectory(double scale)
{
    return QString::number(qRound(scale * 100.0));
}

} // namespace

class ControlsVisualTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void matchesReviewedBaselines_data();
    void matchesReviewedBaselines();
};

void ControlsVisualTests::initTestCase()
{
    pinDeterministicFonts();
    QVERIFY2(QFontDatabase::families().contains(QStringLiteral("Noto Sans")),
             "visual fixture requires the named Noto Sans host family");
    QVERIFY2(QFontDatabase::families().contains(QStringLiteral("Noto Sans Mono")),
             "visual fixture requires the named Noto Sans Mono host family");
}

void ControlsVisualTests::matchesReviewedBaselines_data()
{
    QTest::addColumn<QString>("themeFile");
    QTest::addColumn<QString>("baselineName");
    QTest::addColumn<QSize>("logicalSize");

    const double scale = qEnvironmentVariable("QINDAQT_CONTROLS_TEST_SCALE", "1").toDouble();
    if (qFuzzyCompare(scale, 1.0)) {
        const struct {
            const char *name;
            QSize size;
        } widths[] = {{"compact", QSize(420, 840)},
                      {"ordinary", QSize(720, 840)},
                      {"large", QSize(1080, 840)}};
        for (const auto &theme : kThemes) {
            for (const auto &width : widths) {
                const QByteArray tag = QByteArray(theme.id) + '-' + width.name;
                QTest::newRow(tag.constData())
                    << QString::fromLatin1(theme.file)
                    << QString::fromLatin1(tag) << width.size;
            }
        }
        return;
    }

    for (const auto &theme : kThemes) {
        QTest::newRow(theme.id) << QString::fromLatin1(theme.file)
                                << QString::fromLatin1(theme.id) + QStringLiteral("-ordinary")
                                << QSize(720, 840);
    }
}

void ControlsVisualTests::matchesReviewedBaselines()
{
    QFETCH(QString, themeFile);
    QFETCH(QString, baselineName);
    QFETCH(QSize, logicalSize);

    bool scaleOk = false;
    const double requestedScale =
        qEnvironmentVariable("QINDAQT_CONTROLS_TEST_SCALE", "1").toDouble(&scaleOk);
    QVERIFY(scaleOk);

    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    QVERIFY2(publishTheme(*view.engine(), themeFile, {}, &error), qPrintable(error));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(logicalSize);
    view.setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_CONTROLS_TEST_QML_DIR "/ControlsGallery.qml")));
    QVERIFY2(view.errors().isEmpty(),
             view.errors().isEmpty() ? "" : qPrintable(view.errors().constFirst().toString()));
    QSignalSpy renderedFrames(&view, &QQuickWindow::afterRendering);
    QVERIFY(renderedFrames.isValid());
    view.show();
    QTRY_VERIFY_WITH_TIMEOUT(view.isExposed(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(view.size(), logicalSize, 3000);
    QVERIFY(std::abs(view.devicePixelRatio() - requestedScale) < 0.01);

    // AGENT-GUARD: The fixture intentionally exercises QST motion. Capturing
    // immediately after exposure records an intermediate Slider and can also
    // precede the software text node's final clip. Wait the published control
    // duration—not an arbitrary sleep—before asking for reviewed frames.
    auto *motionProbe = item(view.rootObject(), "galleryScaleSlider");
    const int transitionDuration = motionProbe->property("transitionDuration").toInt();
    QVERIFY(transitionDuration >= 0);
    QElapsedTimer motionBoundary;
    motionBoundary.start();
    waitForMotion(motionProbe);
    QVERIFY2(motionBoundary.elapsed() >= transitionDuration,
             "visual capture advanced before the published motion boundary");

    const auto verifyCardGeometry = [&](const char *cardName, bool requiresAction) {
        auto *card = item(view.rootObject(), cardName);
        auto *content = qobject_cast<QQuickItem *>(
            card->property("contentItem").value<QObject *>());
        auto *textColumn = item(card, "stateCardTextColumn");
        auto *title = item(card, "stateCardTitle");
        auto *message = item(card, "stateCardMessage");
        QVERIFY(content != nullptr);
        QVERIFY(card->width() >= 220.0);
        QVERIFY2(content->width() >= textColumn->width(),
                 qPrintable(QStringLiteral("%1 content=%2 text=%3 clip=%4")
                                .arg(QString::fromLatin1(cardName))
                                .arg(content->width())
                                .arg(textColumn->width())
                                .arg(content->clip())));
        QVERIFY(textColumn->width() >= 160.0);
        QVERIFY(title->width() >= 160.0);
        QVERIFY(message->width() >= 160.0);
        QVERIFY(title->property("lineCount").toInt() <= 2);
        QVERIFY(message->property("lineCount").toInt() <= 3);
        if (requiresAction) {
            auto *action = item(card, "stateCardAction");
            QVERIFY(action->isVisible());
            QVERIFY(action->width() >= 96.0);
        }
    };
    // AGENT-GUARD: Token publication and responsive layout can settle before
    // their text scene-graph nodes reach the first frame. Wait for two
    // explicitly requested renders so reviewed pixels never capture that
    // transient one-character wrapping state.
    for (int frame = 0; frame < 2; ++frame) {
        const qsizetype previousFrames = renderedFrames.size();
        view.requestUpdate();
        QTRY_VERIFY_WITH_TIMEOUT(renderedFrames.size() > previousFrames, 3000);
    }

    // AGENT-GUARD: Inspect geometry at the same settled render boundary used
    // for capture. Pre-frame geometry can be superseded by a later polish.
    verifyCardGeometry("galleryErrorStateCard", false);
    verifyCardGeometry("galleryDegradedNotice", true);

    QImage actual = view.grabWindow().convertToFormat(QImage::Format_RGBA8888);
    const QSize expectedPixels(qRound(logicalSize.width() * requestedScale),
                               qRound(logicalSize.height() * requestedScale));
    QCOMPARE(actual.size(), expectedPixels);

    const QString baselinePath =
        QStringLiteral(QINDAQT_CONTROLS_BASELINE_DIR "/")
        + scaleDirectory(requestedScale) + QLatin1Char('/') + baselineName
        + QStringLiteral(".png");
    if (qEnvironmentVariableIntValue("QINDAQT_UPDATE_CONTROLS_BASELINES") == 1) {
        QDir().mkpath(QFileInfo(baselinePath).absolutePath());
        QVERIFY2(actual.save(baselinePath), qPrintable(baselinePath));
        return;
    }

    const QImage expected(baselinePath);
    QVERIFY2(!expected.isNull(), qPrintable(QStringLiteral("missing baseline: ") + baselinePath));
    QCOMPARE(expected.size(), actual.size());
    const QImage normalized = expected.convertToFormat(QImage::Format_RGBA8888);

    qsizetype changed = 0;
    int maximumDistance = 0;
    for (int y = 0; y < actual.height(); ++y) {
        const auto *actualLine = reinterpret_cast<const QRgb *>(actual.constScanLine(y));
        const auto *expectedLine = reinterpret_cast<const QRgb *>(normalized.constScanLine(y));
        for (int x = 0; x < actual.width(); ++x) {
            const int distance = channelDistance(actualLine[x], expectedLine[x]);
            maximumDistance = std::max(maximumDistance, distance);
            if (distance > 2) {
                ++changed;
            }
        }
    }

    const qsizetype pixels = qsizetype(actual.width()) * qsizetype(actual.height());
    QVERIFY2(maximumDistance <= 8 && changed * 1000 <= pixels,
             qPrintable(QStringLiteral("baseline drift: %1 pixels, max channel delta %2")
                            .arg(changed)
                            .arg(maximumDistance)));
}

QTEST_MAIN(ControlsVisualTests)
#include "tst_controls_visual.moc"
