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
                &TabletOutputInventory::outputsChanged);
        connect(application, &QGuiApplication::screenRemoved, this,
                &TabletOutputInventory::outputsChanged);
    }
}

ScreenTabletOutputs::~ScreenTabletOutputs() = default;

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
        });
    }
    return candidates;
}

} // namespace QindaQt::Services::TabletDevices
