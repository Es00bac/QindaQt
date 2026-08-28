// SPDX-License-Identifier: GPL-3.0-or-later
#include "inputcapabilities.h"
#include "hybridchromepointerrouter.h"
#include "kwininteractionfilter.h"
#include "kwininputadapter.h"
#include "mutationcontrol.h"
#include "normalizedinputevent.h"

#include <core/inputdevice.h>
#include <input_event.h>

#include <QJsonArray>
#include <QTest>

#include <chrono>

using namespace std::chrono_literals;

namespace QindaQt::Compositor::KWinIntegration {
namespace {

class EmptyInteractionResolver final
    : public HybridInput::InteractionTargetResolver
{
public:
    HybridInput::HitTarget hitTest(const QPointF &) const override { return hit; }
    HybridInput::DockTarget pointerDockTarget(
        const HybridInput::HitTarget &, const QPointF &) const override
    {
        return {};
    }
    HybridInput::DockTarget keyboardDockTarget(
        const HybridInput::HitTarget &, HybridInput::DockZone) const override
    {
        return {};
    }

    HybridInput::HitTarget hit;
};

class FakeInputDevice final : public KWin::InputDevice
{
public:
    QString name() const override { return QStringLiteral("Test Combo Device"); }
    quint32 vendor() const override { return 0x1234U; }
    quint32 product() const override { return 0x5678U; }
    quint32 busType() const override { return 0x03U; }
    bool isEnabled() const override { return enabled; }
    void setEnabled(bool value) override { enabled = value; }
    bool isKeyboard() const override { return keyboard; }
    bool isPointer() const override { return pointer; }
    bool isTouchpad() const override { return touchpad; }
    bool isTouch() const override { return touch; }
    bool isTabletTool() const override { return tabletTool; }
    bool isTabletPad() const override { return tabletPad; }
    bool isTabletModeSwitch() const override { return tabletModeSwitch; }
    bool isLidSwitch() const override { return lidSwitch; }
    QString outputName() const override { return output; }
    void setOutputName(const QString &value) override { output = value; }
    int tabletPadButtonCount() const override { return 4; }
    int tabletPadDialCount() const override { return 1; }
    int tabletPadRingCount() const override { return 2; }
    int tabletPadStripCount() const override { return 3; }
    bool tabletToolIsRelative() const override { return relativeTabletTool; }

    QString output;
    bool enabled = true;
    bool keyboard = false;
    bool pointer = false;
    bool touchpad = false;
    bool touch = false;
    bool tabletTool = false;
    bool tabletPad = false;
    bool tabletModeSwitch = false;
    bool lidSwitch = false;
    bool relativeTabletTool = false;
};

class FakeTabletTool final : public KWin::InputDeviceTabletTool
{
public:
    quint64 serialId() const override { return 23U; }
    quint64 uniqueId() const override { return 42U; }
    Type type() const override { return Pen; }
    QList<Capability> capabilities() const override { return {Pressure, Tilt}; }
};

QStringList strings(const QJsonArray &array)
{
    QStringList result;
    for (const auto &entry : array) {
        result.append(entry.toString());
    }
    return result;
}

} // namespace

class KWinInputAdapterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void serializesMultiCapabilityDevice();
    void normalizesRepresentativeEventFamilies();
    void reportsUnavailableObserverWithoutInput();
    void forwardsAllKeyboardInteractionBeginsWithoutInstalledInput();
    void arbitratesChromeAndKeyboardGrabsWithoutInstalledInput();
    void cancelInvalidatesPointerAndKeyboardTransactionsWithoutInstalledInput();
    void requiresBothMutationMarkers();
};

void KWinInputAdapterTest::serializesMultiCapabilityDevice()
{
    FakeInputDevice device;
    device.keyboard = true;
    device.pointer = true;
    device.touchpad = true;
    device.tabletTool = true;
    device.tabletPad = true;
    device.relativeTabletTool = true;
    device.setOutputName(QStringLiteral("Virtual-2"));

    const auto descriptor = describeInputDevice(device, QStringLiteral("input-00000002"));
    const auto capabilities = inputCapabilitiesJson(true, {descriptor});
    QCOMPARE(strings(capabilities.value(QStringLiteral("capabilities")).toArray()),
             QStringList({QStringLiteral("keyboard"), QStringLiteral("pointer"),
                          QStringLiteral("touchpad"), QStringLiteral("tablet-tool"),
                          QStringLiteral("tablet-pad")}));
    QCOMPARE(capabilities.value(QStringLiteral("observerActive")).toBool(), true);
    QCOMPARE(capabilities.value(QStringLiteral("consumesEvents")).toBool(), false);

    const auto devices = capabilities.value(QStringLiteral("devices")).toArray();
    QCOMPARE(devices.size(), 1);
    const auto serialized = devices.first().toObject();
    QCOMPARE(serialized.value(QStringLiteral("id")).toString(),
             QStringLiteral("input-00000002"));
    QCOMPARE(serialized.value(QStringLiteral("outputName")).toString(),
             QStringLiteral("Virtual-2"));
    QCOMPARE(serialized.value(QStringLiteral("relativeTabletTool")).toBool(), true);
    QCOMPARE(serialized.value(QStringLiteral("tabletPad")).toObject()
                 .value(QStringLiteral("strips")).toInt(),
             3);

    device.setEnabled(false);
    device.setOutputName(QStringLiteral("Virtual-3"));
    const auto refreshed = describeInputDevice(device, descriptor.id).toJson();
    QCOMPARE(refreshed.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(refreshed.value(QStringLiteral("outputName")).toString(),
             QStringLiteral("Virtual-3"));
}

void KWinInputAdapterTest::normalizesRepresentativeEventFamilies()
{
    FakeInputDevice device;
    KWin::PointerMotionEvent pointer{};
    pointer.device = &device;
    pointer.position = QPointF(100.5, 200.25);
    pointer.delta = QPointF(4.0, -2.0);
    pointer.deltaUnaccelerated = QPointF(3.0, -1.0);
    pointer.buttons = Qt::LeftButton;
    pointer.modifiers = Qt::AltModifier;
    pointer.modifiersRelevantForShortcuts = Qt::AltModifier;
    pointer.timestamp = 17us;
    const auto normalizedPointer = normalizeInputEvent(pointer, QStringLiteral("input-1"));
    QCOMPARE(static_cast<int>(normalizedPointer.kind),
             static_cast<int>(NormalizedInputEventKind::PointerMotion));
    QCOMPARE(normalizedPointer.position, QPointF(100.5, 200.25));
    QCOMPARE(normalizedPointer.unacceleratedDelta, QPointF(3.0, -1.0));
    QCOMPARE(normalizedPointer.timestampUsec, 17);

    KWin::KeyboardKeyEvent keyboard{};
    keyboard.device = &device;
    keyboard.state = KWin::KeyboardKeyState::Repeated;
    keyboard.key = Qt::Key_A;
    keyboard.nativeScanCode = 30;
    keyboard.text = QStringLiteral("sensitive text is deliberately not copied");
    keyboard.modifiers = Qt::ControlModifier;
    keyboard.modifiersRelevantForGlobalShortcuts = Qt::ControlModifier;
    keyboard.timestamp = 29us;
    keyboard.serial = 99;
    const auto normalizedKeyboard = normalizeInputEvent(keyboard, QStringLiteral("input-1"));
    QCOMPARE(static_cast<int>(normalizedKeyboard.state),
             static_cast<int>(NormalizedInputState::Repeated));
    QCOMPARE(normalizedKeyboard.logicalCode, static_cast<quint32>(Qt::Key_A));
    QCOMPARE(normalizedKeyboard.nativeCode, 0U);
    QCOMPARE(normalizedKeyboard.timestampUsec, 29);

    KWin::TouchDownEvent touch{7, QPointF(9.0, 11.0), 31us};
    const auto normalizedTouch = normalizeInputEvent(touch);
    QCOMPARE(normalizedTouch.contactId, 7);
    QCOMPARE(normalizedTouch.position, QPointF(9.0, 11.0));
    QVERIFY(normalizedTouch.deviceId.isEmpty());

    FakeTabletTool tool;
    KWin::TabletToolAxisEvent tablet{};
    tablet.device = &device;
    tablet.position = QPointF(12.0, 13.0);
    tablet.pressure = 0.75;
    tablet.xTilt = 0.1;
    tablet.yTilt = -0.2;
    tablet.timestamp = 37us;
    tablet.tool = &tool;
    const auto normalizedTablet = normalizeInputEvent(tablet, QStringLiteral("input-1"));
    QCOMPARE(normalizedTablet.pressure, 0.75);
    QCOMPARE(normalizedTablet.tabletToolSerialId, 23U);
    QCOMPARE(normalizedTablet.tabletToolUniqueId, 42U);
    QVERIFY((normalizedTablet.tabletToolCapabilities & TabletToolPressure) != 0U);
    QVERIFY((normalizedTablet.tabletToolCapabilities & TabletToolTilt) != 0U);
}

void KWinInputAdapterTest::reportsUnavailableObserverWithoutInput()
{
    KWinInputAdapter adapter(nullptr);
    QVERIFY(!adapter.observerActive());
    const auto capabilities = adapter.capabilitiesJson();
    QCOMPARE(capabilities.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(capabilities.value(QStringLiteral("observerActive")).toBool(), false);
    QCOMPARE(capabilities.value(QStringLiteral("devices")).toArray().size(), 0);
}

void KWinInputAdapterTest::forwardsAllKeyboardInteractionBeginsWithoutInstalledInput()
{
    EmptyInteractionResolver resolver;
    HybridInput::InteractionController controller(resolver);
    QVector<HybridInput::InteractionIntent> intents;
    KWinInteractionFilter filter(
        nullptr, controller,
        [&](const HybridInput::InteractionIntent &intent) { intents.append(intent); });
    QVERIFY(!filter.installed());

    const auto verify = [&](bool acquired, HybridInput::InteractionKind kind) {
        QVERIFY(acquired);
        QCOMPARE(intents.constLast().phase, HybridInput::IntentPhase::Begin);
        QCOMPARE(intents.constLast().kind, kind);
        filter.cancel();
        QCOMPARE(intents.constLast().phase, HybridInput::IntentPhase::Cancel);
    };
    verify(filter.beginKeyboardDock(
               {HybridInput::HitKind::MemberTitle, {}, QStringLiteral("window"), {}}),
           HybridInput::InteractionKind::MemberDock);
    verify(filter.beginKeyboardMove(
               {HybridInput::HitKind::OuterTitle, QStringLiteral("group"), {}, {}}),
           HybridInput::InteractionKind::ContainerMove);
    verify(filter.beginKeyboardDividerResize(
               {HybridInput::HitKind::Divider, QStringLiteral("group"), {},
                QStringLiteral("split")}),
           HybridInput::InteractionKind::DividerResize);
    verify(filter.beginKeyboardContainerResize(
               {HybridInput::HitKind::OuterResize, QStringLiteral("group"), {}, {},
                Qt::RightEdge | Qt::BottomEdge}),
           HybridInput::InteractionKind::ContainerResize);

    const auto count = intents.size();
    QVERIFY(!filter.beginKeyboardContainerResize(
        {HybridInput::HitKind::OuterResize, QStringLiteral("group"), {}, {}, {}}));
    QCOMPARE(intents.size(), count);
}

void KWinInputAdapterTest::arbitratesChromeAndKeyboardGrabsWithoutInstalledInput()
{
    EmptyInteractionResolver resolver;
    HybridInput::InteractionController controller(resolver);
    HybridChromePointerRouter chrome(
        [](const QPointF &) -> std::optional<ChromePointerHit> {
            return ChromePointerHit{
                QStringLiteral("group"),
                {HybridChrome::HitKind::OuterTitleDrag,
                 QStringLiteral("group"), -1, std::nullopt, {}}};
        });
    KWinInteractionFilter filter(nullptr, controller, {}, &chrome, {});

    const auto acquired = chrome.pointerPress(
        {.position = {10.0, 10.0},
         .changedButton = Qt::LeftButton,
         .buttons = Qt::LeftButton,
         .modifiers = Qt::NoModifier});
    QVERIFY(acquired.consumed);
    QVERIFY(chrome.active());
    QVERIFY(!filter.beginKeyboardMove(
        {HybridInput::HitKind::OuterTitle, QStringLiteral("group"), {}, {}}));

    filter.cancelChrome();
    QVERIFY(!chrome.active());
    QVERIFY(filter.beginKeyboardMove(
        {HybridInput::HitKind::OuterTitle, QStringLiteral("group"), {}, {}}));
    filter.cancel();
}

void KWinInputAdapterTest::cancelInvalidatesPointerAndKeyboardTransactionsWithoutInstalledInput()
{
    EmptyInteractionResolver resolver;
    resolver.hit = {HybridInput::HitKind::OuterTitle,
                    QStringLiteral("group"), {}, {}};
    HybridInput::InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVector<HybridInput::InteractionIntent> intents;
    KWinInteractionFilter filter(
        nullptr, controller,
        [&](const HybridInput::InteractionIntent &intent) { intents.append(intent); });

    QVERIFY(controller.pointerPress(
        {.position = {10.0, 10.0},
         .changedButton = Qt::LeftButton,
         .buttons = Qt::LeftButton,
         .modifiers = Qt::MetaModifier | Qt::ShiftModifier})
                .consumed);
    const auto activated = controller.pointerMove({.position = {20.0, 10.0}});
    QCOMPARE(activated.intents.constFirst().phase,
             HybridInput::IntentPhase::Begin);
    QVERIFY(controller.active());

    filter.cancel();
    QCOMPARE(intents.size(), 1);
    QCOMPARE(intents.constFirst().phase, HybridInput::IntentPhase::Cancel);
    QVERIFY(!controller.active());
    const auto staleRelease = controller.pointerRelease(
        {.position = {30.0, 10.0},
         .changedButton = Qt::LeftButton,
         .buttons = Qt::NoButton,
         .modifiers = Qt::MetaModifier | Qt::ShiftModifier});
    QVERIFY(!staleRelease.consumed);
    QVERIFY(staleRelease.intents.isEmpty());

    intents.clear();
    QVERIFY(filter.beginKeyboardMove(resolver.hit));
    QCOMPARE(intents.size(), 1);
    QCOMPARE(intents.constFirst().phase, HybridInput::IntentPhase::Begin);
    filter.cancel();
    QCOMPARE(intents.size(), 2);
    QCOMPARE(intents.constLast().phase, HybridInput::IntentPhase::Cancel);
    QVERIFY(!controller.active());
    const auto staleCommit = controller.keyEvent({.key = Qt::Key_Enter});
    QVERIFY(!staleCommit.consumed);
    QVERIFY(staleCommit.intents.isEmpty());

    filter.cancel();
    QCOMPARE(intents.size(), 2);
}

void KWinInputAdapterTest::requiresBothMutationMarkers()
{
    QVERIFY(!mutationsEnabledForSession({}, {}));
    QVERIFY(!mutationsEnabledForSession(QByteArrayLiteral("1"), {}));
    QVERIFY(!mutationsEnabledForSession({}, QByteArrayLiteral("scenario.json")));
    QVERIFY(!mutationsEnabledForSession(QByteArrayLiteral("true"),
                                        QByteArrayLiteral("scenario.json")));
    QVERIFY(mutationsEnabledForSession(QByteArrayLiteral("1"),
                                       QByteArrayLiteral("scenario.json")));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        {}, {}, QByteArrayLiteral("virtual")));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        QByteArrayLiteral("1"), {}, QByteArrayLiteral("virtual")));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        {}, QByteArrayLiteral("scenario.json"), QByteArrayLiteral("virtual")));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        QByteArrayLiteral("true"), QByteArrayLiteral("scenario.json"),
        QByteArrayLiteral("virtual")));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        QByteArrayLiteral("1"), QByteArrayLiteral("scenario.json"), {}));
    QVERIFY(!developmentVirtualOutputsEnabledForSession(
        QByteArrayLiteral("1"), QByteArrayLiteral("scenario.json"),
        QByteArrayLiteral("wayland")));
    QVERIFY(developmentVirtualOutputsEnabledForSession(
        QByteArrayLiteral("1"), QByteArrayLiteral("scenario.json"),
        QByteArrayLiteral("virtual")));
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::KWinInputAdapterTest)

#include "tst_kwininputadapter.moc"
