// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QGuiApplication>
#include <QScreen>

namespace QindaQt::Services::TabletDevices {

TabletOutputInventory::TabletOutputInventory(QObject *parent)
    : QObject(parent) {}

TabletOutputInventory::~TabletOutputInventory() = default;

ScreenTabletOutputs::ScreenTabletOutputs(QObject *parent)
    : TabletOutputInventory(parent) {
    if (auto *application = qGuiApp) {
        connect(application, &QGuiApplication::screenAdded, this,
                [this](QScreen *screen) {
                    watchScreen(screen);
                    Q_EMIT outputsChanged();
                });
        connect(application, &QGuiApplication::screenRemoved, this,
                &TabletOutputInventory::outputsChanged);
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            watchScreen(screen);
        }
    }
}

ScreenTabletOutputs::~ScreenTabletOutputs() = default;

void ScreenTabletOutputs::watchScreen(QScreen *screen) {
    if (screen == nullptr) {
        return;
    }
    // A rotated, rescaled or moved screen changes the shape and place the
    // area editor draws (ADR-0285). The connection dies with the screen.
    connect(screen, &QScreen::geometryChanged, this,
            &TabletOutputInventory::outputsChanged);
}

bool ScreenTabletOutputs::isInternalConnector(const QString &connectorName) {
    static const QStringList prefixes{
        QStringLiteral("eDP"), QStringLiteral("LVDS"), QStringLiteral("DSI")};
    for (const QString &prefix : prefixes) {
        if (connectorName.startsWith(prefix, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QList<TabletOutputCandidate> ScreenTabletOutputs::outputs() const {
    QList<TabletOutputCandidate> candidates;
    const QList<QScreen *> screens = QGuiApplication::screens();
    candidates.reserve(screens.size());
    for (const QScreen *screen : screens) {
        if (screen == nullptr || screen->name().isEmpty()) {
            continue;
        }
        candidates.append(TabletOutputCandidate{
            .connectorName = screen->name(),
            .manufacturer = screen->manufacturer(),
            .model = screen->model(),
            // Qt adds no separate label; the model is the only human-facing
            // string beyond the connector the matcher already has.
            .label = screen->model(),
            .internal = isInternalConnector(screen->name()),
            .enabled = true,
            // QScreen cannot say how KWin turns pen input; the Display1
            // decorator adds the rotation (display_tablet_outputs.h).
            .rotation = std::nullopt,
            .logicalGeometry = QRectF(screen->geometry()),
        });
    }
    return candidates;
}

} // namespace QindaQt::Services::TabletDevices
