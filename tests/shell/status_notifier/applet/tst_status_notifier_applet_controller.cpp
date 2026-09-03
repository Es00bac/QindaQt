// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h>

#include "status_notifier_applet_test_fakes.h"

#include <QtTest>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifierApplet;
using namespace QindaQt::StatusNotifierApplet::Tests;

namespace
{

// Arms the fake with one live ready item and returns its exact owner key.
OwnerKey stageSingleItem(FakeStatusNotifierSource &source,
                         const QString &identity = QStringLiteral("org.qindaqt.fake"),
                         const QString &uniqueName = QStringLiteral(":1.42"),
                         quint64 generation = 3)
{
    source.m_presentation.state = PresentationState::Ready;
    source.m_presentation.items = {
        makePresentationItem(identity, uniqueName, QStringLiteral("/StatusNotifierItem"),
                             generation, QStringLiteral("Fake item"),
                             QStringLiteral("active")),
    };
    ItemDescriptor descriptor;
    descriptor.identity = identity;
    descriptor.title = QStringLiteral("Fake item");
    descriptor.status = ItemStatus::Active;
    source.m_descriptors = { descriptor };
    source.m_generations[uniqueName] = generation;
    return source.m_presentation.items.constFirst().owner;
}

} // namespace

class StatusNotifierAppletControllerTests final : public QObject
{
    Q_OBJECT

private slots:
    void readDeniedWithholdsObservation();
    void activateDeniedRefusesBeforeDispatch();
    void admittedActivateDispatchesExactlyOnce();
    void reentrantChangedDuringDispatchStaysExactlyOnce();
    void seamRefusalSurfacesTruthfulFeedback();
    void staleGenerationIsRefused();
    void ownerLossReprojectsAndFencesOldKeys();
    void overflowTruthPassesThrough();
    void iconsCrossAsDataUrlsWithPlaceholderTruth();
    void iconSizeChangeReRenders();
    void menuRowsPreviewIsBoundedAndFenced();
};

void StatusNotifierAppletControllerTests::readDeniedWithholdsObservation()
{
    FakeStatusNotifierSource source;
    stageSingleItem(source);
    StatusNotifierAppletController controller(&source, false, true);

    QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(controller.phaseReasonText(), QLatin1String(kReasonStatusItemsReadNotGranted));
    QVERIFY(controller.itemRows().isEmpty());
    QCOMPARE(controller.itemCount(), 0);
    QCOMPARE(controller.watcherLive(), false);
    // Withheld observation means withheld: the seam was never even read.
    QCOMPARE(source.m_presentationCalls, 0);
    QCOMPARE(source.m_descriptorCalls, 0);
    QCOMPARE(source.m_renderCalls, 0);

    // Re-notifying changes nothing while the grant stays denied.
    source.emitChanged();
    QCOMPARE(source.m_presentationCalls, 0);
    QCOMPARE(source.m_renderCalls, 0);

    QVERIFY(controller.menuRowsFor(QStringLiteral(":1.42"),
                                   QStringLiteral("/StatusNotifierItem"), 3).isEmpty());
}

void StatusNotifierAppletControllerTests::activateDeniedRefusesBeforeDispatch()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, false);

    QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
    QCOMPARE(controller.itemRows().size(), 1);

    QSignalSpy feedbackSpy(&controller, &StatusNotifierAppletController::feedbackChanged);
    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), false);
    QCOMPARE(controller.secondaryActivateItem(key.uniqueName, key.objectPath, key.generation),
             false);
    QCOMPARE(controller.openContextMenu(key.uniqueName, key.objectPath, key.generation), false);
    QCOMPARE(source.m_calls.size(), 0);
    QCOMPARE(feedbackSpy.size(), 3);
    QCOMPARE(controller.feedback(), QStringLiteral("Activating status items is not permitted."));
    QCOMPARE(controller.feedbackStatus(), QStringLiteral("error"));

    controller.clearFeedback();
    QCOMPARE(controller.feedbackPresent(), false);
    QVERIFY(controller.feedback().isEmpty());
}

void StatusNotifierAppletControllerTests::admittedActivateDispatchesExactlyOnce()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, true);

    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), true);
    QCOMPARE(source.m_calls.size(), 1);
    QCOMPARE(source.m_calls.constFirst().kind, QStringLiteral("activate"));
    QCOMPARE(source.m_calls.constFirst().key, key);
    QCOMPARE(source.m_calls.constFirst().x, 0);
    QCOMPARE(source.m_calls.constFirst().y, 0);
    QCOMPARE(controller.feedbackPresent(), false);

    QCOMPARE(controller.secondaryActivateItem(key.uniqueName, key.objectPath, key.generation),
             true);
    QCOMPARE(controller.openContextMenu(key.uniqueName, key.objectPath, key.generation), true);
    QCOMPARE(source.m_calls.size(), 3);
    QCOMPARE(source.m_calls.at(1).kind, QStringLiteral("secondaryActivate"));
    QCOMPARE(source.m_calls.at(2).kind, QStringLiteral("contextMenu"));
}

void StatusNotifierAppletControllerTests::reentrantChangedDuringDispatchStaysExactlyOnce()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    source.m_emitChangedInsideDispatch = true;
    StatusNotifierAppletController controller(&source, true, true);

    QSignalSpy reprojectSpy(&controller, &StatusNotifierAppletController::stateReprojected);
    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), true);
    // The seam emitted changed() synchronously inside the dispatch; the
    // reentrant reprojection must not cause a second dispatch.
    QCOMPARE(source.m_calls.size(), 1);
    QVERIFY(reprojectSpy.size() >= 1);
    QCOMPARE(controller.feedbackPresent(), false);
}

void StatusNotifierAppletControllerTests::seamRefusalSurfacesTruthfulFeedback()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    source.m_acceptIntents = false;
    source.m_refusalReason = QStringLiteral("scripted-refusal");
    StatusNotifierAppletController controller(&source, true, true);

    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), false);
    QCOMPARE(source.m_calls.size(), 1); // the seam was asked; it refused
    QCOMPARE(controller.feedbackPresent(), true);
    QCOMPARE(controller.feedback(),
             QStringLiteral("The status item refused the request (scripted-refusal)."));
}

void StatusNotifierAppletControllerTests::staleGenerationIsRefused()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, true);

    // A generation the registry no longer considers current.
    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation + 1), false);
    QCOMPARE(source.m_calls.size(), 0);
    QCOMPARE(controller.feedback(), QStringLiteral("This status item is no longer available."));

    controller.clearFeedback();
    // A never-presented row, even at the live generation, is refused.
    QCOMPARE(controller.activateItem(key.uniqueName, QStringLiteral("/Other"), key.generation),
             false);
    QCOMPARE(source.m_calls.size(), 0);
    // Generation 0 is never live truth.
    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, 0), false);
    QCOMPARE(source.m_calls.size(), 0);
}

void StatusNotifierAppletControllerTests::ownerLossReprojectsAndFencesOldKeys()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, true);
    QCOMPARE(controller.itemRows().size(), 1);

    // The owner vanishes; the seam reports the new truth.
    source.m_presentation.state = PresentationState::Empty;
    source.m_presentation.items.clear();
    source.m_descriptors.clear();
    source.m_generations.clear();
    QSignalSpy reprojectSpy(&controller, &StatusNotifierAppletController::stateReprojected);
    source.emitChanged();
    QCOMPARE(reprojectSpy.size(), 1);
    QCOMPARE(controller.phaseText(), QStringLiteral("empty"));
    QVERIFY(controller.itemRows().isEmpty());
    QCOMPARE(controller.itemCount(), 0);

    // The pre-loss key is fenced: no dispatch, stale feedback.
    QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), false);
    QCOMPARE(source.m_calls.size(), 0);
    QCOMPARE(controller.feedbackPresent(), true);
}

void StatusNotifierAppletControllerTests::overflowTruthPassesThrough()
{
    FakeStatusNotifierSource source;
    source.m_presentation.state = PresentationState::Ready;
    for (int i = 0; i < 30; ++i) {
        const QString identity = QStringLiteral("org.qindaqt.item%1").arg(i);
        source.m_presentation.items.append(
            makePresentationItem(identity, QStringLiteral(":1.%1").arg(100 + i),
                                 QStringLiteral("/StatusNotifierItem"), quint64(50 + i),
                                 identity, QStringLiteral("active")));
        ItemDescriptor descriptor;
        descriptor.identity = identity;
        source.m_descriptors.append(descriptor);
        source.m_generations[QStringLiteral(":1.%1").arg(100 + i)] = quint64(50 + i);
    }

    StatusNotifierAppletController controller(&source, true, true);
    QCOMPARE(controller.itemCount(), 30);
    QCOMPARE(controller.presentedCount(), 24);
    QCOMPARE(controller.overflowCount(), 6);
    QCOMPARE(controller.overflowText(), QStringLiteral("6 more items"));
    QCOMPARE(controller.itemRows().size(), 24);
    // Icon enrichment stays bounded by the presentation cap.
    QCOMPARE(source.m_renderCalls, 24);
}

void StatusNotifierAppletControllerTests::iconsCrossAsDataUrlsWithPlaceholderTruth()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, true);

    const QVariantList rows = controller.itemRows();
    QCOMPARE(rows.size(), 1);
    const auto row = rows.constFirst().value<StatusNotifierItemRow>();
    QVERIFY(row.iconDataUrl.startsWith(QLatin1String("data:image/png;base64,")));
    // The fake renders the deterministic placeholder, and the row says so.
    QCOMPARE(row.iconIsPlaceholder, true);
    QCOMPARE(row.uniqueName, key.uniqueName);

    // A scripted real icon is not flagged as placeholder.
    QImage realIcon(32, 32, QImage::Format_ARGB32_Premultiplied);
    realIcon.fill(Qt::red);
    source.m_scriptedIcon = realIcon;
    source.emitChanged();
    const auto enriched = controller.itemRows().constFirst().value<StatusNotifierItemRow>();
    QCOMPARE(enriched.iconIsPlaceholder, false);
    QVERIFY(enriched.iconDataUrl.startsWith(QLatin1String("data:image/png;base64,")));
    QVERIFY(enriched.iconDataUrl != row.iconDataUrl);
}

void StatusNotifierAppletControllerTests::iconSizeChangeReRenders()
{
    FakeStatusNotifierSource source;
    stageSingleItem(source);
    StatusNotifierAppletController controller(&source, true, true, 22);
    QCOMPARE(controller.iconSize(), 22);
    const int initialRenders = source.m_renderCalls;
    const QString initialUrl =
        controller.itemRows().constFirst().value<StatusNotifierItemRow>().iconDataUrl;

    QSignalSpy reprojectSpy(&controller, &StatusNotifierAppletController::stateReprojected);
    controller.setIconSize(32);
    QCOMPARE(controller.iconSize(), 32);
    QCOMPARE(reprojectSpy.size(), 1);
    QVERIFY(source.m_renderCalls > initialRenders);
    const QString resizedUrl =
        controller.itemRows().constFirst().value<StatusNotifierItemRow>().iconDataUrl;
    QVERIFY(resizedUrl != initialUrl);

    // Same size: no reprojection, no re-render.
    controller.setIconSize(32);
    QCOMPARE(source.m_renderCalls, initialRenders + 1);

    // Out-of-range sizes clamp to the S1 512-pixel ceiling.
    controller.setIconSize(9999);
    QCOMPARE(controller.iconSize(), 512);
}

void StatusNotifierAppletControllerTests::menuRowsPreviewIsBoundedAndFenced()
{
    FakeStatusNotifierSource source;
    const OwnerKey key = stageSingleItem(source);
    MenuPayload menu;
    MenuEntry open;
    open.kind = MenuEntry::Kind::Item;
    open.label = QStringLiteral("Open");
    MenuEntry sub;
    sub.kind = MenuEntry::Kind::SubMenu;
    sub.label = QStringLiteral("More");
    MenuEntry child;
    child.kind = MenuEntry::Kind::Item;
    child.parentId = 1;
    child.label = QStringLiteral("Leaf");
    menu.entries = { open, sub, child };
    source.m_descriptors[0].menu = menu;
    source.emitChanged();

    StatusNotifierAppletController controller(&source, true, true);
    const QVariantList rows =
        controller.menuRowsFor(key.uniqueName, key.objectPath, key.generation);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).value<StatusNotifierMenuRow>().kind, QStringLiteral("item"));
    QCOMPARE(rows.at(1).value<StatusNotifierMenuRow>().depth, 0);
    QCOMPARE(rows.at(2).value<StatusNotifierMenuRow>().depth, 1);

    // Stale generation: empty preview, no crash.
    QVERIFY(controller.menuRowsFor(key.uniqueName, key.objectPath, key.generation + 1).isEmpty());
    // Owner gone entirely: empty preview.
    source.m_generations.clear();
    QVERIFY(controller.menuRowsFor(key.uniqueName, key.objectPath, key.generation).isEmpty());
}

QTEST_GUILESS_MAIN(StatusNotifierAppletControllerTests)
#include "tst_status_notifier_applet_controller.moc"
