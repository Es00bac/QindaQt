// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QList>
#include <QRectF>
#include <QString>

namespace QindaQt::Apps::SettingsInput {

// One screen outlined inside a surface, normalized to that surface.
struct TabletSurfaceScreen {
    QRectF area;
    QString label;

    friend bool operator==(const TabletSurfaceScreen &,
                           const TabletSurfaceScreen &) = default;
};

// The surface a tablet's output area is a rectangle of, as the user sees it
// (ADR-0285): one screen, or the bounding box of every screen for a tablet
// that spans the workspace, with each screen outlined inside it.
struct TabletSurface {
    // Logical pixels; 0 when no screen reported a geometry, so the editor
    // can draw a neutral shape and refuse to compute proportions from it.
    double width = 0.0;
    double height = 0.0;
    QList<TabletSurfaceScreen> screens;

    friend bool operator==(const TabletSurface &,
                           const TabletSurface &) = default;
};

// Pure: the surface for a mapping.
//
// AGENT-NOTE: follows KWin's tabletToolPosition branches exactly like
// Services::TabletDevices::mappedRotation(): the workspace is the bounding
// box of every enabled output; a named output that is present is that
// output; otherwise KWin uses the active output, which changes per pen event,
// so the largest screen stands in for it.
[[nodiscard]] TabletSurface
tabletSurfaceFor(Services::TabletDevices::TabletMapChoice choice,
                 const QString &outputName,
                 const QList<Services::TabletDevices::TabletOutputCandidate>
                     &outputs);

// "Wacom Technology Corp. Wacom One 13 (HDMI-A-1)": the EDID identity with
// the connector the user may know it by. The connector alone when the EDID
// says nothing.
[[nodiscard]] QString
tabletOutputLabel(const Services::TabletDevices::TabletOutputCandidate &output);

} // namespace QindaQt::Apps::SettingsInput
