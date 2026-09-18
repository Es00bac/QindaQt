// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

#include <qindaqt/apps/settings_input/tablet_device_selection.h>

namespace QindaQt::Apps::SettingsInput {

// Rows of tablets reported by the input authority — one row per device
// group, so a pen and its pad are one tablet, not two devices — plus the
// selected tablet's presentation state.
//
// AGENT-CONTRACT: `available == false` means the authority was unreachable
// at the last refresh; the route must show its degraded notice, never an
// empty list pretending no tablet exists. An empty list WITH availability is
// the honest "no tablet connected" state.
class TabletDevicesModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QObject *selection READ selection CONSTANT)
    // The picker's selected index lives here, not in the combo:
    // QQuickComboBox does not self-select when a list model resets into
    // non-empty (same rule as the pointer route).
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY countChanged)

public:
    enum Roles {
        DeviceGroupIdRole = Qt::UserRole + 1,
        LabelRole,
        HasPadRole,
        MappedOutputRole,
    };

    TabletDevicesModel(
        const Services::TabletDevices::TabletDevicePort &port,
        const Services::TabletDevices::TabletOutputInventory &outputs,
        Services::TabletDevices::TabletMappingStore *store,
        QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_groups.size()); }
    [[nodiscard]] bool empty() const { return m_groups.isEmpty(); }
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool refreshing() const { return m_refreshing; }
    [[nodiscard]] int selectedIndex() const { return m_selectedRow; }
    [[nodiscard]] TabletDeviceSelection *selection() const {
        return m_selection;
    }

    // Selects the row the device picker moves to. An out-of-range row keeps
    // the current selection.
    Q_INVOKABLE void select(int row);
    // Selects by device group id, for the notification's deep link. Returns
    // false when no such tablet is connected, so the route can say so
    // instead of silently showing a different device.
    Q_INVOKABLE bool selectGroup(const QString &deviceGroupId);
    // Re-reads the authority. Called when the destination becomes visible,
    // never during construction (a hung authority must not slow the rest of
    // the Settings window down).
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void selectedIndexChanged();
    void availableChanged();
    void refreshingChanged();

private:
    struct Group {
        QString deviceGroupId;
        QString label;
        Services::TabletDevices::TabletDeviceSnapshot pen;
        Services::TabletDevices::TabletDeviceSnapshot pad;
        bool hasPen = false;
        bool hasPad = false;
    };

    void publishSelection();

    const Services::TabletDevices::TabletDevicePort &m_port;
    TabletDeviceSelection *m_selection;
    QList<Group> m_groups;
    int m_selectedRow = -1;
    bool m_available = true;
    bool m_refreshing = false;
};

} // namespace QindaQt::Apps::SettingsInput
