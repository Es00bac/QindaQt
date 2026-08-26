// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwininputadapter.h"

#include "inputcapabilities.h"

#include <core/inputdevice.h>
#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>

#include <QList>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

class KWinInputAdapter::EventSpy final : public KWin::InputEventSpy
{
public:
    explicit EventSpy(KWinInputAdapter &owner)
        : m_owner(owner)
    {
    }

    void pointerMotion(KWin::PointerMotionEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void pointerButton(KWin::PointerButtonEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void pointerAxis(KWin::PointerAxisEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void touchDown(KWin::TouchDownEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void touchMotion(KWin::TouchMotionEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void touchUp(KWin::TouchUpEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void pinchGestureBegin(KWin::PointerPinchGestureBeginEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void pinchGestureUpdate(KWin::PointerPinchGestureUpdateEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void pinchGestureEnd(KWin::PointerPinchGestureEndEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void pinchGestureCancelled(KWin::PointerPinchGestureCancelEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void swipeGestureBegin(KWin::PointerSwipeGestureBeginEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void swipeGestureUpdate(KWin::PointerSwipeGestureUpdateEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void swipeGestureEnd(KWin::PointerSwipeGestureEndEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void swipeGestureCancelled(KWin::PointerSwipeGestureCancelEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void holdGestureBegin(KWin::PointerHoldGestureBeginEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void holdGestureEnd(KWin::PointerHoldGestureEndEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void holdGestureCancelled(KWin::PointerHoldGestureCancelEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event));
        }
    }

    void switchEvent(KWin::SwitchEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletToolProximityEvent(KWin::TabletToolProximityEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletToolAxisEvent(KWin::TabletToolAxisEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletToolTipEvent(KWin::TabletToolTipEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletToolButtonEvent(KWin::TabletToolButtonEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletPadButtonEvent(KWin::TabletPadButtonEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletPadStripEvent(KWin::TabletPadStripEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletPadRingEvent(KWin::TabletPadRingEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

    void tabletPadDialEvent(KWin::TabletPadDialEvent *event) override
    {
        if (event) {
            m_owner.observe(normalizeInputEvent(*event, m_owner.deviceId(event->device)));
        }
    }

private:
    KWinInputAdapter &m_owner;
};

KWinInputAdapter::KWinInputAdapter(KWin::InputRedirection *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
{
    if (!m_input) {
        return;
    }

    connect(m_input, &KWin::InputRedirection::deviceAdded,
            this, &KWinInputAdapter::trackDevice);
    connect(m_input, &KWin::InputRedirection::deviceRemoved,
            this, &KWinInputAdapter::untrackDevice);
    connect(m_input, &QObject::destroyed, this, [this] {
        m_input = nullptr;
        m_observerActive = false;
        m_deviceIds.clear();
        Q_EMIT capabilitiesChanged();
    });
    for (auto *device : m_input->devices()) {
        trackDevice(device);
    }

    m_spy = std::make_unique<EventSpy>(*this);
    // AGENT-CONTRACT: InputEventSpy observes before KWin filters and has no
    // consume return value. Docking policy must remain outside this adapter.
    m_input->installInputEventSpy(m_spy.get());
    m_observerActive = true;
}

KWinInputAdapter::~KWinInputAdapter()
{
    // AGENT-GUARD: KWin's InputEventSpy destructor unregisters itself. Destroy
    // it while callback state is intact and, during normal plugin unload, while
    // InputRedirection is still alive.
    m_spy.reset();
    m_observerActive = false;
    m_input = nullptr;
}

bool KWinInputAdapter::observerActive() const
{
    return m_observerActive;
}

QJsonObject KWinInputAdapter::capabilitiesJson() const
{
    QList<InputDeviceDescriptor> devices;
    devices.reserve(m_deviceIds.size());
    for (auto iterator = m_deviceIds.cbegin(); iterator != m_deviceIds.cend(); ++iterator) {
        if (iterator.key()) {
            devices.append(describeInputDevice(*iterator.key(), iterator.value()));
        }
    }
    return inputCapabilitiesJson(m_observerActive, std::move(devices));
}

void KWinInputAdapter::trackDevice(KWin::InputDevice *device)
{
    if (!device || m_deviceIds.contains(device)) {
        return;
    }
    const auto id = QStringLiteral("input-%1").arg(m_nextDeviceId++, 8, 10, QLatin1Char('0'));
    m_deviceIds.insert(device, id);
    connect(device, &QObject::destroyed, this, [this, device] {
        untrackDevice(device);
    });
    Q_EMIT capabilitiesChanged();
}

void KWinInputAdapter::untrackDevice(KWin::InputDevice *device)
{
    if (m_deviceIds.remove(device) > 0) {
        Q_EMIT capabilitiesChanged();
    }
}

QString KWinInputAdapter::deviceId(KWin::InputDevice *device)
{
    if (!device) {
        return {};
    }
    if (!m_deviceIds.contains(device)) {
        trackDevice(device);
    }
    return m_deviceIds.value(device);
}

void KWinInputAdapter::observe(NormalizedInputEvent event)
{
    // AGENT-NOTE: KWin 6.6.5 touch and gesture spy events carry no device
    // pointer. Their normalized deviceId intentionally remains empty.
    Q_EMIT inputEventObserved(event);
}

} // namespace QindaQt::Compositor::KWinIntegration
