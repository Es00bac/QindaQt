// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

#include <qindaqt/apps/settings_input/shortcut_port.h>

namespace QindaQt::Apps::SettingsInput {

// Every global shortcut the authority reports, flattened into rows with a
// case-insensitive search filter over component and action names.
//
// AGENT-CONTRACT: Conflict truth is computed here from a full listing —
// a key conflicts when another action's active keys contain the exact same
// sequence — so a capture can always name the conflicting action without a
// second authority round trip (ADR-0134).
class ShortcutsModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    enum Roles {
        ComponentUniqueRole = Qt::UserRole + 1,
        ComponentNameRole,
        ActionUniqueRole,
        ActionNameRole,
        // Comma-joined native-text sequences; empty when disabled.
        KeysRole,
        DefaultKeysRole,
        IsCommandRole,
        CommandRole,
    };

    explicit ShortcutsModel(const ShortcutPort &port, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_visibleRows.size()); }
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] QString filter() const { return m_filter; }
    [[nodiscard]] QString statusText() const { return m_statusText; }

    void setFilter(const QString &filter);

    // Q_INVOKABLE: re-reads the authority; called when the tab becomes
    // visible, never during construction.
    Q_INVOKABLE void refresh();
    // Q_INVOKABLE: native text for one key chord in the kglobalaccel
    // encoding, so capture controls never re-implement key formatting.
    Q_INVOKABLE [[nodiscard]] QString displayKey(int key) const;
    // Q_INVOKABLE: display names of every action other than the one at
    // `row` whose active keys contain any sequence in `keys`, formatted
    // "<component> — <action>". Empty means the capture is conflict-free, so
    // re-capturing a row's own key is never a conflict; a row outside the
    // model excludes nothing.
    Q_INVOKABLE QStringList
    conflictsFor(int row, const QVariantList &keys) const;
    // Q_INVOKABLE: assigns `keys` (a list of ints in the kglobalaccel
    // encoding) to the row, then refreshes the affected truth.
    Q_INVOKABLE bool assign(int row, const QVariantList &keys);
    Q_INVOKABLE bool resetToDefault(int row);
    Q_INVOKABLE bool clear(int row);
    // Q_INVOKABLE: creates a command component and assigns keys; reports
    // the failure reason in statusText.
    Q_INVOKABLE bool addCommand(const QString &name, const QString &command,
                                const QVariantList &keys);
    Q_INVOKABLE bool removeCommand(int row);

Q_SIGNALS:
    void countChanged();
    void availableChanged();
    void busyChanged();
    void filterChanged();
    void statusTextChanged();

private:
    struct Row {
        ShortcutAction action;
    };
    [[nodiscard]] QList<QKeySequence>
    keySequences(const QVariantList &keys) const;
    void setBusy(bool busy);
    void rebuildVisibleRows();
    bool assignIdentity(const QString &componentUnique,
                        const QString &actionUnique,
                        const QList<QKeySequence> &keys);

    const ShortcutPort &m_port;
    QList<ShortcutAction> m_actions;
    QList<int> m_visibleRows;
    QString m_filter;
    QString m_statusText;
    bool m_available = true;
    bool m_busy = false;
};

} // namespace QindaQt::Apps::SettingsInput
