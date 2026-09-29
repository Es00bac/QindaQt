// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QList>

namespace QindaQt::Services::TabletDevices {

// Whether a tablet's surface is a screen (ADR-0285).
enum class TabletKind {
    // The pen moves over a surface on the desk; its directions are the
    // user's to choose, and a rotated monitor must not turn them.
    DeskTablet,
    // The pen draws on a screen (a pen display or a laptop's own pen
    // digitizer); it turns with that screen's rotation and nothing else.
    PenDisplay,
};

// Which evidence decided, strongest first. Exported so the rules can be
// pinned by tests and so a caller can say how the decision was made.
enum class TabletKindEvidence {
    // libinput's own verdict. libinput offers a tablet-area rectangle only
    // for a tablet whose evdev node is NOT INPUT_PROP_DIRECT, after its quirks
    // database has corrected the kernel's properties (libinput 1.31
    // evdev-tablet.c tablet_init_area; evdev.c applies AttrInputProp first),
    // and KWin publishes exactly that as `supportsInputArea`.
    LibinputDirectness,
    // The output matcher found the tablet's own screen (ADR-0197).
    OwnScreen,
    // Nothing decided. A tablet without a screen of its own is a desk tablet.
    Default,
};

struct TabletClassification {
    TabletKind kind = TabletKind::DeskTablet;
    TabletKindEvidence evidence = TabletKindEvidence::Default;

    friend bool operator==(const TabletClassification &,
                           const TabletClassification &) = default;
};

// Pure policy: is this tablet tool a pen display or a desk tablet?
//
// AGENT-NOTE: libwacom's WACOM_DEVICE_INTEGRATED_DISPLAY flag would be the
// canonical source, but neither host has libwacom and dev-libs/libinput is
// built without it (ADR-0285). libinput's directness verdict is the next most
// authoritative source and needs no new dependency: KWin 6.6 requires libinput
// 1.28, whose area support exists exactly for indirect tablets. The output
// matcher decides only when KWin did not publish `supportsInputArea`.
[[nodiscard]] TabletClassification
classifyTablet(const TabletDeviceSnapshot &tool,
               const QList<TabletOutputCandidate> &outputs);

} // namespace QindaQt::Services::TabletDevices
