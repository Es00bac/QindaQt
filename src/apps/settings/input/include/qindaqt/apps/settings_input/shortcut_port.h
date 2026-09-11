// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QKeySequence>
#include <QList>
#include <QString>

#include <array>

namespace QindaQt::Apps::SettingsInput {

// Component ids this route creates for custom command shortcuts. Only these
// carry a command and are removable here; application components that
// kglobalaccel loads from installed .desktop files (Konsole, Dolphin, ...)
// are ordinary components.
inline constexpr char CommandComponentPrefix[] = "qindaqt-custom-";

// The kglobalaccel action that runs a desktop-file component's Exec line.
inline constexpr char LaunchActionName[] = "_launch";

// One global shortcut as the shortcut authority (kglobalaccel) reports it.
struct ShortcutAction {
    QString componentUnique;  // authority identity, e.g. "qindaqt-shell"
    QString componentFriendly;
    QString actionUnique;
    QString actionFriendly;
    QList<QKeySequence> active;   // empty means the shortcut is disabled
    QList<QKeySequence> defaults;
    QString command;              // non-empty only for command components

    [[nodiscard]] bool isCommandComponent() const {
        return componentUnique.startsWith(QLatin1String(CommandComponentPrefix)) &&
               componentUnique.endsWith(QLatin1String(".desktop"));
    }
};

// AGENT-NOTE: kglobalaccel's integer encoding is one int per chord
// (Qt::Modifier bits OR the Qt::Key). A captured global shortcut is one chord,
// so these helpers keep the first chord of each sequence.
[[nodiscard]] QList<int>
shortcutKeysToInts(const QList<QKeySequence> &sequences);
[[nodiscard]] QList<QKeySequence>
shortcutKeysFromInts(const QList<int> &keys);
[[nodiscard]] QString keySequenceDisplay(const QKeySequence &sequence);

// AGENT-CONTRACT: One key sequence on the kglobalaccel wire is the struct
// `(ai)` holding EXACTLY four ints, the four chords of a QKeySequence with
// unused chords zero. KF6GlobalAccel's demarshaller reads four ints without
// checking, and a shorter array aborts the compositor that hosts kglobalaccel,
// ending the desktop session (reproduced in a private KWin, ADR-0134).
// Sequences reach the wire only through this type.
struct ShortcutKeySequence {
    std::array<int, 4> chords{};
};

QDBusArgument &operator<<(QDBusArgument &argument,
                          const ShortcutKeySequence &sequence);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ShortcutKeySequence &sequence);

// Idempotent registration of ShortcutKeySequence and its list with the D-Bus
// type system; the adapter and the test fake call it before any call.
void registerShortcutDBusTypes();

// Port to the global shortcut authority (ADR-0134: kglobalaccel inside KWin).
class ShortcutPort {
public:
    virtual ~ShortcutPort();

    // Every component's actions with their active and default keys, read
    // through each component object's allShortcutInfos. An unreachable
    // authority or a reply with an unexpected D-Bus signature fails closed
    // with an empty list and `error` set. The order is the authority's.
    [[nodiscard]] virtual QList<ShortcutAction>
    actions(QString *error) const = 0;

    // Replaces the active keys of one existing action (empty clears; Reset
    // is the same call with the recorded defaults) and reads them back. The
    // authority silently ignores unknown actions and keeps a key another
    // action holds; both return false with `error` saying so.
    [[nodiscard]] virtual bool
    setShortcuts(const QString &componentUnique, const QString &actionUnique,
                 const QList<QKeySequence> &keys, QString *error) const = 0;

    // Installs a command component (a desktop file under
    // `<dataHome>/kglobalaccel` whose `_launch` action runs the command) and
    // assigns `keys`. Nothing is written while the authority is unreachable;
    // a failed registration or assignment removes the file and the
    // registration again. The new id comes back through `componentUnique`.
    [[nodiscard]] virtual bool
    addCommandShortcut(const QString &name, const QString &command,
                       const QList<QKeySequence> &keys,
                       QString *componentUnique, QString *error) const = 0;

    // Unregisters the `_launch` action of a command component this route
    // created and deletes its file. Every other component fails closed.
    [[nodiscard]] virtual bool
    removeCommandShortcut(const QString &componentUnique,
                          QString *error) const = 0;
};

// Production adapter over org.kde.kglobalaccel. `dataHome` is the
// composition root's XDG data home; command components live under
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

private:
    QDBusConnection m_bus;
    QString m_dataHome;
};

} // namespace QindaQt::Apps::SettingsInput
