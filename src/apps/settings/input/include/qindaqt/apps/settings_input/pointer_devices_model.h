// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

#include <qindaqt/apps/settings_input/pointer_device_port.h>
#include <qindaqt/apps/settings_input/pointer_device_selection.h>

namespace QindaQt::Apps::SettingsInput {

// Rows of pointer/touchpad devices reported by the input authority, plus
// the selected device's presentation state.
//
// AGENT-CONTRACT: `available == false` means the authority was unreachable
// at the last refresh — the route must show its degraded notice, never an
// empty list pretending no devices exist. An empty list WITH availability
// is the honest "no pointer or touchpad" state.
class PointerDevicesModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QObject *selection READ selection CONSTANT)
    // AGENT-CONTRACT: The picker row's selected index lives here, not in the
    // combo: QQuickComboBox does not self-select when a list model resets
    // into non-empty, so the route binds its picker to this property. It
    // changes whenever the selected row changes, including after refresh().
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY
                   selectedIndexChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY countChanged)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        LabelRole,
        IsTouchpadRole,
    };

    explicit PointerDevicesModel(const PointerDevicePort &port,
                                 QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role) const override;
    [[nodiscard]] QHash<int, QByteArray>
    roleNames() const override;

    [[nodiscard]] int count() const { return int(m_devices.size()); }
    [[nodiscard]] bool empty() const { return m_devices.isEmpty(); }
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool refreshing() const { return m_refreshing; }
    [[nodiscard]] int selectedIndex() const { return m_selectedRow; }
    [[nodiscard]] PointerDeviceSelection *selection() const {
        return m_selection;
    }

    // Q_INVOKABLE: selects the row the device picker moves to. An
    // out-of-range row keeps the current selection.
    Q_INVOKABLE void select(int row);
    // Q_INVOKABLE: re-reads the authority. Called when the tab becomes
    // visible, never during construction (a hung authority must not slow
    // the rest of the Settings window down).
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void selectedIndexChanged();
    void availableChanged();
    void refreshingChanged();

private:
    const PointerDevicePort &m_port;
    PointerDeviceSelection *m_selection;
    QList<PointerDeviceSnapshot> m_devices;
    int m_selectedRow = -1;
    bool m_available = true;
    bool m_refreshing = false;
};

} // namespace QindaQt::Apps::SettingsInput
