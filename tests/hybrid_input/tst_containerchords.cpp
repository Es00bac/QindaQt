// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282 window-management chords: modifier + left moves the container
// under the pointer, modifier + Shift + left drags the one window (docking),
// modifier + right resizes -- for a mouse and, through
// TabletPointerTranslator, for a pen whose tip is left and barrel is right.

#include "interactiontestsupport.h"

#include "qindaqt/hybrid_input/containerchords.h"
#include "qindaqt/hybrid_input/tabletpointertranslator.h"

#include <QtTest>

using namespace QindaQt::HybridInput;
using namespace QindaQt::HybridInput::TestSupport;

namespace {

constexpr auto Meta = Qt::MetaModifier;
const Qt::KeyboardModifiers MetaShift = Qt::MetaModifier | Qt::ShiftModifier;

PointerEvent press(QPointF position, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    return {.position = position, .changedButton = button, .buttons = button,
            .modifiers = modifiers};
}

PointerEvent moveTo(QPointF position, Qt::MouseButtons held, Qt::KeyboardModifiers modifiers)
{
    return {.position = position, .changedButton = Qt::NoButton, .buttons = held,
            .modifiers = modifiers};
}

PointerEvent release(QPointF position, Qt::MouseButton button)
{
    return {.position = position, .changedButton = button, .buttons = Qt::NoButton,
            .modifiers = Qt::NoModifier};
}

HitTarget groupedMember()
{
    return {HitKind::MemberTitle, QStringLiteral("group"), QStringLiteral("member"), {}};
}

HitTarget independentWindow()
{
    return {HitKind::MemberTitle, {}, QStringLiteral("alone"), {}};
}

} // namespace

class ContainerChordsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classifiesEveryChord();
    void disabledOrForeignModifiersClaimNothing();
    void picksTheNearestEdgeLikeKWin();
    void modifierLeftMovesTheWholeContainer();
    void modifierLeftOverAnIndependentWindowIsLeftToKWin();
    void modifierShiftLeftStillDocksTheOneWindow();
    void modifierRightResizesTheContainerFromTheNearestCorner();
    void plainModifierClickSendsNothingDownstream();
    void lostReleaseEndsTheGesture();
    void rebindingTheModifierMovesEveryChord();
    void penTipAndBarrelSpeakTheSameChords();
    void penLeavingProximityReleasesWhatIsHeld();
    void penTipAndEraserFollowTheOwnersPenChords();
};

void ContainerChordsTest::classifiesEveryChord()
{
    const auto classify = [](Qt::MouseButton button, Qt::KeyboardModifiers modifiers,
                             bool container, bool independent) {
        return classifyPointerChord({.button = button,
                                     .modifiers = modifiers,
                                     .dockChord = MetaShift,
                                     .overContainer = container,
                                     .overIndependentWindow = independent});
    };
    QCOMPARE(classify(Qt::LeftButton, Meta, true, false), PointerChordAction::ContainerMove);
    QCOMPARE(classify(Qt::LeftButton, Meta, false, true), PointerChordAction::None);
    QCOMPARE(classify(Qt::LeftButton, MetaShift, true, false), PointerChordAction::MemberDock);
    QCOMPARE(classify(Qt::LeftButton, MetaShift, false, true), PointerChordAction::MemberDock);
    QCOMPARE(classify(Qt::RightButton, Meta, true, false), PointerChordAction::ContainerResize);
    QCOMPARE(classify(Qt::RightButton, Meta, false, true), PointerChordAction::WindowResize);
    // Neither a container nor a window: a panel keeps its Meta+right-click.
    QCOMPARE(classify(Qt::RightButton, Meta, false, false), PointerChordAction::None);
    QCOMPARE(classify(Qt::MiddleButton, Meta, true, false), PointerChordAction::None);
    QCOMPARE(classify(Qt::RightButton, MetaShift, true, false), PointerChordAction::None);
}

void ContainerChordsTest::disabledOrForeignModifiersClaimNothing()
{
    QCOMPARE(classifyPointerChord({.button = Qt::LeftButton,
                                   .modifiers = Meta,
                                   .dockChord = std::nullopt,
                                   .overContainer = true}),
             PointerChordAction::None);
    for (const Qt::KeyboardModifiers modifiers :
         {Qt::KeyboardModifiers(Qt::NoModifier), Qt::KeyboardModifiers(Qt::AltModifier),
          Qt::MetaModifier | Qt::ControlModifier}) {
        QCOMPARE(classifyPointerChord({.button = Qt::LeftButton,
                                       .modifiers = modifiers,
                                       .dockChord = MetaShift,
                                       .overContainer = true}),
                 PointerChordAction::None);
    }
    QCOMPARE(windowManagementModifier(MetaShift), Qt::KeyboardModifiers(Meta));
    QCOMPARE(windowManagementModifier(std::nullopt), Qt::KeyboardModifiers(Qt::NoModifier));
}

void ContainerChordsTest::picksTheNearestEdgeLikeKWin()
{
    const QRectF frame(0, 0, 300, 300);
    QCOMPARE(nearestResizeEdges(frame, {10, 10}), Qt::TopEdge | Qt::LeftEdge);
    QCOMPARE(nearestResizeEdges(frame, {290, 10}), Qt::TopEdge | Qt::RightEdge);
    QCOMPARE(nearestResizeEdges(frame, {10, 290}), Qt::BottomEdge | Qt::LeftEdge);
    QCOMPARE(nearestResizeEdges(frame, {290, 290}), Qt::BottomEdge | Qt::RightEdge);
    QCOMPARE(nearestResizeEdges(frame, {150, 10}), Qt::Edges(Qt::TopEdge));
    QCOMPARE(nearestResizeEdges(frame, {150, 290}), Qt::Edges(Qt::BottomEdge));
    QCOMPARE(nearestResizeEdges(frame, {10, 150}), Qt::Edges(Qt::LeftEdge));
    QCOMPARE(nearestResizeEdges(frame, {140, 150}), Qt::Edges(Qt::LeftEdge));
    QCOMPARE(nearestResizeEdges(frame, {160, 150}), Qt::Edges(Qt::RightEdge));
    QCOMPARE(nearestResizeEdges(frame, {900, 900}), Qt::BottomEdge | Qt::RightEdge);
    QCOMPARE(nearestResizeEdges(QRectF{}, {1, 1}), Qt::BottomEdge | Qt::RightEdge);
}

// Owner: "meta+leftmouse should move a container". Before ADR-0282 the bare
// modifier went to KWin, whose move of one member detached it from the
// container instead of moving the container.
void ContainerChordsTest::modifierLeftMovesTheWholeContainer()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    InteractionController controller(resolver);
    QVERIFY(controller.pointerPress(press({100, 100}, Qt::LeftButton, Meta)).consumed);
    const auto begun = controller.pointerMove(moveTo({120, 110}, Qt::LeftButton, Meta));
    QCOMPARE(begun.intents.size(), qsizetype{2});
    QCOMPARE(begun.intents[0].kind, InteractionKind::ContainerMove);
    QCOMPARE(begun.intents[0].phase, IntentPhase::Begin);
    QCOMPARE(begun.intents[0].source,
             HitTarget(HitKind::OuterTitle, QStringLiteral("group"), {}, {}));
    QCOMPARE(begun.intents[0].delta, QPointF(20, 10));
    const auto committed = controller.pointerRelease(release({130, 140}, Qt::LeftButton));
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().delta, QPointF(30, 40));
    QVERIFY(!controller.active());

    // The container's own chrome (title, tab) is the container too.
    resolver.hit = {HitKind::Tab, QStringLiteral("group"), QStringLiteral("member"), {}};
    QVERIFY(controller.pointerPress(press({100, 100}, Qt::LeftButton, Meta)).consumed);
    QCOMPARE(controller.interactionKind(), InteractionKind::ContainerMove);
}

void ContainerChordsTest::modifierLeftOverAnIndependentWindowIsLeftToKWin()
{
    RecordingResolver resolver;
    resolver.hit = independentWindow();
    InteractionController controller(resolver);
    QVERIFY(!controller.pointerPress(press({10, 10}, Qt::LeftButton, Meta)).consumed);
    QVERIFY(!controller.active());
    resolver.hit = {HitKind::IconChip, {}, QStringLiteral("chip"), {}};
    QVERIFY(!controller.pointerPress(press({10, 10}, Qt::LeftButton, Meta)).consumed);
    resolver.hit = {};
    QVERIFY(!controller.pointerPress(press({10, 10}, Qt::LeftButton, Meta)).consumed);
    QVERIFY(!controller.pointerPress(press({10, 10}, Qt::RightButton, Meta)).consumed);
}

void ContainerChordsTest::modifierShiftLeftStillDocksTheOneWindow()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    resolver.pointerTarget = {QStringLiteral("other"), QStringLiteral("peer"), DockZone::Tab};
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVERIFY(controller.pointerPress(press({10, 10}, Qt::LeftButton, MetaShift)).consumed);
    const auto moved = controller.pointerMove(moveTo({40, 10}, Qt::LeftButton, MetaShift));
    QCOMPARE(moved.intents.constFirst().kind, InteractionKind::MemberDock);
    QCOMPARE(moved.intents.constFirst().source.memberId, QStringLiteral("member"));
    const auto dropped = controller.pointerRelease(release({40, 10}, Qt::LeftButton));
    QCOMPARE(dropped.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(dropped.intents.constFirst().target, resolver.pointerTarget);
}

void ContainerChordsTest::modifierRightResizesTheContainerFromTheNearestCorner()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    resolver.frame = QRectF(0, 0, 900, 600);
    InteractionController controller(resolver);
    QVERIFY(controller.pointerPress(press({50, 40}, Qt::RightButton, Meta)).consumed);
    const auto begun = controller.pointerMove(moveTo({30, 20}, Qt::RightButton, Meta));
    QCOMPARE(begun.intents[0].kind, InteractionKind::ContainerResize);
    QCOMPARE(begun.intents[0].source.kind, HitKind::OuterResize);
    QCOMPARE(begun.intents[0].source.edges, Qt::TopEdge | Qt::LeftEdge);
    QVERIFY(begun.intents[0].source.isValid());
    // A left release does not end a right-button gesture.
    QVERIFY(controller.pointerRelease(release({30, 20}, Qt::LeftButton)).intents.isEmpty());
    QVERIFY(controller.active());
    const auto committed = controller.pointerRelease(release({20, 20}, Qt::RightButton));
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().delta, QPointF(-30, -20));
}

void ContainerChordsTest::plainModifierClickSendsNothingDownstream()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    InteractionController controller(resolver);
    QVERIFY(controller.pointerPress(press({50, 40}, Qt::LeftButton, Meta)).consumed);
    const auto released = controller.pointerRelease(release({52, 41}, Qt::LeftButton));
    QVERIFY(released.consumed);
    QVERIFY2(released.intents.isEmpty(),
             "a click that never began must not ask placement to cancel");
    QVERIFY(!controller.active());
}

// ADR-0282: a release that never arrives must still end the gesture; a
// controller stuck active swallows every later pointer event.
void ContainerChordsTest::lostReleaseEndsTheGesture()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVERIFY(controller.pointerPress(press({50, 40}, Qt::LeftButton, Meta)).consumed);
    QCOMPARE(controller.pointerMove(moveTo({60, 40}, Qt::LeftButton, Meta))
                 .intents.constFirst().phase,
             IntentPhase::Begin);
    const auto lost = controller.pointerMove(moveTo({70, 40}, Qt::NoButton, Meta));
    QVERIFY(lost.consumed);
    QCOMPARE(lost.intents.constFirst().phase, IntentPhase::Cancel);
    QVERIFY(!controller.active());
    QVERIFY(!controller.pointerMove(moveTo({80, 40}, Qt::NoButton, Meta)).consumed);
}

void ContainerChordsTest::rebindingTheModifierMovesEveryChord()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    InteractionController controller(resolver);
    controller.setPointerModifiers(Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(!controller.pointerPress(press({1, 1}, Qt::LeftButton, Meta)).consumed);
    QVERIFY(controller.pointerPress(press({1, 1}, Qt::LeftButton, Qt::AltModifier)).consumed);
    QCOMPARE(controller.interactionKind(), InteractionKind::ContainerMove);
    static_cast<void>(controller.cancel());

    controller.setPointerModifiers(std::nullopt);
    QVERIFY(!controller.pointerPress(press({1, 1}, Qt::LeftButton, Qt::AltModifier)).consumed);
    QVERIFY(!controller.pointerPress(press({1, 1}, Qt::RightButton, Qt::AltModifier)).consumed);

    InteractionController dockOnly(resolver, {.containerChords = false});
    QVERIFY(!dockOnly.pointerPress(press({1, 1}, Qt::LeftButton, Meta)).consumed);
}

// Owner: "including the pen button". The tip is the left button and the
// barrel the right button, with the keyboard's modifiers.
void ContainerChordsTest::penTipAndBarrelSpeakTheSameChords()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    resolver.frame = QRectF(0, 0, 900, 600);
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    TabletPointerTranslator pen;

    // Meta + tip drag moves the container.
    QVERIFY(controller.pointerPress(pen.tip(true, {100, 100}, Meta)).consumed);
    QCOMPARE(controller.pointerMove(pen.motion({110, 105}, Meta)).intents.constFirst().kind,
             InteractionKind::ContainerMove);
    QCOMPARE(controller.pointerRelease(pen.tip(false, {120, 110}, Meta))
                 .intents.constFirst().phase,
             IntentPhase::Commit);

    // Meta + Shift + tip docks the one window.
    QVERIFY(controller.pointerPress(pen.tip(true, {100, 100}, MetaShift)).consumed);
    QCOMPARE(controller.interactionKind(), InteractionKind::MemberDock);
    static_cast<void>(controller.pointerRelease(pen.tip(false, {100, 100}, MetaShift)));

    // Meta + barrel (hovering, tip up) resizes from the nearest corner.
    static_cast<void>(pen.motion({880, 590}, Meta));
    const auto barrel = pen.button(TabletPointerTranslator::ButtonStylus, true, Meta);
    QVERIFY(barrel.has_value());
    QCOMPARE(barrel->changedButton, Qt::RightButton);
    QCOMPARE(barrel->position, QPointF(880, 590));
    QVERIFY(controller.pointerPress(*barrel).consumed);
    const auto resized = controller.pointerMove(pen.motion({900, 610}, Meta));
    QCOMPARE(resized.intents.constFirst().kind, InteractionKind::ContainerResize);
    QCOMPARE(resized.intents.constFirst().source.edges, Qt::BottomEdge | Qt::RightEdge);
    QCOMPARE(controller.pointerRelease(
                 *pen.button(TabletPointerTranslator::ButtonStylus, false, Meta))
                 .intents.constFirst().phase,
             IntentPhase::Commit);

    // Without the modifier the barrel is an ordinary right click: unclaimed.
    QVERIFY(!controller.pointerPress(
        *pen.button(TabletPointerTranslator::ButtonStylus, true, Qt::NoModifier)).consumed);
    QVERIFY(!pen.button(0x14a, true, Meta).has_value());
    QCOMPARE(TabletPointerTranslator::mouseButtonForStylus(TabletPointerTranslator::ButtonStylus2),
             Qt::MiddleButton);
}

void ContainerChordsTest::penLeavingProximityReleasesWhatIsHeld()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    TabletPointerTranslator pen;
    QVERIFY(controller.pointerPress(pen.tip(true, {10, 10}, Meta)).consumed);
    static_cast<void>(controller.pointerMove(pen.motion({30, 10}, Meta)));
    const auto releases = pen.leaveProximity(Meta);
    QCOMPARE(releases.size(), qsizetype{1});
    QCOMPARE(releases.constFirst().changedButton, Qt::LeftButton);
    QCOMPARE(controller.pointerRelease(releases.constFirst()).intents.constFirst().phase,
             IntentPhase::Commit);
    QVERIFY(!controller.active());
    QCOMPARE(pen.buttons(), Qt::MouseButtons(Qt::NoButton));
    QVERIFY(pen.leaveProximity(Meta).isEmpty());
}

// Owner, 2026-09-28: "Using the tip moves a window regularly, moves a
// container, while holding meta. While holding meta, if you use the eraser,
// it moves a window into and out of containers ... If you're not, then it's
// just a normal eraser." and "meta+ctrl+tip resizes a window or container".
void ContainerChordsTest::penTipAndEraserFollowTheOwnersPenChords()
{
    RecordingResolver resolver;
    resolver.hit = groupedMember();
    resolver.frame = QRectF(0, 0, 900, 600);
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    TabletPointerTranslator pen;
    const std::optional<Qt::KeyboardModifiers> dock = MetaShift;
    const Qt::KeyboardModifiers MetaCtrl = Qt::MetaModifier | Qt::ControlModifier;

    // Meta + tip moves the container.
    auto down = pen.contact(TabletTool::Pen, true, {100, 100}, Meta, dock);
    QVERIFY(down.has_value());
    QCOMPARE(down->changedButton, Qt::LeftButton);
    QVERIFY(controller.pointerPress(*down).consumed);
    QCOMPARE(controller.pointerMove(pen.motion({120, 100}, Meta)).intents.constFirst().kind,
             InteractionKind::ContainerMove);
    // Letting go of Meta before lifting still ends the same gesture.
    auto up = pen.contact(TabletTool::Pen, false, {130, 100}, Qt::NoModifier, dock);
    QVERIFY(up.has_value());
    QCOMPARE(controller.pointerRelease(*up).intents.constFirst().phase, IntentPhase::Commit);
    QVERIFY(!controller.active());

    // Meta + eraser docks the one window, and its motion stays a docking drag.
    down = pen.contact(TabletTool::Eraser, true, {100, 100}, Meta, dock);
    QVERIFY(down.has_value());
    QCOMPARE(down->modifiers, MetaShift);
    QVERIFY(controller.pointerPress(*down).consumed);
    QCOMPARE(controller.interactionKind(), InteractionKind::MemberDock);
    QCOMPARE(pen.motion({140, 100}, Meta).modifiers, MetaShift);
    static_cast<void>(controller.pointerRelease(
        *pen.contact(TabletTool::Eraser, false, {140, 100}, Meta, dock)));
    QVERIFY(!controller.active());

    // Meta + Ctrl + tip resizes from the nearest corner.
    down = pen.contact(TabletTool::Pen, true, {880, 590}, MetaCtrl, dock);
    QVERIFY(down.has_value());
    QCOMPARE(down->changedButton, Qt::RightButton);
    QCOMPARE(down->modifiers, Qt::KeyboardModifiers(Meta));
    QVERIFY(controller.pointerPress(*down).consumed);
    const auto resized = controller.pointerMove(pen.motion({900, 610}, MetaCtrl));
    QCOMPARE(resized.intents.constFirst().kind, InteractionKind::ContainerResize);
    QCOMPARE(resized.intents.constFirst().source.edges, Qt::BottomEdge | Qt::RightEdge);
    up = pen.contact(TabletTool::Pen, false, {900, 610}, MetaCtrl, dock);
    QCOMPARE(up->changedButton, Qt::RightButton);
    QCOMPARE(controller.pointerRelease(*up).intents.constFirst().phase, IntentPhase::Commit);

    // Without Meta the eraser is the application's: nothing to map or claim.
    QVERIFY(!pen.contact(TabletTool::Eraser, true, {10, 10}, Qt::NoModifier, dock).has_value());
    QVERIFY(!pen.contact(TabletTool::Eraser, false, {10, 10}, Qt::NoModifier, dock).has_value());
    QVERIFY(!pen.contact(TabletTool::Eraser, true, {10, 10}, Qt::ShiftModifier, dock).has_value());
    // A plain tip is a plain left press the controller never claims.
    down = pen.contact(TabletTool::Pen, true, {10, 10}, Qt::NoModifier, dock);
    QVERIFY(down.has_value());
    QVERIFY(!controller.pointerPress(*down).consumed);
    // With modifier gestures disabled nothing is a chord and the eraser stays plain.
    QVERIFY(!pen.contact(TabletTool::Eraser, true, {10, 10}, Meta, std::nullopt).has_value());

    // KWin's own move/resize filter can consume a lift QindaQt never sees;
    // the next press forgets that contact instead of reporting it held.
    TabletPointerTranslator kwinOwned;
    static_cast<void>(kwinOwned.contact(TabletTool::Pen, true, {5, 5}, MetaCtrl, dock));
    QCOMPARE(kwinOwned.buttons(), Qt::MouseButtons(Qt::RightButton));
    down = kwinOwned.contact(TabletTool::Pen, true, {6, 6}, Meta, dock);
    QCOMPARE(down->buttons, Qt::MouseButtons(Qt::LeftButton));
}

QTEST_GUILESS_MAIN(ContainerChordsTest)
#include "tst_containerchords.moc"
