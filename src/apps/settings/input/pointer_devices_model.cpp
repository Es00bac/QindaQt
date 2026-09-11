// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_devices_model.h>

namespace QindaQt::Apps::SettingsInput {

PointerDevicesModel::PointerDevicesModel(const PointerDevicePort &port,
                                         QObject *parent)
    : QAbstractListModel(parent), m_port(port),
      m_selection(new PointerDeviceSelection(port, this)) {}

int PointerDevicesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_devices.size());
}

QVariant PointerDevicesModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_devices.size())) {
        return {};
    }
    const PointerDeviceSnapshot &device = m_devices.at(index.row());
    switch (role) {
    case DeviceIdRole:
        return device.deviceId;
    case LabelRole:
        return device.name;
    case IsTouchpadRole:
        return device.touchpad;
    default:
        return {};
    }
}

QHash<int, QByteArray> PointerDevicesModel::roleNames() const {
    return {
        {DeviceIdRole, "deviceId"},
        {LabelRole, "label"},
        {IsTouchpadRole, "isTouchpad"},
    };
}

void PointerDevicesModel::select(int row) {
    if (row < 0 || row >= int(m_devices.size()) || row == m_selectedRow) {
        return;
    }
    m_selectedRow = row;
    m_selection->setSnapshot(m_devices.at(row));
}

void PointerDevicesModel::refresh() {
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;
    Q_EMIT refreshingChanged();
    QString error;
    const QList<PointerDeviceSnapshot> devices = m_port.devices(&error);
    const bool authorityAnswered = error.isEmpty();
    beginResetModel();
    m_devices = devices;
    m_available = authorityAnswered;
    if (m_selectedRow >= int(m_devices.size()) ||
        (m_selectedRow < 0 && !m_devices.isEmpty())) {
        m_selectedRow = m_devices.isEmpty() ? -1 : 0;
    }
    const int selectedRow = m_selectedRow;
    endResetModel();
    m_refreshing = false;
    Q_EMIT refreshingChanged();
    Q_EMIT availableChanged();
    Q_EMIT countChanged();
    if (selectedRow >= 0 && selectedRow < int(m_devices.size())) {
        m_selection->setSnapshot(m_devices.at(selectedRow));
    } else {
        m_selectedRow = -1;
    }
}

} // namespace QindaQt::Apps::SettingsInput
