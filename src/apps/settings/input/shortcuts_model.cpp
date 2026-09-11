// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcuts_model.h>

#include <algorithm>

namespace QindaQt::Apps::SettingsInput {
namespace {

QString keysDisplay(const QList<QKeySequence> &sequences) {
    QStringList parts;
    parts.reserve(sequences.size());
    for (const QKeySequence &sequence : sequences) {
        const QString display = keySequenceDisplay(sequence);
        if (!display.isEmpty()) {
            parts.append(display);
        }
    }
    return parts.join(QStringLiteral(", "));
}

QString conflictName(const ShortcutAction &action) {
    return QStringLiteral("%1 — %2")
        .arg(action.componentFriendly.isEmpty() ? action.componentUnique
                                                : action.componentFriendly,
             action.actionFriendly.isEmpty() ? action.actionUnique
                                             : action.actionFriendly);
}

} // namespace

ShortcutsModel::ShortcutsModel(const ShortcutPort &port, QObject *parent)
    : QAbstractListModel(parent), m_port(port) {}

int ShortcutsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_visibleRows.size());
}

QVariant ShortcutsModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_visibleRows.size())) {
        return {};
    }
    const ShortcutAction &action = m_actions.at(m_visibleRows.at(index.row()));
    switch (role) {
    case ComponentUniqueRole:
        return action.componentUnique;
    case ComponentNameRole:
        return action.componentFriendly.isEmpty() ? action.componentUnique
                                                  : action.componentFriendly;
    case ActionUniqueRole:
        return action.actionUnique;
    case ActionNameRole:
        return action.actionFriendly.isEmpty() ? action.actionUnique
                                               : action.actionFriendly;
    case KeysRole:
        return keysDisplay(action.active);
    case DefaultKeysRole:
        return keysDisplay(action.defaults);
    case IsCommandRole:
        return action.isCommandComponent();
    case CommandRole:
        return action.command;
    default:
        return {};
    }
}

QHash<int, QByteArray> ShortcutsModel::roleNames() const {
    return {
        {ComponentUniqueRole, "componentUnique"},
        {ComponentNameRole, "componentName"},
        {ActionUniqueRole, "actionUnique"},
        {ActionNameRole, "actionName"},
        {KeysRole, "keys"},
        {DefaultKeysRole, "defaultKeys"},
        {IsCommandRole, "isCommand"},
        {CommandRole, "command"},
    };
}

void ShortcutsModel::setFilter(const QString &filter) {
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
    Q_EMIT filterChanged();
    Q_EMIT countChanged();
}

void ShortcutsModel::rebuildVisibleRows() {
    // Case-insensitive substring match across component and action names;
    // a user looks for "lock", not for the component id.
    m_visibleRows.clear();
    const QString needle = m_filter.trimmed();
    for (int row = 0; row < int(m_actions.size()); ++row) {
        const ShortcutAction &action = m_actions.at(row);
        const bool matches =
            needle.isEmpty() ||
            action.componentFriendly.contains(needle, Qt::CaseInsensitive) ||
            action.componentUnique.contains(needle, Qt::CaseInsensitive) ||
            action.actionFriendly.contains(needle, Qt::CaseInsensitive) ||
            action.actionUnique.contains(needle, Qt::CaseInsensitive);
        if (matches) {
            m_visibleRows.append(row);
        }
    }
}

void ShortcutsModel::refresh() {
    if (m_busy) {
        return;
    }
    setBusy(true);
    QString error;
    QList<ShortcutAction> actions = m_port.actions(&error);
    setBusy(false);
    beginResetModel();
    m_actions = actions;
    rebuildVisibleRows();
    m_available = error.isEmpty();
    m_statusText = error;
    endResetModel();
    Q_EMIT availableChanged();
    Q_EMIT countChanged();
    Q_EMIT statusTextChanged();
}

QString ShortcutsModel::displayKey(int key) const {
    if (key == 0) {
        return QString();
    }
    return keySequenceDisplay(QKeySequence(QKeyCombination::fromCombined(key)));
}

QStringList ShortcutsModel::conflictsFor(const QVariantList &keys) const {
    QStringList conflicts;
    for (const QVariant &keyValue : keys) {
        bool ok = false;
        const int key = keyValue.toInt(&ok);
        if (!ok || key == 0) {
            continue;
        }
        for (const ShortcutAction &action : m_actions) {
            const bool holdsKey = std::any_of(
                action.active.cbegin(), action.active.cend(),
                [key](const QKeySequence &sequence) {
                    return sequence.count() >= 1 &&
                           int(sequence[0].toCombined()) == key;
                });
            if (holdsKey && !conflicts.contains(conflictName(action))) {
                conflicts.append(conflictName(action));
            }
        }
    }
    return conflicts;
}

QList<QKeySequence>
ShortcutsModel::keySequences(const QVariantList &keys) const {
    QList<int> ints;
    for (const QVariant &keyValue : keys) {
        bool ok = false;
        const int key = keyValue.toInt(&ok);
        if (ok && key != 0) {
            ints.append(key);
        }
    }
    return shortcutKeysFromInts(ints);
}

void ShortcutsModel::setBusy(bool busy) {
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

bool ShortcutsModel::assignIdentity(const QString &componentUnique,
                                    const QString &actionUnique,
                                    const QList<QKeySequence> &keys) {
    QString error;
    const bool ok =
        m_port.setShortcuts(componentUnique, actionUnique, keys, &error);
    m_statusText = ok ? QString() : error;
    Q_EMIT statusTextChanged();
    if (ok) {
        refresh();
    }
    return ok;
}

bool ShortcutsModel::assign(int row, const QVariantList &keys) {
    if (row < 0 || row >= int(m_visibleRows.size())) {
        return false;
    }
    const ShortcutAction &action = m_actions.at(m_visibleRows.at(row));
    return assignIdentity(action.componentUnique, action.actionUnique,
                          keySequences(keys));
}

bool ShortcutsModel::resetToDefault(int row) {
    if (row < 0 || row >= int(m_visibleRows.size())) {
        return false;
    }
    const ShortcutAction &action = m_actions.at(m_visibleRows.at(row));
    return assignIdentity(action.componentUnique, action.actionUnique,
                          action.defaults);
}

bool ShortcutsModel::clear(int row) {
    if (row < 0 || row >= int(m_visibleRows.size())) {
        return false;
    }
    const ShortcutAction &action = m_actions.at(m_visibleRows.at(row));
    return assignIdentity(action.componentUnique, action.actionUnique, {});
}

bool ShortcutsModel::addCommand(const QString &name, const QString &command,
                                const QVariantList &keys) {
    setBusy(true);
    QString error;
    QString componentUnique;
    const bool ok = m_port.addCommandShortcut(name, command,
                                              keySequences(keys),
                                              &componentUnique, &error);
    setBusy(false);
    m_statusText = ok ? tr("Command shortcut added") : error;
    Q_EMIT statusTextChanged();
    if (ok) {
        refresh();
    }
    return ok;
}

bool ShortcutsModel::removeCommand(int row) {
    if (row < 0 || row >= int(m_visibleRows.size())) {
        return false;
    }
    setBusy(true);
    QString error;
    const bool ok = m_port.removeCommandShortcut(
        m_actions.at(m_visibleRows.at(row)).componentUnique, &error);
    setBusy(false);
    m_statusText = ok ? QString() : error;
    Q_EMIT statusTextChanged();
    if (ok) {
        refresh();
    }
    return ok;
}

} // namespace QindaQt::Apps::SettingsInput
