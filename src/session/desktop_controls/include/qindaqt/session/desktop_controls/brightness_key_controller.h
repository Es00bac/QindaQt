// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Media-key brightness over the public SysfsBacklightSource write primitive
// (ADR-0060 building block; Power1 v1 intentionally has no wire method). The
// controller picks one eligible device using the kernel type preference
// firmware > platform > raw and applies one sixteenth of the raw maximum per
// trigger, clamped to [1, maximum]. A read-only or absent device reports
// honest unavailability; nothing is escalated or retried.
class BrightnessKeyController final : public QObject {
    Q_OBJECT

public:
    explicit BrightnessKeyController(Power::Upstream::SysfsBacklightSource &source,
                                     QObject *parent = nullptr);
    ~BrightnessKeyController() override;

    BrightnessKeyController(const BrightnessKeyController &) = delete;
    BrightnessKeyController &operator=(const BrightnessKeyController &) = delete;

    void raiseBrightness();
    void lowerBrightness();

Q_SIGNALS:
    // `percent` is 0..100 of the device's raw maximum.
    void brightnessFeedbackRequested(int percent);
    void brightnessUnavailable(const QString &reasonCode);

private:
    void submitStep(int delta);
    [[nodiscard]] const Power::InternalBacklight *preferredDevice() const;

    Power::Upstream::SysfsBacklightSource &m_source;
};

} // namespace QindaQt::Session::DesktopControls
