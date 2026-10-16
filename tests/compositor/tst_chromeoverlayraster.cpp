// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeoverlayraster.h"
#include "chromeplancomparison.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/hybrid_chrome/chromesplitdeck.h"

#include <QPainter>
#include <QtMath>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;
using HybridChrome::ChromeRenderPlan;

namespace {

struct PlanSpec final
{
    QSizeF size = QSizeF(2560.0, 1408.0);
    qreal ratio = 1.0;
    bool focused = true;
    bool shaded = false;
    HybridChrome::ChromeStyle style =
        HybridChrome::ChromeStyle::standard(HybridChrome::ButtonSide::Right);
    QStringList tabTitles{QStringLiteral("⠋ Claude Code"), QStringLiteral("notes.txt")};
};

// A maximized two-page container (the live case: a terminal whose spinner
// title animates) laid out by the real chrome layout engine, at the origin
// like every localized overlay plan.
ChromeRenderPlan makePlan(const PlanSpec &spec)
{
    HybridChrome::ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-q");
    request.outerRect = QRectF(QPointF(0.0, 0.0), spec.size);
    request.devicePixelRatio = spec.ratio;
    request.containerFocused = spec.focused;
    request.shaded = spec.shaded;
    request.containerTitle = QStringLiteral("Container");
    request.containerTitleIsGenerated = true;
    request.style = spec.style;
    for (qsizetype index = 0; index < spec.tabTitles.size(); ++index) {
        request.tabs.append({QStringLiteral("page-%1").arg(index), spec.tabTitles[index],
                             index == 0});
    }
    if (spec.shaded) {
        request.badgeLabelText = request.containerTitle;
        request.badgeLabelWidth = 80.0;
    } else {
        const auto &metrics = request.metrics;
        const QRectF content(metrics.outerBorder, metrics.outerBorder + metrics.titleBarHeight,
                             spec.size.width() - 2.0 * metrics.outerBorder,
                             spec.size.height() - 2.0 * metrics.outerBorder
                                 - metrics.titleBarHeight);
        request.members = {{QStringLiteral("window-term"), spec.tabTitles.value(0), content,
                            spec.focused}};
    }
    QString error;
    auto plan = HybridChrome::ChromeLayoutEngine::build(request, &error);
    if (!plan) {
        qFatal("chrome layout failed: %s", qPrintable(error));
    }
    return std::move(*plan);
}

QImage fullRender(const ChromeRenderPlan &plan, const HybridChrome::ChromePaintState &state)
{
    const QSize physical(qCeil(plan.outerFrame.width() * plan.devicePixelRatio),
                         qCeil(plan.outerFrame.height() * plan.devicePixelRatio));
    QImage image(physical, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(plan.devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    HybridChrome::ChromeRenderer::paint(painter, plan, state);
    return image;
}

// What KWin shows: the band item stacked at the body's origin.
QImage stacked(const ChromeOverlayRaster &raster)
{
    QImage result = raster.body().copy();
    const QImage &band = raster.band();
    for (int row = 0; row < band.height(); ++row) {
        std::memcpy(result.scanLine(row), band.constScanLine(row),
                    static_cast<size_t>(band.bytesPerLine()));
    }
    return result;
}

// Where two renders differ, for a readable failure: rows and the first pixel.
QString differences(const QImage &actual, const QImage &expected, int bandRows)
{
    if (actual.size() != expected.size() || actual.format() != expected.format()) {
        return QStringLiteral("size/format %1x%2 vs %3x%4")
            .arg(actual.width()).arg(actual.height())
            .arg(expected.width()).arg(expected.height());
    }
    int first = -1;
    int last = -1;
    int count = 0;
    QString sample;
    for (int y = 0; y < actual.height(); ++y) {
        const auto *a = reinterpret_cast<const QRgb *>(actual.constScanLine(y));
        const auto *b = reinterpret_cast<const QRgb *>(expected.constScanLine(y));
        for (int x = 0; x < actual.width(); ++x) {
            if (a[x] != b[x]) {
                if (first < 0) {
                    first = y;
                    sample = QStringLiteral("(%1,%2) %3 vs %4").arg(x).arg(y)
                                 .arg(a[x], 8, 16, QLatin1Char('0'))
                                 .arg(b[x], 8, 16, QLatin1Char('0'));
                }
                last = y;
                ++count;
            }
        }
    }
    return first < 0 ? QString()
                     : QStringLiteral("%1 pixels differ in rows %2..%3 (band rows 0..%4), "
                                      "first %5")
                           .arg(count).arg(first).arg(last).arg(bandRows - 1).arg(sample);
}

// What a raster that painted `plan` from scratch (a full repaint) shows.
QImage freshlyStacked(const ChromeRenderPlan &plan, const HybridChrome::ChromePaintState &state)
{
    ChromeOverlayRaster fresh;
    (void)fresh.setPaintState(state);
    if (fresh.setPlan(plan) != ChromeOverlayRaster::Repaint::Full) {
        qFatal("a fresh raster must repaint fully");
    }
    return stacked(fresh);
}

// Largest per-channel difference, and whether every difference lies inside
// the first `bandRows` rows.
struct ChannelDifference final
{
    int maximum = 0;
    bool insideBand = true;
};

ChannelDifference channelDifference(const QImage &a, const QImage &b, int bandRows)
{
    ChannelDifference result;
    for (int y = 0; y < a.height(); ++y) {
        const auto *left = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const auto *right = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            const int delta = std::max({std::abs(qAlpha(left[x]) - qAlpha(right[x])),
                                        std::abs(qRed(left[x]) - qRed(right[x])),
                                        std::abs(qGreen(left[x]) - qGreen(right[x])),
                                        std::abs(qBlue(left[x]) - qBlue(right[x]))});
            if (delta > 0 && y >= bandRows) {
                result.insideBand = false;
            }
            result.maximum = std::max(result.maximum, delta);
        }
    }
    return result;
}

bool bodyBandRowsAreTransparent(const ChromeOverlayRaster &raster)
{
    const QImage &body = raster.body();
    for (int row = 0; row < raster.band().height(); ++row) {
        const uchar *line = body.constScanLine(row);
        for (qsizetype byte = 0; byte < body.bytesPerLine(); ++byte) {
            if (line[byte] != 0) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

class ChromeOverlayRasterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classifiesPlanDifferencesByWhatTheyRepaint();
    void identicalPlanRepaintsNothing();
    void memberTitleChangeTouchesNoPixels();
    void tabTitleChangeRepaintsOnlyTheBand_data();
    void tabTitleChangeRepaintsOnlyTheBand();
    void hoverRepaintsOnlyTheBand();
    void deckGlideFramesRepaintOnlyTheBand();
    void geometryChangeRepaintsTheWholeFrame();
    void invalidateForcesOneFullRepaint();
    void bandIsAFractionOfTheMaximizedFrame();
};

void ChromeOverlayRasterTest::classifiesPlanDifferencesByWhatTheyRepaint()
{
    const auto plan = makePlan({});
    QCOMPARE(compareChromePlans(plan, plan), ChromePlanDifferences{});

    auto titled = plan;
    titled.tabs[0].title = QStringLiteral("⠙ Claude Code");
    QCOMPARE(compareChromePlans(plan, titled),
             ChromePlanDifferences(ChromePlanDifference::TitleText));

    auto member = plan;
    member.members[0].title = QStringLiteral("other");
    QCOMPARE(compareChromePlans(plan, member),
             ChromePlanDifferences(ChromePlanDifference::UnpaintedText));

    auto moved = plan;
    moved.tabs[1].rect.translate(3.0, 0.0);
    QCOMPARE(compareChromePlans(plan, moved),
             ChromePlanDifferences(ChromePlanDifference::TabGeometry));

    auto focused = plan;
    focused.containerFocused = !plan.containerFocused;
    QVERIFY(compareChromePlans(plan, focused).testFlag(ChromePlanDifference::Structure));

    auto restyled = plan;
    restyled.style.palette.surface = Qt::red;
    QVERIFY(compareChromePlans(plan, restyled).testFlag(ChromePlanDifference::Structure));

    auto activePage = plan;
    activePage.tabs[0].active = false;
    activePage.tabs[1].active = true;
    QVERIFY(compareChromePlans(plan, activePage).testFlag(ChromePlanDifference::Structure));
}

void ChromeOverlayRasterTest::identicalPlanRepaintsNothing()
{
    ChromeOverlayRaster raster;
    const auto plan = makePlan({});
    QCOMPARE(raster.setPlan(plan), ChromeOverlayRaster::Repaint::Full);
    const qint64 bodyKey = raster.body().cacheKey();
    const qint64 bandKey = raster.band().cacheKey();
    for (int repeat = 0; repeat < 10; ++repeat) {
        QCOMPARE(raster.setPlan(plan), ChromeOverlayRaster::Repaint::None);
    }
    QCOMPARE(raster.body().cacheKey(), bodyKey);
    QCOMPARE(raster.band().cacheKey(), bandKey);
    QCOMPARE(raster.counters().fullRenders, quint64(1));
    QCOMPARE(raster.counters().bandRenders, quint64(0));
    QCOMPARE(raster.counters().unchangedUpdates, quint64(10));
}

void ChromeOverlayRasterTest::memberTitleChangeTouchesNoPixels()
{
    ChromeOverlayRaster raster;
    auto plan = makePlan({});
    raster.setPlan(plan);
    plan.members[0].title = QStringLiteral("a member title the decoration draws");
    QCOMPARE(raster.setPlan(plan), ChromeOverlayRaster::Repaint::None);
    // The newest text is still the published model.
    QCOMPARE(raster.plan().members[0].title, plan.members[0].title);
}

void ChromeOverlayRasterTest::tabTitleChangeRepaintsOnlyTheBand_data()
{
    QTest::addColumn<qreal>("ratio");
    QTest::addColumn<bool>("focused");
    QTest::addColumn<int>("style");
    QTest::addColumn<bool>("shaded");
    for (const qreal ratio : {1.0, 1.25, 1.5, 2.0}) {
        for (const bool focused : {true, false}) {
            for (int style = 0; style < 3; ++style) {
                QTest::addRow("ratio %.2f %s style %d", ratio,
                              focused ? "focused" : "unfocused", style)
                    << ratio << focused << style << false;
            }
        }
        QTest::addRow("ratio %.2f shaded", ratio) << ratio << true << 0 << true;
    }
}

// The pixel contract: after a title-only repaint, the band stacked over the
// unchanged body is byte-identical to one full render of the new plan.
void ChromeOverlayRasterTest::tabTitleChangeRepaintsOnlyTheBand()
{
    QFETCH(qreal, ratio);
    QFETCH(bool, focused);
    QFETCH(int, style);
    QFETCH(bool, shaded);
    PlanSpec spec;
    spec.size = shaded ? QSizeF(420.0, 32.0) : QSizeF(1280.0, 720.0);
    spec.ratio = ratio;
    spec.focused = focused;
    spec.shaded = shaded;
    if (style == 1) {
        spec.style = HybridChrome::ChromeStyle::qindaMacOS({});
        spec.style.material.opacity = 0.82; // translucent rows
        spec.style.material.highlight = true;
    } else if (style == 2) {
        spec.style = HybridChrome::ChromeStyle::qindaMacOS({});
        spec.style.titleLayout = HybridChrome::ContainerTitleLayout::SplitDeck;
    }
    ChromeOverlayRaster raster;
    QCOMPARE(raster.setPlan(makePlan(spec)), ChromeOverlayRaster::Repaint::Full);
    const qint64 bodyKey = raster.body().cacheKey();
    const QStringList spinner{QStringLiteral("⠙"), QStringLiteral("⠹"), QStringLiteral("⠸")};
    for (const auto &frame : spinner) {
        spec.tabTitles[0] = frame + QStringLiteral(" Claude Code is thinking");
        const auto next = makePlan(spec);
        QCOMPARE(raster.setPlan(next), ChromeOverlayRaster::Repaint::Band);
        QCOMPARE(raster.body().cacheKey(), bodyKey);
        QVERIFY(bodyBandRowsAreTransparent(raster));
        // Exactly what a full repaint of the new plan shows...
        const QString mismatch =
            differences(stacked(raster), freshlyStacked(next, {}), raster.band().height());
        QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        // ...and within one antialiasing step of a single whole-frame paint,
        // only ever inside the band (QPainter strokes the wide focus glow a
        // little differently on a shorter device).
        const auto drift =
            channelDifference(stacked(raster), fullRender(next, {}), raster.band().height());
        QVERIFY2(drift.maximum <= 2, qPrintable(QString::number(drift.maximum)));
        QVERIFY(drift.insideBand);
    }
    QCOMPARE(raster.counters().fullRenders, quint64(1));
    QCOMPARE(raster.counters().bandRenders, quint64(spinner.size()));
}

void ChromeOverlayRasterTest::hoverRepaintsOnlyTheBand()
{
    ChromeOverlayRaster raster;
    // Traffic-light buttons reveal their glyphs on hover (ADR-0264).
    const auto plan = makePlan({.size = QSizeF(1280.0, 720.0),
                                .ratio = 1.25,
                                .style = HybridChrome::ChromeStyle::qindaMacOS({})});
    raster.setPlan(plan);
    const qint64 bodyKey = raster.body().cacheKey();
    HybridChrome::ChromePaintState hovered;
    hovered.controlsHovered = true;
    hovered.hoveredTarget.kind = HybridChrome::HitKind::WindowButton;
    hovered.hoveredTarget.action = plan.buttons.constFirst().action;
    QCOMPARE(raster.setPaintState(hovered), ChromeOverlayRaster::Repaint::Band);
    QCOMPARE(raster.body().cacheKey(), bodyKey);
    QCOMPARE(stacked(raster), freshlyStacked(plan, hovered));
    QVERIFY(stacked(raster) != freshlyStacked(plan, {}));
    QCOMPARE(raster.setPaintState(hovered), ChromeOverlayRaster::Repaint::None);
    QCOMPARE(raster.setPaintState({}), ChromeOverlayRaster::Repaint::Band);
    QCOMPARE(stacked(raster), freshlyStacked(plan, {}));
}

void ChromeOverlayRasterTest::deckGlideFramesRepaintOnlyTheBand()
{
    PlanSpec spec;
    spec.size = QSizeF(1280.0, 720.0);
    spec.style = HybridChrome::ChromeStyle::qindaMacOS({});
    spec.style.titleLayout = HybridChrome::ContainerTitleLayout::SplitDeck;
    // Enough pages that the row overflows into the carousel, whose cards
    // move when the front page changes (inline tabs keep their slots).
    spec.size = QSizeF(800.0, 600.0);
    spec.tabTitles.clear();
    for (int page = 0; page < 10; ++page) {
        spec.tabTitles.append(QStringLiteral("page %1").arg(page));
    }
    const auto from = makePlan(spec);
    // The same deck with the second card in front.
    auto to = from;
    {
        HybridChrome::ChromeLayoutRequest request;
        request.containerId = from.containerId;
        request.outerRect = from.outerFrame;
        request.containerFocused = true;
        request.containerTitle = from.containerTitle;
        request.containerTitleIsGenerated = true;
        request.style = spec.style;
        for (qsizetype index = 0; index < spec.tabTitles.size(); ++index) {
            request.tabs.append({QStringLiteral("page-%1").arg(index), spec.tabTitles[index],
                                 index == 1});
        }
        request.members = {{QStringLiteral("window-term"), QStringLiteral("two"),
                            from.members[0].windowRect, true}};
        QString error;
        auto built = HybridChrome::ChromeLayoutEngine::build(request, &error);
        QVERIFY2(built, qPrintable(error));
        to = *built;
    }
    QVERIFY(from.tabsOverflowed);
    QVERIFY(HybridChrome::ChromeSplitDeck::canAnimate(from, to));
    ChromeOverlayRaster raster;
    raster.setPlan(from);
    // The first glide frame changes the active page: a full repaint.
    QCOMPARE(raster.setPlan(HybridChrome::ChromeSplitDeck::interpolate(from, to, 0.0)),
             ChromeOverlayRaster::Repaint::Full);
    for (const qreal progress : {0.25, 0.5, 0.75, 1.0}) {
        const auto frame = HybridChrome::ChromeSplitDeck::interpolate(from, to, progress);
        QCOMPARE(raster.setPlan(frame), ChromeOverlayRaster::Repaint::Band);
        QCOMPARE(stacked(raster), freshlyStacked(frame, {}));
    }
}

void ChromeOverlayRasterTest::geometryChangeRepaintsTheWholeFrame()
{
    ChromeOverlayRaster raster;
    raster.setPlan(makePlan({.size = QSizeF(1280.0, 720.0)}));
    QCOMPARE(raster.setPlan(makePlan({.size = QSizeF(1300.0, 720.0)})),
             ChromeOverlayRaster::Repaint::Full);
    QCOMPARE(raster.setPlan(makePlan({.size = QSizeF(1300.0, 720.0), .focused = false})),
             ChromeOverlayRaster::Repaint::Full);
    QCOMPARE(raster.counters().fullRenders, quint64(3));
}

void ChromeOverlayRasterTest::invalidateForcesOneFullRepaint()
{
    ChromeOverlayRaster raster;
    const auto plan = makePlan({.size = QSizeF(800.0, 600.0)});
    raster.setPlan(plan);
    raster.invalidate();
    QCOMPARE(raster.setPlan(plan), ChromeOverlayRaster::Repaint::Full);
    QCOMPARE(raster.setPlan(plan), ChromeOverlayRaster::Repaint::None);
}

void ChromeOverlayRasterTest::bandIsAFractionOfTheMaximizedFrame()
{
    // The live measurement: a maximized terminal container re-rendered and
    // re-uploaded its whole frame on every spinner tick.
    ChromeOverlayRaster raster;
    PlanSpec spec;
    raster.setPlan(makePlan(spec));
    const qint64 fullBytes = raster.body().sizeInBytes();
    spec.tabTitles[0] = QStringLiteral("⠙ Claude Code");
    raster.setPlan(makePlan(spec));
    const qint64 bandBytes = raster.band().sizeInBytes();
    QVERIFY(bandBytes > 0);
    QVERIFY2(bandBytes * 20 < fullBytes,
             qPrintable(QStringLiteral("band %1 B vs frame %2 B").arg(bandBytes).arg(fullBytes)));
    qInfo("2560x1408 container: full frame %lld bytes, title band %lld bytes per title change",
          qlonglong(fullBytes), qlonglong(bandBytes));
}

QTEST_MAIN(ChromeOverlayRasterTest)
#include "tst_chromeoverlayraster.moc"
