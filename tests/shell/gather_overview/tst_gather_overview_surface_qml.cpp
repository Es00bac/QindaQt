// SPDX-License-Identifier: GPL-3.0-or-later

// The gather overview surface, rendered over a stub projection. The surface
// draws what it is handed and reports what was clicked, so the whole test is
// "does the right component appear at the right rectangle, and does the right
// signal come back" - no window facts producer, no compositor, no KWin.

#include "../task_list/task_list_applet_qml_theme_fixture.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_GatherOverviewPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

namespace {

[[nodiscard]] QVariantMap tile(const QString &taskId, const QString &lane,
                               const QRectF &frame,
                               const QString &title = QString())
{
    QVariantMap item;
    item.insert(QStringLiteral("taskId"), taskId);
    item.insert(QStringLiteral("windowId"), QString());
    item.insert(QStringLiteral("lane"), lane);
    item.insert(QStringLiteral("frame"), frame);
    item.insert(QStringLiteral("title"),
                title.isEmpty() ? taskId + QStringLiteral(" title") : title);
    item.insert(QStringLiteral("applicationId"), taskId + QStringLiteral(".desktop"));
    item.insert(QStringLiteral("applicationName"), taskId.toUpper());
    // The controller resolves this from applicationId, exactly as the task
    // list's does; an empty name exercises the fallbackText path.
    item.insert(QStringLiteral("iconName"), QString());
    item.insert(QStringLiteral("iconText"), taskId.left(1).toUpper());
    item.insert(QStringLiteral("colorHex"), QString());
    item.insert(QStringLiteral("windowCount"), 1);
    item.insert(QStringLiteral("active"), false);
    item.insert(QStringLiteral("urgent"), false);
    item.insert(QStringLiteral("generationRevision"), 11);
    item.insert(QStringLiteral("accessibleName"), taskId + QStringLiteral(" accessible"));
    return item;
}

struct StubOptions final {
    bool available = true;
    bool interactive = true;
    bool empty = false;
    int gridColumns = 2;
    int gridRows = 1;
    qreal gridContentHeight = 176;
    int windowsHidden = 0;
    int sourceOverflowCount = 0;
};

[[nodiscard]] QVariantMap stubProjection(const QVariantList &items,
                                         const StubOptions &options = {})
{
    QVariantMap projection;
    projection.insert(QStringLiteral("available"), options.available);
    projection.insert(QStringLiteral("interactive"), options.interactive);
    projection.insert(QStringLiteral("empty"), options.empty);
    projection.insert(QStringLiteral("diagnostic"), QString());
    projection.insert(QStringLiteral("field"), QRectF(90, 90, 1740, 900));
    projection.insert(QStringLiteral("gridViewport"), QRectF(450, 90, 1380, 900));
    projection.insert(QStringLiteral("gridColumns"), options.gridColumns);
    projection.insert(QStringLiteral("gridRows"), options.gridRows);
    projection.insert(QStringLiteral("gridContentHeight"), options.gridContentHeight);
    projection.insert(QStringLiteral("maximumScrollOffset"), 0.0);
    projection.insert(QStringLiteral("appliedScrollOffset"), 0.0);
    projection.insert(QStringLiteral("iconCount"), 0);
    projection.insert(QStringLiteral("cardCount"), 0);
    projection.insert(QStringLiteral("windowCount"), 0);
    projection.insert(QStringLiteral("windowsHidden"), options.windowsHidden);
    projection.insert(QStringLiteral("sourceOverflowCount"),
                      options.sourceOverflowCount);
    projection.insert(QStringLiteral("items"), items);
    return projection;
}

[[nodiscard]] QList<QQuickItem *> itemsNamed(QQuickItem *root,
                                             const QString &name)
{
    QList<QQuickItem *> matches;
    if (root == nullptr)
        return matches;
    if (root->objectName() == name)
        matches.append(root);
    for (QQuickItem *child : root->childItems())
        matches.append(itemsNamed(child, name));
    return matches;
}

[[nodiscard]] QQuickItem *firstNamed(QQuickItem *root, const QString &name)
{
    const auto matches = itemsNamed(root, name);
    return matches.isEmpty() ? nullptr : matches.first();
}

// Where a tile actually lands on screen. Walking one parent level is wrong:
// the visual's parent is the delegate's Loader, which fills the delegate, so
// its own x/y are zero. The scene position is what "drawn here" means.
[[nodiscard]] QPointF scenePosition(QQuickItem *item)
{
    return item == nullptr ? QPointF() : item->mapToScene(QPointF(0, 0));
}

// A drawn tile means more than "the object exists": it has to reach the window
// through its parent chain and occupy real space.
[[nodiscard]] bool isDrawn(QQuickItem *item, QQuickWindow *window)
{
    if (item == nullptr || window == nullptr)
        return false;
    if (item->width() <= 0 || item->height() <= 0)
        return false;
    for (QQuickItem *walk = item; walk != nullptr; walk = walk->parentItem()) {
        if (walk == window->contentItem())
            return true;
    }
    return false;
}

} // namespace

class GatherOverviewSurfaceQmlTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void everyLaneDrawsItsOwnComponentAtThePlannersFrame();
    void originTranslatesDesktopLogicalFramesIntoTheSurface();
    void clickingATileReportsTheItemItWasProjectedFrom();
    void escapeAsksToDismiss();
    void clickingTheScrimAsksToDismiss();
    void aFencedProjectionDrawsButRefusesEveryTile();
    void anEmptyProjectionShowsAnEmptyState();
    void hiddenTilesAndUpstreamOverflowAreStated();
    void anUnavailableProjectionDrawsNothing();
    void grabsOneRepresentativeFrameForReview();

private:
    [[nodiscard]] QQuickItem *createSurface(const QVariantMap &projection);

    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QQuickItem> m_surface;
};

void GatherOverviewSurfaceQmlTests::initTestCase()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
}

void GatherOverviewSurfaceQmlTests::init()
{
    m_engine = std::make_unique<QQmlEngine>();
    m_engine->addImportPath(QStringLiteral(QINDAQT_GATHER_OVERVIEW_QML_IMPORT_PATH));

    QString error;
    QVERIFY2(TaskListAppletQmlTest::publishTokens(*m_engine, &error),
             qPrintable(error));

    m_window = std::make_unique<QQuickWindow>();
    m_window->resize(1920, 1080);
    m_window->show();
}

void GatherOverviewSurfaceQmlTests::cleanup()
{
    m_surface.reset();
    m_window.reset();
    m_engine.reset();
}

QQuickItem *
GatherOverviewSurfaceQmlTests::createSurface(const QVariantMap &projection)
{
    QQmlComponent component(m_engine.get());
    component.setData(R"qml(
        import QtQuick
        import QindaQt.Shell.GatherOverview 1.0
        GatherOverviewSurface {
            width: 1920
            height: 1080
        }
    )qml",
                      QUrl(QStringLiteral("inline:gather-surface.qml")));
    for (int spin = 0; spin < 100 && component.status() == QQmlComponent::Loading;
         ++spin) {
        QTest::qWait(10);
    }
    if (!component.isReady()) {
        qWarning("%s", qPrintable(component.errorString()));
        return nullptr;
    }
    // `projection` is a required property, so it has to be set at creation.
    auto *created = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
        {{QStringLiteral("projection"), projection}}));
    if (created == nullptr) {
        qWarning("%s", qPrintable(component.errorString()));
        return nullptr;
    }
    m_surface.reset(created);
    created->setParentItem(m_window->contentItem());
    // One polish pass, so frames and lazy Loaders settle before assertions.
    QTest::qWait(50);
    return created;
}

void GatherOverviewSurfaceQmlTests::
    everyLaneDrawsItsOwnComponentAtThePlannersFrame()
{
    const QRectF iconFrame(90, 90, 48, 48);
    const QRectF cardFrame(154, 90, 240, 64);
    const QRectF windowFrame(450, 90, 260, 176);

    QQuickItem *surface = createSurface(stubProjection({
        tile(QStringLiteral("i1"), QStringLiteral("icon"), iconFrame),
        tile(QStringLiteral("c1"), QStringLiteral("card"), cardFrame),
        tile(QStringLiteral("w1"), QStringLiteral("window"), windowFrame),
    }));
    QVERIFY(surface != nullptr);

    QQuickItem *chip = firstNamed(surface, QStringLiteral("gatherIconChip"));
    QQuickItem *card = firstNamed(surface, QStringLiteral("gatherContainerCard"));
    QQuickItem *win = firstNamed(surface, QStringLiteral("gatherWindowTile"));
    QVERIFY2(chip != nullptr, "the icon lane drew no chip");
    QVERIFY2(card != nullptr, "the card lane drew no card");
    QVERIFY2(win != nullptr, "the window grid drew no tile");

    // Each reaches the window and occupies real space.
    QVERIFY(isDrawn(chip, m_window.get()));
    QVERIFY(isDrawn(card, m_window.get()));
    QVERIFY(isDrawn(win, m_window.get()));

    // And sits exactly where the planner put it, at the size it was given.
    QCOMPARE(scenePosition(chip), iconFrame.topLeft());
    QCOMPARE(scenePosition(card), cardFrame.topLeft());
    QCOMPARE(scenePosition(win), windowFrame.topLeft());
    QCOMPARE(QSizeF(chip->width(), chip->height()), iconFrame.size());
    QCOMPARE(QSizeF(card->width(), card->height()), cardFrame.size());
    QCOMPARE(QSizeF(win->width(), win->height()), windowFrame.size());

    // Exactly one component per item: no lane drew another lane's visual.
    QCOMPARE(itemsNamed(surface, QStringLiteral("gatherIconChip")).size(), 1);
    QCOMPARE(itemsNamed(surface, QStringLiteral("gatherContainerCard")).size(), 1);
    QCOMPARE(itemsNamed(surface, QStringLiteral("gatherWindowTile")).size(), 1);
}

void GatherOverviewSurfaceQmlTests::
    originTranslatesDesktopLogicalFramesIntoTheSurface()
{
    // A second output at x=1920: the projection's frames stay desktop-logical
    // and the surface subtracts its own origin.
    const QRectF frame(1920 + 90, 90, 260, 176);
    QQuickItem *surface = createSurface(
        stubProjection({tile(QStringLiteral("w1"), QStringLiteral("window"), frame)}));
    QVERIFY(surface != nullptr);
    surface->setProperty("origin", QPointF(1920, 0));
    QTest::qWait(50);

    QQuickItem *win = firstNamed(surface, QStringLiteral("gatherWindowTile"));
    QVERIFY(win != nullptr);
    QCOMPARE(scenePosition(win), QPointF(90, 90));
}

void GatherOverviewSurfaceQmlTests::
    clickingATileReportsTheItemItWasProjectedFrom()
{
    const QRectF windowFrame(450, 90, 260, 176);
    QQuickItem *surface = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"), windowFrame)}));
    QVERIFY(surface != nullptr);

    QSignalSpy activated(surface, SIGNAL(activated(QVariant)));
    QVERIFY(activated.isValid());

    QTest::mouseClick(m_window.get(), Qt::LeftButton, Qt::NoModifier,
                      windowFrame.center().toPoint());
    QTRY_COMPARE(activated.count(), 1);

    const QVariantMap reported = activated.first().first().toMap();
    QCOMPARE(reported.value(QStringLiteral("taskId")).toString(),
             QStringLiteral("w1"));
    // The generation it was projected from rides along, so the owner can echo
    // it and let stale-revision arbitration refuse a dead action.
    QCOMPARE(reported.value(QStringLiteral("generationRevision")).toULongLong(),
             11ull);
}

void GatherOverviewSurfaceQmlTests::escapeAsksToDismiss()
{
    QQuickItem *surface = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"),
              QRectF(450, 90, 260, 176))}));
    QVERIFY(surface != nullptr);
    surface->forceActiveFocus();

    QSignalSpy dismissed(surface, SIGNAL(dismissRequested()));
    QVERIFY(dismissed.isValid());
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QTRY_COMPARE(dismissed.count(), 1);
}

void GatherOverviewSurfaceQmlTests::clickingTheScrimAsksToDismiss()
{
    // One tile in the top-left; the click lands far from it, on the scrim.
    QQuickItem *surface = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"),
              QRectF(450, 90, 260, 176))}));
    QVERIFY(surface != nullptr);

    QSignalSpy dismissed(surface, SIGNAL(dismissRequested()));
    QSignalSpy activated(surface, SIGNAL(activated(QVariant)));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(1800, 1000));
    QTRY_COMPARE(dismissed.count(), 1);
    QCOMPARE(activated.count(), 0);
}

void GatherOverviewSurfaceQmlTests::aFencedProjectionDrawsButRefusesEveryTile()
{
    StubOptions fenced;
    fenced.interactive = false;
    const QRectF windowFrame(450, 90, 260, 176);
    QQuickItem *surface = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"), windowFrame)},
        fenced));
    QVERIFY(surface != nullptr);

    // The retained generation is still drawn - it is the last thing the user
    // actually saw.
    QQuickItem *win = firstNamed(surface, QStringLiteral("gatherWindowTile"));
    QVERIFY(win != nullptr);
    QVERIFY(isDrawn(win, m_window.get()));

    // But a click on it does nothing, and the surface says why.
    QSignalSpy activated(surface, SIGNAL(activated(QVariant)));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, Qt::NoModifier,
                      windowFrame.center().toPoint());
    QTest::qWait(50);
    QCOMPARE(activated.count(), 0);

    QQuickItem *notice = firstNamed(surface, QStringLiteral("gatherDegradedNotice"));
    QVERIFY(notice != nullptr);
    QVERIFY(notice->isVisible());
}

void GatherOverviewSurfaceQmlTests::anEmptyProjectionShowsAnEmptyState()
{
    StubOptions empty;
    empty.empty = true;
    empty.gridColumns = 0;
    empty.gridRows = 0;
    QQuickItem *surface = createSurface(stubProjection({}, empty));
    QVERIFY(surface != nullptr);

    QQuickItem *state = firstNamed(surface, QStringLiteral("gatherEmptyState"));
    QVERIFY(state != nullptr);
    QVERIFY(state->isVisible());
    // Inside the field, not floating at the surface origin.
    QCOMPARE(state->x(), 90.0);
    QCOMPARE(state->y(), 90.0);
    QVERIFY(state->width() > 0);

    QVERIFY(itemsNamed(surface, QStringLiteral("gatherWindowTile")).isEmpty());
}

void GatherOverviewSurfaceQmlTests::hiddenTilesAndUpstreamOverflowAreStated()
{
    StubOptions truncated;
    truncated.windowsHidden = 7;
    truncated.sourceOverflowCount = 0;
    QQuickItem *surface = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"),
              QRectF(450, 90, 260, 176))},
        truncated));
    QVERIFY(surface != nullptr);

    QQuickItem *hint = firstNamed(surface, QStringLiteral("gatherHiddenHint"));
    QVERIFY(hint != nullptr);
    QVERIFY(hint->isVisible());
    QVERIFY2(hint->property("text").toString().contains(QStringLiteral("7")),
             qPrintable(hint->property("text").toString()));

    // Upstream overflow is named separately: the overview cannot show what the
    // task list never handed over, and should not imply it has everything.
    StubOptions capped;
    capped.windowsHidden = 2;
    capped.sourceOverflowCount = 250;
    QQuickItem *second = createSurface(stubProjection(
        {tile(QStringLiteral("w1"), QStringLiteral("window"),
              QRectF(450, 90, 260, 176))},
        capped));
    QVERIFY(second != nullptr);
    QQuickItem *cappedHint = firstNamed(second, QStringLiteral("gatherHiddenHint"));
    QVERIFY(cappedHint != nullptr);
    const QString text = cappedHint->property("text").toString();
    QVERIFY2(text.contains(QStringLiteral("250")), qPrintable(text));
}

void GatherOverviewSurfaceQmlTests::anUnavailableProjectionDrawsNothing()
{
    StubOptions unavailable;
    unavailable.available = false;
    unavailable.interactive = false;
    QQuickItem *surface = createSurface(stubProjection({}, unavailable));
    QVERIFY(surface != nullptr);

    QVERIFY(itemsNamed(surface, QStringLiteral("gatherWindowTile")).isEmpty());
    QVERIFY(itemsNamed(surface, QStringLiteral("gatherIconChip")).isEmpty());
    QVERIFY(itemsNamed(surface, QStringLiteral("gatherContainerCard")).isEmpty());

    QQuickItem *scrim = firstNamed(surface, QStringLiteral("gatherScrim"));
    QVERIFY(scrim != nullptr);
    QVERIFY2(!scrim->isVisible(),
             "an unavailable projection must not even dim the desktop");

    // And no click on it can be mistaken for a dismissal of something real.
    QSignalSpy activated(surface, SIGNAL(activated(QVariant)));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(960, 540));
    QTest::qWait(50);
    QCOMPARE(activated.count(), 0);
}

// A rendered frame, written where a human can look at it. Every other row
// here asserts a fact; this one exists because the arrangement is a visual
// design and a passing assertion does not tell anyone whether it looks right.
// It asserts only that a frame was produced and is not blank - the layout
// itself is reviewed by eye from the PNG.
void GatherOverviewSurfaceQmlTests::grabsOneRepresentativeFrameForReview()
{
    // A full lane set: three iconified chips, two container cards, six window
    // tiles in a 3x2 grid, with the planner's 90 px buffer honoured.
    QVariantList items;
    for (int i = 0; i < 3; ++i) {
        items.append(tile(QStringLiteral("chip%1").arg(i + 1),
                          QStringLiteral("icon"),
                          QRectF(90, 90 + i * 64, 48, 48)));
    }
    for (int i = 0; i < 2; ++i) {
        items.append(tile(QStringLiteral("group%1").arg(i + 1),
                          QStringLiteral("card"),
                          QRectF(154, 90 + i * 80, 240, 64),
                          QStringLiteral("Container %1").arg(i + 1)));
    }
    for (int i = 0; i < 6; ++i) {
        items.append(tile(QStringLiteral("win%1").arg(i + 1),
                          QStringLiteral("window"),
                          QRectF(426 + (i % 3) * 276, 90 + (i / 3) * 192,
                                 260, 176),
                          QStringLiteral("Window %1").arg(i + 1)));
    }
    QVariantMap projection = stubProjection({});
    projection.insert(QStringLiteral("items"), items);
    projection.insert(QStringLiteral("gridColumns"), 3);
    projection.insert(QStringLiteral("gridRows"), 2);
    projection.insert(QStringLiteral("gridContentHeight"), 384);

    QQuickItem *surface = createSurface(projection);
    QVERIFY(surface != nullptr);
    // Long enough for the icon fallbacks and the scrim material to settle.
    QTest::qWait(400);

    const QImage frame = m_window->grabWindow();
    QVERIFY(!frame.isNull());
    QCOMPARE(frame.size().isEmpty(), false);
    const QString path =
        QStringLiteral(QINDAQT_GATHER_OVERVIEW_ARTIFACT_DIR "/gather-overview.png");
    QVERIFY2(frame.save(path), qPrintable(path));
    qInfo().noquote() << "gather overview frame written to" << path;

    // Not blank: a surface that constructed but painted nothing would still
    // save a valid PNG, and that is exactly the failure worth catching.
    QSet<QRgb> distinct;
    for (int y = 0; y < frame.height(); y += 8) {
        for (int x = 0; x < frame.width(); x += 8) {
            distinct.insert(frame.pixel(x, y));
            if (distinct.size() > 4) {
                break;
            }
        }
    }
    QVERIFY2(distinct.size() > 4, "the grabbed frame has almost no detail");
}

QTEST_MAIN(GatherOverviewSurfaceQmlTests)

#include "tst_gather_overview_surface_qml.moc"
