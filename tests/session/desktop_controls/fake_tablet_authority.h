// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QList>
#include <QString>
#include <QVariant>

#include <tuple>

// Shared by the tablet mapping and tablet orientation policy rows so both
// drive the policy through one fake authority.
namespace QindaQt::Tests::TabletPolicy {

using namespace QindaQt::Services::TabletDevices;

// In-memory tablet authority: the same contract as the KWin port, with every
// write recorded. No bus, no device, no compositor.
class FakeTabletPort final : public TabletDevicePort {
public:
    QList<TabletDeviceSnapshot> scripted;
    QString listError;
    mutable QList<std::tuple<QString, QString, QVariant>> writes;
    mutable QString refuse; // property name the authority rejects

    [[nodiscard]] QList<TabletDeviceSnapshot>
    devices(QString *error) const override {
        if (error != nullptr) {
            *error = listError;
        }
        return listError.isEmpty() ? scripted : QList<TabletDeviceSnapshot>{};
    }

    [[nodiscard]] bool device(const QString &deviceId,
                              TabletDeviceSnapshot *snapshot,
                              QString *error) const override {
        Q_UNUSED(error)
        for (const TabletDeviceSnapshot &candidate : scripted) {
            if (candidate.deviceId == deviceId) {
                *snapshot = candidate;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool writeProperty(const QString &deviceId,
                                     const QString &property,
                                     const QVariant &value,
                                     QString *error) const override {
        if (property == refuse) {
            if (error != nullptr) {
                *error = QStringLiteral("authority refused %1").arg(property);
            }
            return false;
        }
        writes.append({deviceId, property, value});
        // The authority's own state moves, so the next reconcile sees it.
        for (TabletDeviceSnapshot &candidate :
             const_cast<QList<TabletDeviceSnapshot> &>(scripted)) {
            if (candidate.deviceId == deviceId) {
                candidate.properties.insert(property, value);
            }
        }
        return true;
    }
};

class FakeWatcher final : public TabletDeviceWatcher {
public:
    bool started = false;
    [[nodiscard]] bool start(QString *error) override {
        Q_UNUSED(error)
        started = true;
        return true;
    }
    void plug(const QString &id) { Q_EMIT deviceAdded(id); }
    void unplug(const QString &id) { Q_EMIT deviceRemoved(id); }
};

class FakeOutputs final : public TabletOutputInventory {
public:
    QList<TabletOutputCandidate> scripted;
    [[nodiscard]] QList<TabletOutputCandidate> outputs() const override {
        return scripted;
    }
    void publish(QList<TabletOutputCandidate> next) {
        scripted = std::move(next);
        Q_EMIT outputsChanged();
    }
};

class FakeStore final : public TabletMappingStore {
public:
    TabletMappingLedger held;
    bool loaded = true;
    bool saveFails = false;
    int saves = 0;

    [[nodiscard]] bool isLoaded() const override { return loaded; }
    [[nodiscard]] TabletMappingLedger ledger() const override { return held; }
    bool save(const TabletMappingLedger &ledger) override {
        ++saves;
        if (saveFails) {
            return false;
        }
        held = ledger;
        return true;
    }
    void load(TabletMappingLedger ledger) {
        held = std::move(ledger);
        loaded = true;
        Q_EMIT ledgerChanged();
    }
};

} // namespace QindaQt::Tests::TabletPolicy
