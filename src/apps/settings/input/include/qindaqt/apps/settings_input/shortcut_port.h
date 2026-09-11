// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QKeySequence>
#include <QList>
#include <QString>

namespace QindaQt::Apps::SettingsInput {

// One global shortcut as the shortcut authority (kglobalaccel) reports it.
// Components from installed .desktop entries are command-launchable; their
// `command` holds the Exec value read back from the component file, and
// everything else leaves `command` empty.
struct ShortcutAction {
    QString componentUnique;  // authority identity, e.g. "qindaqt-shell"
    QString componentFriendly;
    QString actionUnique;
    QString actionFriendly;
    QList<QKeySequence> active;   // empty means the shortcut is disabled
    QList<QKeySequence> defaults;
    QString command;              // non-empty only for .desktop components

    [[nodiscard]] bool isCommandComponent() const noexcept {
        return componentUnique.endsWith(QStringLiteral(".desktop"));
    }
};

// AGENT-NOTE: Sequence helpers keep the kglobalaccel D-Bus encoding (one
// int per sequence: Qt::Modifier bits OR the Qt::Key) at the port boundary
// so models and QML never touch it. A global shortcut sequence is one key;
// QKeySequence with more than one key is reduced to its first key.
[[nodiscard]] QList<int>
shortcutKeysToInts(const QList<QKeySequence> &sequences);
[[nodiscard]] QList<QKeySequence>
shortcutKeysFromInts(const QList<int> &keys);
[[nodiscard]] QString keySequenceDisplay(const QKeySequence &sequence);

// AGENT-CONTRACT: The kglobalaccel wire types for one shortcut sequence —
// signature `(ai)`, a struct holding one array of key ints — and for one
// `allShortcutInfos` row. They live here (not in the adapter) because the
// authority-side test fakes must declare the exact same C++ types: Qt D-Bus
// matches incoming calls per registered type, and two types claiming one
// signature in one process break the dispatch.
struct ShortcutKeySequence {
    QList<int> keys;
};

struct ShortcutInfoRow {
    QString actionUnique;
    QString actionFriendly;
    QString componentUnique;
    QString componentFriendly;
    QString contextUnique;
    QString contextFriendly;
    ShortcutKeySequence keys;
    ShortcutKeySequence defaults;
};

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const ShortcutKeySequence &sequence) {
    argument.beginStructure();
    argument.beginArray(QMetaType::Int);
    for (const int key : sequence.keys) {
        argument << key;
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       ShortcutKeySequence &sequence) {
    sequence.keys.clear();
    argument.beginStructure();
    argument.beginArray();
    while (!argument.atEnd()) {
        int key = 0;
        argument >> key;
        sequence.keys.append(key);
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const ShortcutInfoRow &row) {
    argument.beginStructure();
    argument << row.actionUnique << row.actionFriendly << row.componentUnique
             << row.componentFriendly << row.contextUnique
             << row.contextFriendly << row.keys << row.defaults;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       ShortcutInfoRow &row) {
    argument.beginStructure();
    argument >> row.actionUnique >> row.actionFriendly >> row.componentUnique
        >> row.componentFriendly >> row.contextUnique >> row.contextFriendly
        >> row.keys >> row.defaults;
    argument.endStructure();
    return argument;
}

// Idempotent registration of the wire types with the D-Bus meta-type
// system; the adapter and the test fakes both call it before exporting.
void registerShortcutDBusTypes();

// Port to the global shortcut authority (ADR-0134: kglobalaccel inside KWin;
// D-Bus because the settings process registers no shortcuts of its own and
// the enumeration/foreign-mutation surface is the D-Bus contract).
class ShortcutPort {
public:
    virtual ~ShortcutPort();

    // Every component's actions with their active and default keys. An
    // unreachable authority or a malformed reply fails closed with an
    // empty list and `error` set. The list order is the authority's order.
    [[nodiscard]] virtual QList<ShortcutAction>
    actions(QString *error) const = 0;

    // Replaces the active keys of one existing action. An empty list
    // disables (clear); the default keys reset is the same call with the
    // action's recorded defaults. Unknown actions fail closed.
    [[nodiscard]] virtual bool
    setShortcuts(const QString &componentUnique, const QString &actionUnique,
                 const QList<QKeySequence> &keys, QString *error) const = 0;

    // Installs a launchable command component under the user's shortcut
    // data directory and assigns `keys` to it. The derived component id is
    // reported through `componentUnique`; the caller must re-list to see it.
    [[nodiscard]] virtual bool
    addCommandShortcut(const QString &name, const QString &command,
                       const QList<QKeySequence> &keys,
                       QString *componentUnique, QString *error) const = 0;

    // Removes a command component previously added by
    // addCommandShortcut (its file and its registration). Non-command
    // components fail closed.
    [[nodiscard]] virtual bool
    removeCommandShortcut(const QString &componentUnique,
                          QString *error) const = 0;
};

// Production adapter over org.kde.KGlobalAccel (kglobalaccel). `dataHome`
// is the composition root's XDG data home; command components live under
// `<dataHome>/kglobalaccel/<id>.desktop` (ADR-0134).
class QtShortcutPort final : public ShortcutPort {
public:
    QtShortcutPort(QDBusConnection bus, QString dataHome);

    [[nodiscard]] QList<ShortcutAction>
    actions(QString *error) const override;
    [[nodiscard]] bool
    setShortcuts(const QString &componentUnique, const QString &actionUnique,
                 const QList<QKeySequence> &keys, QString *error) const override;
    [[nodiscard]] bool
    addCommandShortcut(const QString &name, const QString &command,
                       const QList<QKeySequence> &keys,
                       QString *componentUnique, QString *error) const override;
    [[nodiscard]] bool
    removeCommandShortcut(const QString &componentUnique,
                          QString *error) const override;

    // Built-in-type key accessors used while listing (a(ai) replies).
    [[nodiscard]] QList<QKeySequence>
    shortcutKeys(const QString &componentUnique,
                 const QString &actionUnique) const;
    [[nodiscard]] QList<QKeySequence>
    defaultShortcutKeys(const QString &componentUnique,
                        const QString &actionUnique) const;

    QDBusConnection m_bus;
    QString m_dataHome;
};

} // namespace QindaQt::Apps::SettingsInput
