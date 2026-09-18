// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QList>
#include <QString>

namespace QindaQt::Services::TabletDevices {

// One output as the matcher needs to see it. `connectorName` is what KWin's
// `outputName` property takes (for example "HDMI-A-1"); `manufacturer` and
// `model` are the display identity KWin decoded from the EDID.
struct TabletOutputCandidate {
    QString connectorName;
    QString manufacturer;
    QString model;
    QString label;
    bool internal = false;
    bool enabled = true;

    friend bool operator==(const TabletOutputCandidate &,
                           const TabletOutputCandidate &) = default;
};

enum class TabletMatchReason {
    // No output could be named; the device keeps KWin's default (the active
    // output), which is the honest outcome for an opaque tablet.
    NoMatch,
    // The output's EDID manufacturer is a display-tablet vendor.
    DisplayTabletVendor,
    // The output's model or label shares a distinctive word with the
    // tablet's product name.
    ProductName,
    // More than one output matched equally well; auto-mapping must not
    // guess, so the user chooses.
    Ambiguous,
};

struct TabletOutputMatch {
    TabletMatchReason reason = TabletMatchReason::NoMatch;
    QString connectorName;

    [[nodiscard]] bool decided() const noexcept {
        return reason == TabletMatchReason::DisplayTabletVendor ||
               reason == TabletMatchReason::ProductName;
    }
};

// Pure policy: pick the output a pen display's own screen is, if any.
//
// AGENT-CONTRACT: This decides nothing about writing. It answers "which
// output is this tablet's own screen" and says why. Exactly one candidate
// must win; two equally good candidates are Ambiguous, never a coin flip.
// Internal panels are never a display-tablet match by vendor, because the
// laptop's own panel is the wrong answer the user already lived through.
[[nodiscard]] TabletOutputMatch
matchTabletOutput(const TabletDeviceSnapshot &device,
                  const QList<TabletOutputCandidate> &outputs);

// True when `manufacturer` names a vendor that ships display tablets, under
// either the raw EDID PNP code ("WAC") or the decoded vendor string
// ("Wacom Technology Corp."). Exported for the Display route's badge.
[[nodiscard]] bool isDisplayTabletVendor(const QString &manufacturer);

} // namespace QindaQt::Services::TabletDevices
