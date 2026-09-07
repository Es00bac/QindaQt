// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/brightness_key_controller.h"

#include <QtGlobal>

#include <algorithm>

namespace QindaQt::Session::DesktopControls {
namespace {

int deviceTier(const Power::InternalBacklight &device) noexcept
{
    switch (device.kind) {
    case Power::BacklightKind::Firmware:
        return 0;
    case Power::BacklightKind::Platform:
        return 1;
    case Power::BacklightKind::Raw:
        return 2;
    }
    return 3;
}

int stepFor(quint32 maximum) noexcept
{
    const quint32 rawStep = maximum / 16U;
    return static_cast<int>(rawStep == 0U ? 1U : rawStep);
}

int percentOf(quint32 observed, quint32 maximum) noexcept
{
    if (maximum == 0U) {
        return 0;
    }
    return qRound(static_cast<double>(observed) * 100.0
                  / static_cast<double>(maximum));
}

} // namespace

BrightnessKeyController::BrightnessKeyController(
    Power::Upstream::SysfsBacklightSource &source, QObject *parent)
    : QObject(parent), m_source(source)
{
}

BrightnessKeyController::~BrightnessKeyController() = default;

void BrightnessKeyController::raiseBrightness()
{
    submitStep(1);
}

void BrightnessKeyController::lowerBrightness()
{
    submitStep(-1);
}

void BrightnessKeyController::submitStep(int direction)
{
    const auto *device = preferredDevice();
    if (device == nullptr) {
        Q_EMIT brightnessUnavailable(QStringLiteral("no-backlight-device"));
        return;
    }
    const int step = stepFor(device->maximum);
    // AGENT-GUARD: the raw floor is 1, never 0 — a media key must keep the
    // panel readable; full-off is the idle policy's DPMS concern.
    const int next = static_cast<int>(device->observed) + direction * step;
    const int clamped = qBound(1, next, static_cast<int>(device->maximum));
    const auto outcome = m_source.writeBrightness(
        device->handle.opaqueId, static_cast<quint32>(clamped));
    if (outcome.status != Power::Upstream::BacklightWriteStatus::Succeeded) {
        Q_EMIT brightnessUnavailable(outcome.reasonCode);
        return;
    }
    Q_EMIT brightnessFeedbackRequested(percentOf(static_cast<quint32>(clamped),
                                                 device->maximum));
}

const Power::InternalBacklight *BrightnessKeyController::preferredDevice() const
{
    const auto &devices = m_source.devices();
    const Power::InternalBacklight *preferred = nullptr;
    for (const auto &device : devices) {
        // AGENT-NOTE: the sysfs adapter stamps only the opaque id; the
        // resident service restamps epochs at publication, which this local
        // writer never sees. The write primitive matches by opaque id alone.
        // A read-only device is still selectable: the write returns the typed
        // "backlight-read-only" truth instead of hiding the control.
        if (!device.observedKnown || device.maximum == 0U
            || device.handle.opaqueId.isEmpty()) {
            continue;
        }
        if (preferred == nullptr
            || (deviceTier(device) < deviceTier(*preferred)
                || (deviceTier(device) == deviceTier(*preferred)
                    && device.deviceName < preferred->deviceName))) {
            preferred = &device;
        }
    }
    return preferred;
}

} // namespace QindaQt::Session::DesktopControls
