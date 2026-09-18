// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/desktop_controls/tablet_mapping_policy.h"

#include <QHash>
#include <QScopeGuard>

namespace QindaQt::Session::DesktopControls {

using Services::TabletDevices::TabletDeviceSnapshot;
using Services::TabletDevices::TabletMapChoice;
using Services::TabletDevices::TabletMappingLedger;
using Services::TabletDevices::TabletMappingRecord;
using Services::TabletDevices::TabletOutputMatch;

namespace {

// AGENT-GUARD: the ledger key is the STABLE identity, never KWin's
// `deviceGroupId` — that one hashes the libinput device group's pointer
// address and changes on every re-plug, which would make a known tablet look
// new, announce again, and lose the user's recorded choice.
QString groupKey(const TabletDeviceSnapshot &device) {
    return Services::TabletDevices::tabletIdentity(device);
}

} // namespace

TabletMappingPolicy::TabletMappingPolicy(
    const Services::TabletDevices::TabletDevicePort &port,
    Services::TabletDevices::TabletDeviceWatcher &watcher,
    TabletOutputInventory &outputs, TabletMappingStore &store, QObject *parent)
    : QObject(parent), m_port(port), m_watcher(watcher), m_outputs(outputs),
      m_store(store) {}

TabletMappingPolicy::~TabletMappingPolicy() = default;

bool TabletMappingPolicy::start(QString *error) {
    connect(&m_watcher,
            &Services::TabletDevices::TabletDeviceWatcher::deviceAdded, this,
            &TabletMappingPolicy::handleDeviceAdded);
    connect(&m_watcher,
            &Services::TabletDevices::TabletDeviceWatcher::deviceRemoved, this,
            &TabletMappingPolicy::handleDeviceRemoved);
    // AGENT-NOTE: A pen is routinely plugged in over USB before its HDMI
    // output is up. The output signal is what maps it a moment later, so it
    // is as load-bearing as the hotplug signal.
    connect(&m_outputs, &TabletOutputInventory::outputsChanged, this,
            &TabletMappingPolicy::reconcile);
    connect(&m_store, &TabletMappingStore::ledgerChanged, this,
            &TabletMappingPolicy::reconcile);
    QString watchError;
    const bool watching = m_watcher.start(&watchError);
    if (!watching && error != nullptr) {
        *error = watchError;
    }
    reconcile();
    return watching;
}

void TabletMappingPolicy::handleDeviceAdded(const QString &deviceId) {
    Q_UNUSED(deviceId)
    reconcile();
}

void TabletMappingPolicy::handleDeviceRemoved(const QString &deviceId) {
    Q_UNUSED(deviceId)
    // Nothing is unwritten on removal: KWin persists the mapping by output
    // UUID and the ledger keeps the decision, so a re-plug lands silently on
    // the same screen.
    reconcile();
}

void TabletMappingPolicy::fail(const QString &message) {
    m_lastError = message;
    Q_EMIT mappingFailed(message);
}

void TabletMappingPolicy::reconcile() {
    // AGENT-GUARD: applyChoice() writes properties, and a store that
    // republishes on save would re-enter here mid-pass. One pass at a time.
    if (m_reconciling) {
        return;
    }
    // Waiting for the ledger is the only correct behavior before it loads:
    // acting now could override a user choice this process has not read yet.
    if (!m_store.isLoaded()) {
        return;
    }
    m_reconciling = true;
    const auto guard = qScopeGuard([this] { m_reconciling = false; });

    QString error;
    const QList<TabletDeviceSnapshot> devices = m_port.devices(&error);
    if (!error.isEmpty()) {
        fail(error);
        return;
    }

    QHash<QString, GroupState> groups;
    QStringList order;
    for (const TabletDeviceSnapshot &device : devices) {
        const QString key = groupKey(device);
        if (key.isEmpty()) {
            continue;
        }
        if (!groups.contains(key)) {
            groups.insert(key, GroupState{key, device.name, {}, false});
            order.append(key);
        }
        GroupState &group = groups[key];
        if (device.tabletTool) {
            group.toolDeviceIds.append(device.deviceId);
            // A group's readable name is its tool's name; a pad's name is
            // the one users do not recognize.
            group.deviceName = device.name;
        }
        if (device.tabletPad) {
            group.hasPad = true;
        }
    }

    const QList<Services::TabletDevices::TabletOutputCandidate> outputs =
        m_outputs.outputs();
    TabletMappingLedger ledger = m_store.ledger();
    bool ledgerChanged = false;

    for (const QString &key : order) {
        const GroupState &group = groups.value(key);
        if (group.toolDeviceIds.isEmpty()) {
            continue; // a pad with no pen maps nothing
        }
        TabletMappingRecord record = ledger.record(key);
        const bool known = ledger.contains(key);

        TabletMapChoice choice = record.choice;
        QString outputName = record.outputName;

        if (!known || !record.userChosen) {
            const TabletDeviceSnapshot *tool = nullptr;
            for (const TabletDeviceSnapshot &device : devices) {
                if (device.tabletTool && groupKey(device) == key) {
                    tool = &device;
                    break;
                }
            }
            const TabletOutputMatch match =
                tool != nullptr
                    ? Services::TabletDevices::matchTabletOutput(*tool, outputs)
                    : TabletOutputMatch{};
            if (match.decided()) {
                choice = TabletMapChoice::NamedOutput;
                outputName = match.connectorName;
            } else if (!known) {
                // AGENT-GUARD: adopt what KWin already has rather than
                // clearing it. A tablet this desktop has never seen may
                // still be mapped correctly — by KWin's own persisted
                // OutputUuid, or by the user in another tool — and forcing
                // it to the active screen would un-map a working pen.
                const QString existing =
                    tool != nullptr
                        ? tool->properties.value(QStringLiteral("outputName"))
                              .toString()
                        : QString();
                const bool existingWorkspace =
                    tool != nullptr &&
                    tool->properties.value(QStringLiteral("mapToWorkspace"))
                        .toBool();
                if (existingWorkspace) {
                    choice = TabletMapChoice::EntireWorkspace;
                    outputName.clear();
                } else if (!existing.isEmpty()) {
                    choice = TabletMapChoice::NamedOutput;
                    outputName = existing;
                } else {
                    choice = TabletMapChoice::FollowActiveScreen;
                    outputName.clear();
                }
            }
        }

        QString applyError;
        if (!applyChoice(group, choice, outputName, devices, &applyError)) {
            fail(applyError);
            continue;
        }

        // Announce a tablet the user has not been told about, and a known
        // tablet whose mapping actually changed — the USB-before-HDMI case,
        // where the first pass could only say "follows the active screen".
        const bool mappingChanged =
            known && (record.choice != choice ||
                      (choice == TabletMapChoice::NamedOutput &&
                       record.outputName != outputName));
        const bool announceNow = !record.announced || mappingChanged;
        TabletMappingRecord next = record;
        next.choice = choice;
        next.outputName =
            choice == TabletMapChoice::NamedOutput ? outputName : QString();
        next.deviceName = group.deviceName;
        next.announced = true;
        // A matched output is an automatic decision; it must stay
        // overridable by the user and must not pretend to be their choice.
        next.userChosen = record.userChosen;
        if (!known || next != record) {
            ledger.setRecord(key, next);
            ledgerChanged = true;
        }
        if (announceNow) {
            Q_EMIT tabletAnnounced(
                key, group.deviceName,
                choice == TabletMapChoice::NamedOutput ? outputName
                                                       : QString());
        }
    }

    if (ledgerChanged && !m_store.save(ledger)) {
        fail(QStringLiteral("Could not record the tablet mapping decision"));
    }
}

bool TabletMappingPolicy::applyChoice(
    const GroupState &group, TabletMapChoice choice, const QString &outputName,
    const QList<TabletDeviceSnapshot> &devices, QString *error) {
    for (const QString &deviceId : group.toolDeviceIds) {
        const TabletDeviceSnapshot *device = nullptr;
        for (const TabletDeviceSnapshot &candidate : devices) {
            if (candidate.deviceId == deviceId) {
                device = &candidate;
                break;
            }
        }
        if (device == nullptr) {
            continue;
        }
        const QString currentOutput =
            device->properties.value(QStringLiteral("outputName")).toString();
        const bool currentWorkspace =
            device->properties.value(QStringLiteral("mapToWorkspace")).toBool();
        const QString wantedOutput =
            choice == TabletMapChoice::NamedOutput ? outputName : QString();
        const bool wantedWorkspace = choice == TabletMapChoice::EntireWorkspace;

        // Order matters: clearing the workspace flag first keeps KWin from
        // rejecting an output name while the tablet still spans everything.
        if (currentWorkspace != wantedWorkspace && !wantedWorkspace &&
            !m_port.writeProperty(deviceId, QStringLiteral("mapToWorkspace"),
                                  false, error)) {
            return false;
        }
        if (currentOutput != wantedOutput &&
            !m_port.writeProperty(deviceId, QStringLiteral("outputName"),
                                  wantedOutput, error)) {
            return false;
        }
        if (currentWorkspace != wantedWorkspace && wantedWorkspace &&
            !m_port.writeProperty(deviceId, QStringLiteral("mapToWorkspace"),
                                  true, error)) {
            return false;
        }
    }
    return true;
}

bool TabletMappingPolicy::applyUserChoice(const QString &deviceGroupId,
                                          TabletMapChoice choice,
                                          const QString &outputName,
                                          QString *error) {
    QString listError;
    const QList<TabletDeviceSnapshot> devices = m_port.devices(&listError);
    if (!listError.isEmpty()) {
        if (error != nullptr) {
            *error = listError;
        }
        return false;
    }
    GroupState group{deviceGroupId, {}, {}, false};
    for (const TabletDeviceSnapshot &device : devices) {
        if (groupKey(device) != deviceGroupId) {
            continue;
        }
        if (device.tabletTool) {
            group.toolDeviceIds.append(device.deviceId);
            group.deviceName = device.name;
        }
        group.hasPad = group.hasPad || device.tabletPad;
    }
    if (!applyChoice(group, choice, outputName, devices, error)) {
        return false;
    }
    if (!m_store.recordChoice(deviceGroupId, choice, outputName, true,
                              group.deviceName)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not record the tablet mapping "
                                    "choice");
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session::DesktopControls
