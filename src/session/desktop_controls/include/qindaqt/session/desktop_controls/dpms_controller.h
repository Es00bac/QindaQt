// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Seam over display power control. Production drives org-kde-kwin-dpms on the
// session Wayland display; the compositor itself wakes outputs on input.
class DpmsController : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~DpmsController() override = default;

    DpmsController(const DpmsController &) = delete;
    DpmsController &operator=(const DpmsController &) = delete;

    // Honest capability truth observed at bind time. A controller without a
    // bound manager keeps the idle policy passive instead of guessing.
    [[nodiscard]] virtual bool available() const = 0;
    virtual void requestDisplaysOff() = 0;
    virtual void requestDisplaysOn() = 0;

Q_SIGNALS:
    void displaysPowerChanged(bool off);
    // Fired when the ability to control display power first appears or is
    // lost (late global bind, hotplug, connection death). The idle policy
    // re-applies the current preference on recovery so an unavailable-at-
    // start controller cannot leave the policy permanently passive.
    void availabilityChanged(bool available);
};

} // namespace QindaQt::Session::DesktopControls
