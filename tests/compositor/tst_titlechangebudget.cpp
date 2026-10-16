// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0383 acceptance: one window retitling at 10 Hz (a terminal agent's
// spinner) for 10 simulated seconds. The pipeline below is assembled from the
// production components in the order the plugin wires them:
//
//   KWin captionChanged
//     -> ManagedWindowRegistry caption limiter (ChangeRateLimiter, 250 ms)
//        -> KWinHybridSession::refreshWindowTitles:
//             HybridChromePlanBuilder::refreshTitles
//             -> KWinChromeManager::updateTitles -> overlay ChromeOverlayRaster
//        -> KWinControlEndpoint WindowsChanged limiter (250 ms)
//        -> KWinShellTaskFactsPublisher refresh limiter (250 ms)
//             -> ShellTaskFactsStore::publish -> TaskListSnapshotChanged
//                -> shell TaskListSnapshot read (cached payload)
//                   both authenticated through ShellCredentialCache
//
// The "legacy" run models fb1ca72e9's wiring with the same components: every
// caption change was its own event-loop turn, re-planned and re-published all
// chrome, re-rendered the full frame (no plan comparison), sampled task facts
// on the change and again on the shell's read, and looked up credentials for
// both. It exists only to report before/after numbers side by side.

#include "changeratelimiter.h"
#include "chromeoverlayraster.h"
#include "chromeplanlocalizer.h"
#include "hybridchromeplanbuilder.h"
#include "kwinchromemanager.h"

#include "qindaqt/compositor/shellcredentialcache.h"
#include "qindaqt/compositor/shelltaskfacts.h"
#include "qindaqt/hybrid/windowtopology.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QtTest>

#include <memory>

using namespace QindaQt;
using namespace QindaQt::Compositor;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

constexpr qint64 SimulatedMilliseconds = 10'000;
constexpr qint64 TitlePeriodMilliseconds = 100;
const QString ContainerId = QStringLiteral("container-term");
const QString TerminalId = QStringLiteral("window-term");
const QString EditorId = QStringLiteral("window-editor");
const QString ShellName = QStringLiteral(":1.42");

struct Counters final
{
    int captionChanges = 0;
    int captionDeliveries = 0;
    int windowsChangedEmits = 0;
    int taskFactsSamples = 0;
    int taskListChangedEmits = 0;
    int chromeReplans = 0;
    int chromePublications = 0;
    quint64 fullFrameRenders = 0;
    quint64 bandRenders = 0;
    qint64 uploadedBytes = 0;
    qsizetype credentialLookups = 0;
};

// The production overlay minus KWin: localizes the plan and lets the real
// raster decide what to repaint. `legacy` reproduces the old overlay, which
// re-rendered the whole image on every setRenderPlan().
class RasterOverlay final : public ChromeOverlay
{
public:
    RasterOverlay(std::shared_ptr<ChromeOverlayRaster> raster, bool legacy)
        : m_raster(std::move(raster)), m_legacy(legacy)
    {
    }
    void setRenderPlan(HybridChrome::ChromeRenderPlan plan) override
    {
        const QPointF origin = plan.outerFrame.topLeft();
        if (m_legacy) {
            m_raster->invalidate();
        }
        m_raster->setPlan(localizeChromeRenderPlan(std::move(plan), origin));
    }
    void setPointerHoverTarget(HybridChrome::ChromeHitTarget) override {}
    void invalidateRendering() noexcept override { m_raster->invalidate(); }
    bool setStackingAnchor(const QString &, QString *) override { return true; }
    void showOverlay() override { m_visible = true; }
    void hideOverlay() noexcept override { m_visible = false; }
    bool isVisible() const noexcept override { return m_visible; }
    bool hasStackingAnchor() const noexcept override { return true; }
    void closeOverlay() noexcept override { m_visible = false; }

private:
    std::shared_ptr<ChromeOverlayRaster> m_raster;
    bool m_legacy = false;
    bool m_visible = false;
};

class RasterOverlayFactory final : public ChromeOverlayFactory
{
public:
    explicit RasterOverlayFactory(bool legacy) : m_legacy(legacy) {}
    std::unique_ptr<ChromeOverlay> create(const QString &) override
    {
        return std::make_unique<RasterOverlay>(raster, m_legacy);
    }
    std::shared_ptr<ChromeOverlayRaster> raster = std::make_shared<ChromeOverlayRaster>();

private:
    bool m_legacy = false;
};

Core::WindowContainer terminalContainer()
{
    Core::WindowContainer container(ContainerId);
    QString error;
    if (!container.addPage(QStringLiteral("page-term"), QStringLiteral("leaf-term"),
                           TerminalId, &error)
        || !container.addPage(QStringLiteral("page-editor"), QStringLiteral("leaf-editor"),
                              EditorId, &error)) {
        qFatal("container fixture failed: %s", qPrintable(error));
    }
    return container;
}

// The chrome a synchronization publishes for the maximized terminal
// container, with every title read from `titles` the way the session's
// HybridWindowTitleLookup reads KWin captions.
HybridChrome::ChromeRenderPlan planFor(const Core::WindowContainer &container,
                                       const QHash<QString, QString> &titles)
{
    HybridChrome::ChromeLayoutRequest request;
    request.containerId = container.id();
    request.outerRect = QRectF(0.0, 32.0, 2560.0, 1408.0);
    request.containerFocused = true;
    request.containerTitle = QStringLiteral("Container");
    request.containerTitleIsGenerated = true;
    request.style = HybridChrome::ChromeStyle::qindaMacOS({});
    for (const auto &page : container.pages()) {
        request.tabs.append({page.id(), titles.value(page.root().windowId()),
                             page.id() == container.activePageId()});
    }
    const auto &metrics = request.metrics;
    const QRectF content = request.outerRect.adjusted(
        metrics.outerBorder, metrics.outerBorder + metrics.titleBarHeight,
        -metrics.outerBorder, -metrics.outerBorder);
    request.members = {{TerminalId, titles.value(TerminalId), content, true}};
    QString error;
    auto plan = HybridChrome::ChromeLayoutEngine::build(request, &error);
    if (!plan) {
        qFatal("chrome fixture failed: %s", qPrintable(error));
    }
    return std::move(*plan);
}

ShellTaskFactsCandidate factsFor(const QString &epoch, quint64 actionRevision,
                                 const QHash<QString, QString> &titles)
{
    ShellTaskFactsCandidate candidate;
    candidate.actionGeneration = {epoch, actionRevision};
    candidate.outputs = {{QStringLiteral("eDP-1")}};
    candidate.workspaces = {{QStringLiteral("desktop-1")}};
    candidate.containers = {{ContainerId, 7, ShellTaskContainerAuthority::HybridProcess}};
    candidate.windows = {
        {.windowId = TerminalId, .applicationId = QStringLiteral("org.qindaqt.Terminal"),
         .applicationName = QStringLiteral("Terminal"), .title = titles.value(TerminalId),
         .colorHex = {}, .role = ShellTaskWindowRole::ContainerPrimary, .active = true,
         .outputId = QStringLiteral("eDP-1"), .workspaceIds = {QStringLiteral("desktop-1")},
         .containerId = ContainerId},
        {.windowId = EditorId, .applicationId = QStringLiteral("org.qindaqt.TextEditor"),
         .applicationName = QStringLiteral("Text Editor"), .title = titles.value(EditorId),
         .colorHex = {}, .role = ShellTaskWindowRole::ContainerMember, .outputId = QStringLiteral("eDP-1"),
         .workspaceIds = {QStringLiteral("desktop-1")}, .containerId = ContainerId}};
    return candidate;
}

QString spinnerTitle(int tick)
{
    static const QStringList frames{
        QStringLiteral("⠋"), QStringLiteral("⠙"), QStringLiteral("⠹"), QStringLiteral("⠸"),
        QStringLiteral("⠼"), QStringLiteral("⠴"), QStringLiteral("⠦"), QStringLiteral("⠧"),
        QStringLiteral("⠇"), QStringLiteral("⠏")};
    return frames[tick % frames.size()] + QStringLiteral(" Claude Code");
}

Counters run(bool legacy)
{
    const auto container = terminalContainer();
    QString error;
    auto topology = Hybrid::WindowTopology::create({}, {container}, 7, &error);
    if (!topology) {
        qFatal("topology fixture failed: %s", qPrintable(error));
    }
    QHash<QString, QString> titles{{TerminalId, spinnerTitle(0)},
                                   {EditorId, QStringLiteral("notes.txt")}};

    RasterOverlayFactory factory(legacy);
    KWinChromeManager chrome(factory);
    if (!chrome.updateFromSnapshot(*topology, {{ContainerId, planFor(container, titles)}},
                                   {}, &error)) {
        qFatal("initial publication failed: %s", qPrintable(error));
    }
    const auto baseline = factory.raster->counters();

    qsizetype rawLookups = 0;
    const auto lookup = [&rawLookups](const QString &) -> std::optional<qint64> {
        ++rawLookups;
        return 4242;
    };
    ShellCredentialCache cache(lookup);
    const auto authenticate = [&](const QString &name) {
        return legacy ? lookup(name) : cache.processIdForUniqueName(name);
    };

    ShellTaskFactsStore store(QStringLiteral("epoch-a"));
    (void)store.publish(factsFor(QStringLiteral("epoch-a"), 3, titles));

    Counters counters;
    const qint64 interval = legacy ? 0 : 250;
    ChangeRateLimiter captions(interval);
    ChangeRateLimiter windowsChanged(interval);
    ChangeRateLimiter taskFacts(interval);

    const auto sampleTaskFacts = [&] {
        ++counters.taskFactsSamples;
        const auto result = store.publish(factsFor(QStringLiteral("epoch-a"), 3, titles));
        if (result == ShellTaskFactsPublishResult::Published) {
            ++counters.taskListChangedEmits;
            (void)authenticate(ShellName); // the targeted hint's owner check
            (void)authenticate(ShellName); // the shell's TaskListSnapshot read
            if (legacy) {
                // The old read re-sampled before answering.
                ++counters.taskFactsSamples;
                (void)store.publish(factsFor(QStringLiteral("epoch-a"), 3, titles));
            }
        }
    };
    const auto deliverCaption = [&](qint64 now) {
        ++counters.captionDeliveries;
        if (legacy) {
            // synchronizeChrome(): re-plan and re-publish every container.
            ++counters.chromeReplans;
            ++counters.chromePublications;
            (void)chrome.updateFromSnapshot(
                *topology, {{ContainerId, planFor(container, titles)}}, {}, &error);
        } else {
            const auto published = chrome.plan(ContainerId);
            HybridChromePlanOptions options;
            options.containerTitle = published->containerTitle;
            const auto refresh = HybridChromePlanBuilder::refreshTitles(
                container, *published, options,
                [&titles](const QString &windowId) { return titles.value(windowId); });
            using Outcome = HybridChromePlanBuilder::TitleRefresh::Outcome;
            if (refresh.outcome == Outcome::NeedsReplan) {
                qFatal("a title-only change asked for a re-plan");
            }
            if (refresh.outcome == Outcome::Patched
                && !chrome.updateTitles(ContainerId, refresh.plan, &error)) {
                qFatal("title update failed: %s", qPrintable(error));
            }
        }
        if (windowsChanged.admit(QString(), now)) {
            ++counters.windowsChangedEmits;
        }
        if (taskFacts.admit(QString(), now)) {
            sampleTaskFacts();
        }
    };

    int tick = 0;
    for (qint64 now = 0; now <= SimulatedMilliseconds + 1000; ++now) {
        if (now < SimulatedMilliseconds && now % TitlePeriodMilliseconds == 0) {
            titles[TerminalId] = spinnerTitle(++tick);
            ++counters.captionChanges;
            if (captions.admit(TerminalId, now)) {
                deliverCaption(now);
            }
        }
        if (!captions.takeDue(now).isEmpty()) {
            deliverCaption(now);
        }
        if (!windowsChanged.takeDue(now).isEmpty()) {
            ++counters.windowsChangedEmits;
        }
        if (!taskFacts.takeDue(now).isEmpty()) {
            sampleTaskFacts();
        }
    }
    const auto &after = factory.raster->counters();
    counters.fullFrameRenders = after.fullRenders - baseline.fullRenders;
    counters.bandRenders = after.bandRenders - baseline.bandRenders;
    counters.uploadedBytes = after.uploadedBytes - baseline.uploadedBytes;
    counters.credentialLookups = rawLookups;
    return counters;
}

void report(const char *label, const Counters &counters)
{
    qInfo("%s: %d caption changes -> %d deliveries, %d WindowsChanged, %d task-facts "
          "samples, %d TaskListSnapshotChanged, %d chrome re-plans, %d chrome publications, "
          "%llu full-frame renders, %llu title-band renders, %lld bytes uploaded, "
          "%lld credential lookups",
          label, counters.captionChanges, counters.captionDeliveries,
          counters.windowsChangedEmits, counters.taskFactsSamples,
          counters.taskListChangedEmits, counters.chromeReplans, counters.chromePublications,
          static_cast<unsigned long long>(counters.fullFrameRenders),
          static_cast<unsigned long long>(counters.bandRenders),
          qlonglong(counters.uploadedBytes), qlonglong(counters.credentialLookups));
}

} // namespace

class TitleChangeBudgetTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void spinnerTitleStaysWithinTheBudget();
    void titleUpdatesRefuseAnythingButText();
};

void TitleChangeBudgetTest::spinnerTitleStaysWithinTheBudget()
{
    const Counters before = run(true);
    const Counters after = run(false);
    report("before (fb1ca72e9 wiring)", before);
    report("after (ADR-0383)", after);

    QCOMPARE(before.captionChanges, 100);
    QCOMPARE(after.captionChanges, 100);

    // Snapshot builds and emits: at most about 4 Hz for 10 s.
    QVERIFY(after.captionDeliveries <= 41);
    QVERIFY(after.windowsChangedEmits <= 41);
    QVERIFY(after.taskFactsSamples <= 41);
    QVERIFY(after.taskListChangedEmits <= 41);
    // The last title is never lost.
    QVERIFY(after.taskListChangedEmits >= 39);
    // No container re-plans, no re-publication, no full-frame chrome render.
    QCOMPARE(after.chromeReplans, 0);
    QCOMPARE(after.chromePublications, 0);
    QCOMPARE(after.fullFrameRenders, quint64(0));
    QVERIFY(after.bandRenders >= 39 && after.bandRenders <= 41);
    // One credential lookup for the shell's connection, then cached.
    QCOMPARE(after.credentialLookups, qsizetype(1));

    // The model of the old wiring reproduces the measured storm.
    QCOMPARE(before.windowsChangedEmits, 100);
    QCOMPARE(before.chromeReplans, 100);
    QCOMPARE(before.fullFrameRenders, quint64(100));
    QCOMPARE(before.taskFactsSamples, 200);
    QVERIFY(after.uploadedBytes * 30 < before.uploadedBytes);
}

// KWinChromeManager::updateTitles skips topology validation, so it must
// never let geometry, focus or tab structure through.
void TitleChangeBudgetTest::titleUpdatesRefuseAnythingButText()
{
    const auto container = terminalContainer();
    QString error;
    auto topology = Hybrid::WindowTopology::create({}, {container}, 7, &error);
    QVERIFY2(topology, qPrintable(error));
    const QHash<QString, QString> titles{{TerminalId, QStringLiteral("a")},
                                         {EditorId, QStringLiteral("b")}};
    RasterOverlayFactory factory(false);
    KWinChromeManager chrome(factory);
    const auto published = planFor(container, titles);
    QVERIFY2(chrome.updateFromSnapshot(*topology, {{ContainerId, published}}, {}, &error),
             qPrintable(error));

    auto moved = published;
    moved.outerFrame.translate(10.0, 0.0);
    QVERIFY(!chrome.updateTitles(ContainerId, moved, &error));
    QVERIFY(error.contains(QStringLiteral("more than text")));
    auto refocused = published;
    refocused.containerFocused = false;
    QVERIFY(!chrome.updateTitles(ContainerId, refocused, &error));
    auto retabbed = published;
    retabbed.tabs[1].rect.translate(5.0, 0.0);
    QVERIFY(!chrome.updateTitles(ContainerId, retabbed, &error));
    QVERIFY(!chrome.updateTitles(QStringLiteral("unknown"), published, &error));
    QCOMPARE(chrome.plan(ContainerId)->outerFrame, published.outerFrame);

    auto retitled = published;
    retitled.tabs[0].title = QStringLiteral("⠙ Claude Code");
    const quint64 bandRendersBefore = factory.raster->counters().bandRenders;
    QVERIFY2(chrome.updateTitles(ContainerId, retitled, &error), qPrintable(error));
    QCOMPARE(chrome.plan(ContainerId)->tabs[0].title, QStringLiteral("⠙ Claude Code"));
    QCOMPARE(factory.raster->counters().bandRenders, bandRendersBefore + 1);
    // An unchanged title is accepted and repaints nothing.
    QVERIFY(chrome.updateTitles(ContainerId, retitled, &error));
    QCOMPARE(factory.raster->counters().bandRenders, bandRendersBefore + 1);
}

QTEST_MAIN(TitleChangeBudgetTest)
#include "tst_titlechangebudget.moc"
