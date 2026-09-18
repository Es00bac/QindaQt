// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QObject>

namespace QindaQt::Services::TabletDevices {

// The outputs a process currently drives, as tablet mapping needs to see
// them. Implementations publish a change whenever an output appears,
// disappears or changes identity, so a pen plugged in before its screen is
// mapped as soon as the screen arrives.
class TabletOutputInventory : public QObject {
    Q_OBJECT
public:
    explicit TabletOutputInventory(QObject *parent = nullptr);
    ~TabletOutputInventory() override;

    [[nodiscard]] virtual QList<TabletOutputCandidate> outputs() const = 0;

Q_SIGNALS:
    void outputsChanged();
};

// Output inventory over the platform screens the process already has.
//
// AGENT-NOTE: QScreen carries the same EDID identity Display1 publishes —
// KWin fills a Wayland output's make/model from the EDID and Qt exposes them
// as manufacturer()/model(), with name() being the connector ("HDMI-A-1"),
// which is exactly what KWin's `outputName` property takes. Reading them
// here keeps both the session process and the Settings route free of a
// Display1 client for a decision that needs one string per output.
//
// AGENT-GUARD: An internal panel must be recognizable, because the pen must
// never be auto-mapped to the laptop's own screen. Qt exposes no "internal"
// flag, so the connector prefix decides (eDP/LVDS/DSI).
class ScreenTabletOutputs final : public TabletOutputInventory {
    Q_OBJECT
public:
    explicit ScreenTabletOutputs(QObject *parent = nullptr);
    ~ScreenTabletOutputs() override;

    [[nodiscard]] QList<TabletOutputCandidate> outputs() const override;

    [[nodiscard]] static bool isInternalConnector(const QString &connectorName);
};

} // namespace QindaQt::Services::TabletDevices
