// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_surface.h>

#include <QStringList>

namespace QindaQt::Apps::SettingsInput {

using Services::TabletDevices::TabletMapChoice;
using Services::TabletDevices::TabletOutputCandidate;

namespace {

bool drawable(const TabletOutputCandidate &output) {
    return output.enabled && !output.connectorName.isEmpty() &&
           output.logicalGeometry.width() > 0.0 &&
           output.logicalGeometry.height() > 0.0;
}

TabletSurface singleScreen(const TabletOutputCandidate &output) {
    TabletSurface surface;
    surface.width = output.logicalGeometry.width();
    surface.height = output.logicalGeometry.height();
    surface.screens.append(
        TabletSurfaceScreen{QRectF(0.0, 0.0, 1.0, 1.0), output.connectorName});
    return surface;
}

TabletSurface workspace(const QList<TabletOutputCandidate> &outputs) {
    QRectF bounds;
    for (const TabletOutputCandidate &output : outputs) {
        if (drawable(output)) {
            bounds = bounds.united(output.logicalGeometry);
        }
    }
    if (!(bounds.width() > 0.0) || !(bounds.height() > 0.0)) {
        return TabletSurface{};
    }
    TabletSurface surface;
    surface.width = bounds.width();
    surface.height = bounds.height();
    for (const TabletOutputCandidate &output : outputs) {
        if (!drawable(output)) {
            continue;
        }
        const QRectF &geometry = output.logicalGeometry;
        surface.screens.append(TabletSurfaceScreen{
            QRectF((geometry.x() - bounds.x()) / bounds.width(),
                   (geometry.y() - bounds.y()) / bounds.height(),
                   geometry.width() / bounds.width(),
                   geometry.height() / bounds.height()),
            output.connectorName});
    }
    return surface;
}

} // namespace

TabletSurface tabletSurfaceFor(TabletMapChoice choice,
                               const QString &outputName,
                               const QList<TabletOutputCandidate> &outputs) {
    switch (choice) {
    case TabletMapChoice::EntireWorkspace:
        return workspace(outputs);
    case TabletMapChoice::NamedOutput:
        for (const TabletOutputCandidate &output : outputs) {
            if (drawable(output) && !outputName.isEmpty() &&
                output.connectorName == outputName) {
                return singleScreen(output);
            }
        }
        // Not present: KWin routes the pen to the active output instead.
        break;
    case TabletMapChoice::FollowActiveScreen:
        break;
    }
    const TabletOutputCandidate *largest = nullptr;
    for (const TabletOutputCandidate &output : outputs) {
        if (!drawable(output)) {
            continue;
        }
        const double area =
            output.logicalGeometry.width() * output.logicalGeometry.height();
        if (largest == nullptr || area > largest->logicalGeometry.width() *
                                             largest->logicalGeometry.height()) {
            largest = &output;
        }
    }
    return largest != nullptr ? singleScreen(*largest) : TabletSurface{};
}

QString tabletOutputLabel(const TabletOutputCandidate &output) {
    QStringList parts;
    if (!output.manufacturer.trimmed().isEmpty()) {
        parts.append(output.manufacturer.trimmed());
    }
    if (!output.model.trimmed().isEmpty() &&
        output.model.trimmed() != output.connectorName) {
        parts.append(output.model.trimmed());
    }
    if (parts.isEmpty()) {
        return output.connectorName;
    }
    return QStringLiteral("%1 (%2)")
        .arg(parts.join(QLatin1Char(' ')), output.connectorName);
}

} // namespace QindaQt::Apps::SettingsInput
