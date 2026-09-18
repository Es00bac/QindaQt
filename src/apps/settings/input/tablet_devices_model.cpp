// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_devices_model.h>

namespace QindaQt::Apps::SettingsInput {

using Services::TabletDevices::TabletDeviceSnapshot;

namespace {

// AGENT-GUARD: the row key is the STABLE identity, never KWin's
// `deviceGroupId` (a hash of a pointer address). The route and the session
// policy must agree on what a tablet is, and the deep link from the
// notification carries this identity.
QString groupKey(const TabletDeviceSnapshot &device) {
    return Services::TabletDevices::tabletIdentity(device);
}

// "Wacom One Pen Display 13 Pen" is the pen's name; the tablet is what the
// user recognizes, so the trailing role word is dropped for the row label —
// the same rule the stable identity uses, from one implementation.
QString groupLabel(const TabletDeviceSnapshot &device) {
    return Services::TabletDevices::tabletBaseName(device.name);
}

} // namespace

TabletDevicesModel::TabletDevicesModel(
    const Services::TabletDevices::TabletDevicePort &port,
    const Services::TabletDevices::TabletOutputInventory &outputs,
    Services::TabletDevices::TabletMappingStore *store, QObject *parent)
    : QAbstractListModel(parent), m_port(port),
      m_selection(new TabletDeviceSelection(port, outputs, store, this)) {
    connect(&outputs,
            &Services::TabletDevices::TabletOutputInventory::outputsChanged,
            m_selection, [this] { m_selection->refreshOutputs(); });
}

int TabletDevicesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_groups.size());
}

QVariant TabletDevicesModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_groups.size())) {
        return {};
    }
    const Group &group = m_groups.at(index.row());
    switch (role) {
    case DeviceGroupIdRole:
        return group.deviceGroupId;
    case LabelRole:
        return group.label;
    case HasPadRole:
        return group.hasPad;
    case MappedOutputRole:
        return group.hasPen
                   ? group.pen.properties.value(QStringLiteral("outputName"))
                         .toString()
                   : QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> TabletDevicesModel::roleNames() const {
    return {
        {DeviceGroupIdRole, "deviceGroupId"},
        {LabelRole, "label"},
        {HasPadRole, "hasPad"},
        {MappedOutputRole, "mappedOutput"},
    };
}

void TabletDevicesModel::publishSelection() {
    if (m_selectedRow < 0 || m_selectedRow >= int(m_groups.size())) {
        m_selection->clear();
        return;
    }
    const Group &group = m_groups.at(m_selectedRow);
    m_selection->setGroup(group.pen, group.hasPen, group.pad, group.hasPad);
}

void TabletDevicesModel::select(int row) {
    if (row < 0 || row >= int(m_groups.size()) || row == m_selectedRow) {
        return;
    }
    m_selectedRow = row;
    Q_EMIT selectedIndexChanged();
    publishSelection();
}

bool TabletDevicesModel::selectGroup(const QString &deviceGroupId) {
    if (deviceGroupId.isEmpty()) {
        return false;
    }
    for (int row = 0; row < int(m_groups.size()); ++row) {
        if (m_groups.at(row).deviceGroupId != deviceGroupId) {
            continue;
        }
        if (row != m_selectedRow) {
            m_selectedRow = row;
            Q_EMIT selectedIndexChanged();
            publishSelection();
        }
        return true;
    }
    return false;
}

void TabletDevicesModel::refresh() {
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;
    Q_EMIT refreshingChanged();
    QString error;
    const QList<TabletDeviceSnapshot> devices = m_port.devices(&error);
    const bool authorityAnswered = error.isEmpty();

    QList<Group> groups;
    for (const TabletDeviceSnapshot &device : devices) {
        const QString key = groupKey(device);
        if (key.isEmpty()) {
            continue;
        }
        int existing = -1;
        for (int row = 0; row < int(groups.size()); ++row) {
            if (groups.at(row).deviceGroupId == key) {
                existing = row;
                break;
            }
        }
        if (existing < 0) {
            groups.append(Group{key, groupLabel(device), {}, {}, false, false});
            existing = int(groups.size()) - 1;
        }
        Group &group = groups[existing];
        if (device.tabletTool && !group.hasPen) {
            group.pen = device;
            group.hasPen = true;
            // The tool names the tablet; a pad-only label is the one users
            // do not recognize.
            group.label = groupLabel(device);
        } else if (device.tabletPad && !group.hasPad) {
            group.pad = device;
            group.hasPad = true;
        }
    }

    const QString previousGroup =
        m_selectedRow >= 0 && m_selectedRow < int(m_groups.size())
            ? m_groups.at(m_selectedRow).deviceGroupId
            : QString();
    const int previousSelectedRow = m_selectedRow;

    beginResetModel();
    m_groups = groups;
    m_available = authorityAnswered;
    // Keeping the same tablet selected across a refresh matters: a re-plug
    // reorders the authority's list, and the user's controls must not jump
    // to another device under their hand.
    m_selectedRow = -1;
    if (!previousGroup.isEmpty()) {
        for (int row = 0; row < int(m_groups.size()); ++row) {
            if (m_groups.at(row).deviceGroupId == previousGroup) {
                m_selectedRow = row;
                break;
            }
        }
    }
    if (m_selectedRow < 0 && !m_groups.isEmpty()) {
        m_selectedRow = 0;
    }
    endResetModel();

    m_refreshing = false;
    Q_EMIT refreshingChanged();
    Q_EMIT availableChanged();
    Q_EMIT countChanged();
    publishSelection();
    if (m_selectedRow != previousSelectedRow) {
        Q_EMIT selectedIndexChanged();
    }
}

} // namespace QindaQt::Apps::SettingsInput
