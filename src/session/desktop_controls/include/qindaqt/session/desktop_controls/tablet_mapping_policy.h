// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_orientation.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QObject>
#include <QString>

namespace QindaQt::Session::DesktopControls {

using Services::TabletDevices::TabletOutputInventory;

using Services::TabletDevices::TabletMappingStore;

// Session policy: a pen display maps itself to its own screen, exactly once,
// and never argues with a choice the user made. Every pass also keeps each
// tablet's rotation and areas right for the screen it reaches (ADR-0285): a
// desk tablet's up stays the screen's up when that screen is rotated, and a
// pen display carries no stale rotation of its own.
//
// AGENT-CONTRACT: This class writes KWin device properties and the mapping
// ledger; it draws nothing and knows nothing about notifications. It reports
// `tabletAnnounced` once per device group so a presenter can post one
// notification, and records that it did so in the ledger, so a re-plug of a
// known tablet is silent. Output rotations arrive through `outputs` (the
// Display1 decorator in production); an output change therefore re-plans
// every tablet, and an unknown rotation plans nothing.
//
// AGENT-GUARD: A record with `userChosen` is never replaced by automatic
// matching. The whole promise of the feature is that it asks once; a second
// opinion later would move the user's pen out from under their hand.
class TabletMappingPolicy final : public QObject {
    Q_OBJECT
public:
    TabletMappingPolicy(const Services::TabletDevices::TabletDevicePort &port,
                        Services::TabletDevices::TabletDeviceWatcher &watcher,
                        TabletOutputInventory &outputs,
                        TabletMappingStore &store,
                        QObject *parent = nullptr);
    ~TabletMappingPolicy() override;

    // Subscribes to hotplug, output and ledger changes and reconciles once.
    // Returns false with a diagnostic when hotplug cannot be observed; the
    // policy still reconciles what is present.
    bool start(QString *error = nullptr);

    // Re-reads every tablet and applies the ledger and the matcher. Safe to
    // call at any time; it writes only properties whose value differs.
    void reconcile();

    // Records an explicit user decision for one device group and applies it
    // immediately. Automatic matching never overrides it afterwards.
    bool applyUserChoice(const QString &deviceGroupId,
                         Services::TabletDevices::TabletMapChoice choice,
                         const QString &outputName, QString *error = nullptr);

    [[nodiscard]] QString lastError() const { return m_lastError; }

Q_SIGNALS:
    // `outputName` is empty when the tablet has no screen of its own and
    // keeps following the active output.
    void tabletAnnounced(const QString &deviceGroupId,
                         const QString &deviceName, const QString &outputName);
    // A write the authority refused. The policy keeps the ledger unchanged.
    void mappingFailed(const QString &message);

private:
    struct GroupState {
        QString deviceGroupId;
        QString deviceName;
        QStringList toolDeviceIds;
        bool hasPad = false;
    };

    void handleDeviceAdded(const QString &deviceId);
    void handleDeviceRemoved(const QString &deviceId);
    [[nodiscard]] bool applyChoice(
        const GroupState &group,
        Services::TabletDevices::TabletMapChoice choice,
        const QString &outputName,
        const QList<Services::TabletDevices::TabletDeviceSnapshot> &devices,
        QString *error);
    // Writes each tool's planned rotation and areas for the mapping just
    // applied and completes `intent` with what a desk tablet adopted, so the
    // caller records it with the mapping decision.
    [[nodiscard]] bool applyPlacement(
        const GroupState &group,
        Services::TabletDevices::TabletMapChoice choice,
        const QString &outputName,
        const QList<Services::TabletDevices::TabletDeviceSnapshot> &devices,
        const QList<Services::TabletDevices::TabletOutputCandidate> &outputs,
        Services::TabletDevices::TabletPlacementIntent *intent,
        QString *error);
    void fail(const QString &message);

    const Services::TabletDevices::TabletDevicePort &m_port;
    Services::TabletDevices::TabletDeviceWatcher &m_watcher;
    TabletOutputInventory &m_outputs;
    TabletMappingStore &m_store;
    QString m_lastError;
    bool m_reconciling = false;
};

} // namespace QindaQt::Session::DesktopControls
