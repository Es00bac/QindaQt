// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_devices_model.h>

namespace QindaQt::Apps::SettingsInput {

PointerDevicesModel::PointerDevicesModel(PointerDevicePort &port,
                                         QObject *parent)
    : QAbstractListModel(parent), m_port(port),
      m_selection(new PointerDeviceSelection(port, this)) {
    m_poll.setInterval(3000);
    connect(&m_poll, &QTimer::timeout, this, &PointerDevicesModel::refresh);
    connect(&port, &PointerDevicePort::inventoryChanged, this,
            [this] { if (m_active) refresh(); });
    connect(&port, &PointerDevicePort::authorityChanged, this, [this] {
        // AGENT-GUARD: An old-owner reply may complete after a replacement.
        // Clear the selection immediately and fence every pending completion.
        ++m_generation;
        m_refreshRequested = false;
        m_selection->setSnapshot({});
        const int oldRow = m_selectedRow;
        beginResetModel();
        m_devices.clear();
        m_selectedId.clear();
        m_selectedRow = -1;
        endResetModel();
        if (m_available) {
            m_available = false;
            Q_EMIT availableChanged();
        }
        Q_EMIT countChanged();
        if (oldRow != -1) Q_EMIT selectedIndexChanged();
        m_refreshing = false;
        Q_EMIT refreshingChanged();
        if (m_active) refresh();
    });
    connect(m_selection, &PointerDeviceSelection::busyChanged, this,
            [this] {
        if (m_active && !m_selection->busy() && m_refreshRequested &&
            !m_refreshing) {
            m_refreshRequested = false;
            refresh();
        }
    });
    connect(m_selection, &PointerDeviceSelection::refreshRequested, this,
            [this] { if (m_active) refresh(); });
    connect(m_selection, &PointerDeviceSelection::confirmedSnapshot, this,
            [this](const PointerDeviceSnapshot &snapshot) {
        for (int row = 0; row < m_devices.size(); ++row) {
            if (m_devices.at(row).deviceId != snapshot.deviceId) continue;
            m_devices[row] = snapshot;
            Q_EMIT dataChanged(index(row), index(row));
            break;
        }
    });
}

int PointerDevicesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_devices.size());
}

QVariant PointerDevicesModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_devices.size())) return {};
    const PointerDeviceSnapshot &device = m_devices.at(index.row());
    switch (role) {
    case DeviceIdRole: return device.deviceId;
    case LabelRole: return device.name;
    case IsTouchpadRole: return device.touchpad;
    default: return {};
    }
}

QHash<int, QByteArray> PointerDevicesModel::roleNames() const {
    return {{DeviceIdRole, "deviceId"}, {LabelRole, "label"},
            {IsTouchpadRole, "isTouchpad"}};
}

void PointerDevicesModel::select(int row) {
    if (row < 0 || row >= int(m_devices.size())) return;
    const auto &device = m_devices.at(row);
    if (row == m_selectedRow && device.deviceId == m_selectedId) return;
    m_selectedRow = row;
    m_selectedId = device.deviceId;
    Q_EMIT selectedIndexChanged();
    m_selection->setSnapshot(device);
}

void PointerDevicesModel::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    Q_EMIT activeChanged();
    if (active) {
        m_port.setObserving(true);
        m_poll.start();
        refresh();
    } else {
        m_poll.stop();
        m_port.setObserving(false);
    }
}

void PointerDevicesModel::refresh() {
    if (m_refreshing || m_selection->busy()) {
        m_refreshRequested = true;
        return;
    }
    m_refreshing = true;
    Q_EMIT refreshingChanged();
    const quint64 generation = ++m_generation;
    m_port.requestDevices(this, [this, generation](
                              QList<PointerDeviceSnapshot> devices,
                              QString error) {
        if (generation != m_generation) return;
        const int oldRow = m_selectedRow;
        const bool oldAvailable = m_available;
        // AGENT-CONTRACT: An error means degraded, not a trustworthy empty
        // inventory. Device identity survives reorder but never owner loss.
        if (!error.isEmpty()) devices.clear();
        int selected = -1;
        for (int row = 0; row < devices.size(); ++row) {
            if (devices.at(row).deviceId == m_selectedId) {
                selected = row;
                break;
            }
        }
        if (selected < 0 && !devices.isEmpty()) selected = 0;
        beginResetModel();
        m_devices = std::move(devices);
        m_selectedRow = selected;
        m_selectedId = selected < 0 ? QString()
                                    : m_devices.at(selected).deviceId;
        m_available = error.isEmpty();
        endResetModel();
        if (selected < 0) m_selection->setSnapshot({});
        else m_selection->setSnapshot(m_devices.at(selected));
        if (oldAvailable != m_available) Q_EMIT availableChanged();
        Q_EMIT countChanged();
        if (oldRow != selected) Q_EMIT selectedIndexChanged();
        m_refreshing = false;
        Q_EMIT refreshingChanged();
        if (m_refreshRequested) {
            m_refreshRequested = false;
            if (m_active) refresh();
        }
    });
}

} // namespace QindaQt::Apps::SettingsInput
